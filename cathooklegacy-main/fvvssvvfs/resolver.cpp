#include "includes.h"
Resolver g_resolver{};


LagRecord* Resolver::FindIdealRecord(AimPlayer* data)
{
	LagRecord* first_valid, * current;

	if (data->m_records.empty())
		return nullptr;

	first_valid = nullptr;

	// iterate records.
	for (const auto& it : data->m_records) {
		if (it->dormant() || it->immune() || !it->valid())
			continue;

		// get current record.
		current = it.get();

		// first record that was valid, store it for later.
		if (!first_valid)
			first_valid = current;

		// try to find a record with onshot, lby update, walking or no anti-aim.
		if (it->m_shot || it->m_mode == Modes::RESOLVE_BODY || it->m_mode == Modes::RESOLVE_WALK || it->m_mode == Modes::RESOLVE_NONE)
			return current;
	}

	// none found above, return the first valid record if possible.
	return (first_valid) ? first_valid : nullptr;
}

LagRecord* Resolver::FindLastRecord(AimPlayer* data)
{
	LagRecord* current;
	if (data->m_records.empty())
		return nullptr;

	// iterate records in reverse.
	for (auto it = data->m_records.crbegin(); it != data->m_records.crend(); ++it) {
		current = it->get();

		// if this record is valid.
		// we are done since we iterated in reverse.
		if (current->valid() && !current->immune() && !current->dormant())
			return current;
	}

	return nullptr;
}

void Resolver::OnBodyUpdate(Player* player, float value)
{
	AimPlayer* data = &g_aimbot.m_players[player->index() - 1];

	// set data.
	data->m_old_body = data->m_body;
	data->m_body = value;
}

float Resolver::GetAwayAngle(LagRecord* record)
{
	float  delta{ std::numeric_limits< float >::max() };
	vec3_t pos;
	ang_t  away;

	if (g_cl.m_net_pos.empty()) {
		math::VectorAngles(g_cl.m_local->m_vecOrigin() - record->m_pred_origin, away);
		return away.y;
	}

	float target = record->m_pred_time;

	// iterate all.
	for (const auto& net : g_cl.m_net_pos) {
		float dt = std::abs(target - net.m_time);

		// the best origin.
		if (dt < delta) {
			delta = dt;
			pos = net.m_pos;
		}
	}

	math::VectorAngles(pos - record->m_pred_origin, away);
	return away.y;
}

void Resolver::SetMode(LagRecord* record)
{
	float speed = record->m_velocity.length_2d();

	// if on ground, moving, not fake flicking or fake walking.
	if ((record->m_flags & FL_ONGROUND) && speed > 0.1f && !record->m_fake_walk && !record->m_fake_flick)
		record->m_mode = Modes::RESOLVE_WALK;

	// if on ground, not moving, fakewalking, fake flicking.
	if ((record->m_flags & FL_ONGROUND) && (speed < 0.1f || record->m_fake_walk || record->m_fake_flick) && !g_input.GetKeyState(g_menu.main.aimbot.override.get()))
	{
		record->m_eye_angles.x = 89.f;
		record->m_mode = Modes::RESOLVE_STAND;
	}
	// override
	if (g_input.GetKeyState(g_menu.main.aimbot.override.get()) && record->m_flags & FL_ONGROUND && (speed < 0.1f || record->m_fake_walk))
		record->m_mode = Modes::RESOLVE_OVERRIDE;
	// if not on ground.
	else if (!(record->m_flags & FL_ONGROUND))
		record->m_mode = Modes::RESOLVE_AIR;
}

void Resolver::MatchShot(AimPlayer* data, LagRecord* record)
{
	float shoot_time = -1.f;

	Weapon* weapon = data->m_player->GetActiveWeapon();
	if (weapon) {
		shoot_time = weapon->m_fLastShotTime() - g_csgo.m_globals->m_interval;
	}

	// this record has a shot on it.
	if (game::TIME_TO_TICKS(shoot_time) == game::TIME_TO_TICKS(record->m_sim_time)) {
		if (record->m_lag <= 2)
			record->m_shot = true;

		// more then 1 choke, cant hit pitch, apply prev pitch.
		else if (data->m_records.size() >= 2) {
			LagRecord* previous = data->m_records[1].get();

			if (previous && !previous->dormant())
				record->m_eye_angles.x = previous->m_eye_angles.x;
		}
	}
}

int last_ticks[65];
int GetChokedPackets(Player* target)
{
	auto ticks = game::TIME_TO_TICKS(target->m_flSimulationTime() - target->m_flOldSimulationTime());
	if (ticks == 0 && last_ticks[target->index()] > 0) {
		return last_ticks[target->index()] - 1;
	}
	else {
		last_ticks[target->index()] = ticks;
		return ticks;
	}
}

void Resolver::ResolveAngles(Player* player, LagRecord* record)
{
	// lets clean up the specator view a bit
	if (!g_cl.m_processing)
	{
		record->m_eye_angles.y = record->m_body;
		return;
	}

	AimPlayer* data = &g_aimbot.m_players[player->index() - 1];
	bool fake = GetChokedPackets(record->m_player) > 1;

	// next up mark this record with a resolver mode that will be used.
	SetMode(record);

	if (record->m_mode == Modes::RESOLVE_WALK && fake)
		ResolveWalk(data, record);

	else if (record->m_mode == Modes::RESOLVE_STAND && fake && !(g_input.GetKeyState(g_menu.main.aimbot.override.get())))
		ResolveStand(player, data, record);

	else if (record->m_mode == Modes::RESOLVE_OVERRIDE || (g_input.GetKeyState(g_menu.main.aimbot.override.get())))
		ResolveOverride(player, record, data);

	else if (record->m_mode == Modes::RESOLVE_AIR && fake)
		ResolveAir(player, data, record);

	else if (!fake)// if no fake yaw detected.
	{
		record->m_mode = Modes::RESOLVE_NONE;
		//record->m_resolver_mode = XOR("logic");
		//record->m_resolver_mode2 = XOR("LOGIC");
	}

	// normalize the eye angles, doesn't really matter but its clean.
	math::NormalizeAngle(record->m_eye_angles.y);
}

void Resolver::ResolveOverride(Player* player, LagRecord* record, AimPlayer* data)
{
	// get predicted away angle for the player.
	float away = GetAwayAngle(record);

	// pointer for easy access.
	LagRecord* move = &data->m_walk_record;
	float delta = record->m_anim_time - move->m_anim_time;

	C_AnimationLayer* curr = &record->m_layers[3];
	int act = data->m_player->GetSequenceActivity(curr->m_sequence);

	if (g_input.GetKeyState(g_menu.main.aimbot.override.get())) {
		ang_t viewangles;
		g_csgo.m_engine->GetViewAngles(viewangles);

		//auto yaw = math::clamp (g_cl.m_local->GetAbsOrigin(), Player->origin()).y;
		const float at_target_yaw = math::CalcAngle(g_cl.m_local->m_vecOrigin(), player->m_vecOrigin()).y;

		if (fabs(math::NormalizedAngle(viewangles.y - at_target_yaw)) > 30.f)
			return ResolveStand(player, data, record);

		record->m_eye_angles.y = (math::NormalizedAngle(viewangles.y - at_target_yaw) > 0) ? at_target_yaw + 90.f : at_target_yaw - 90.f;

		record->m_mode = Modes::RESOLVE_OVERRIDE;
	//	record->m_resolver_mode = XOR("override");
	//	record->m_resolver_mode2 = XOR("OVERRIDE");
	}
	if (move->m_sim_time > 0.f && !move->dormant() && !record->dormant()) {
		vec3_t delta = move->m_origin - record->m_origin;

		// check if moving record is close.
		if (delta.length() <= 128.f) {
			// indicate that we are using the moving lby.
			data->m_moved = true;
		}
	}
	if (delta < 0.22f && delta > 0.01f && !record->m_fake_flick)
	{
		record->m_mode = Modes::RESOLVE_STOPPED_MOVING;
	//	record->m_resolver_mode = XOR("stopped");
	//	record->m_resolver_mode2 = XOR("STOPPED");
		record->m_eye_angles.y = move->m_body;
	}
	else if (record->m_anim_time >= data->m_body_update && data->m_body != data->m_old_body && data->m_body_index < 1)
	{
		record->m_mode = Modes::RESOLVE_BODY;
	//	record->m_resolver_mode = XOR("flick");
	//	record->m_resolver_mode2 = XOR("FLICK");
		record->m_eye_angles.y = data->m_body;
		data->m_body_update = record->m_anim_time + g_csgo.m_globals->m_interval + 1.1;
		iPlayers[record->m_player->index()] = false;
	}
}

bool Resolver::ShouldUseFreestand(LagRecord* record) // allows freestanding if not in open
{
	/* externs */
	vec3_t src3D, dst3D, forward, right, up, src, dst;
	float back_two, right_two, left_two;
	CGameTrace tr;
	CTraceFilterSimple filter;

	/* angle vectors */
	math::AngleVectors(ang_t(0, GetAwayAngle(record), 0), &forward, &right, &up);

	/* filtering */
	filter.SetPassEntity(record->m_player);
	src3D = record->m_player->GetShootPosition();
	dst3D = src3D + (forward * 384);

	/* back engine tracers */
	g_csgo.m_engine_trace->TraceRay(Ray(src3D, dst3D), MASK_SHOT_BRUSHONLY | MASK_OPAQUE_AND_NPCS, &filter, &tr);
	back_two = (tr.m_endpos - tr.m_startpos).length();

	/* right engine tracers */
	g_csgo.m_engine_trace->TraceRay(Ray(src3D + right * 35, dst3D + right * 35), MASK_SHOT_BRUSHONLY | MASK_OPAQUE_AND_NPCS, &filter, &tr);
	right_two = (tr.m_endpos - tr.m_startpos).length();

	/* left engine tracers */
	g_csgo.m_engine_trace->TraceRay(Ray(src3D - right * 35, dst3D - right * 35), MASK_SHOT_BRUSHONLY | MASK_OPAQUE_AND_NPCS, &filter, &tr);
	left_two = (tr.m_endpos - tr.m_startpos).length();

	/* fix side */
	if (left_two > right_two) {
		bFacingleft = true;
		bFacingright = false;
		return true;
	}
	else if (right_two > left_two) {
		bFacingright = true;
		bFacingleft = false;
		return true;
	}
	else
		return false;

	bool fake = GetChokedPackets(record->m_player) > 1;
	if (!fake)
		return false;
}

void Resolver::Freestand(Player* player, AimPlayer* data, LagRecord* record)
{
	// constants
	constexpr float STEP{ 4.f };
	constexpr float RANGE{ 32.f };

	// best target.
	vec3_t enemypos = record->m_player->GetShootPosition();
	float away = GetAwayAngle(record);
	// construct vector of angles to test.

	C_AnimationLayer* curr = &record->m_layers[3];
	int act = data->m_player->GetSequenceActivity(curr->m_sequence);
	float lby = record->m_body;

	record->m_mode = Modes::RESOLVE_FREESTAND;
	//record->m_resolver_mode = XOR("freestand");
	//record->m_resolver_mode2 = XOR("FREESTAND");

	std::vector< AdaptiveAngle > angles{ };
	angles.emplace_back(away + 190.f);
	angles.emplace_back(away + 100.f);
	angles.emplace_back(away - 80.f);

	// start the trace at the your shoot pos.
	vec3_t start = g_cl.m_local->GetShootPosition();

	// see if we got any valid result.
	// if this is false the path was not obstructed with anything.
	bool valid{ false };
	// iterate vector of angles.

	for (auto it = angles.begin(); it != angles.end(); ++it) {
		// compute the 'rough' estimation of where our head will be.
		vec3_t end{ enemypos.x + std::cos(math::deg_to_rad(it->m_yaw)) * RANGE,
			enemypos.y + std::sin(math::deg_to_rad(it->m_yaw)) * RANGE,
			enemypos.z };

		// draw a line for debugging purposes.
		//g_csgo.m_debug_overlay->AddLineOverlay( start, end, 255, 0, 0, true, 0.1f );

		// compute the direction.
		vec3_t dir = end - start;
		float len = dir.normalize();

		// should never happen.

		if (len <= 0.f)

			continue;

		// step thru the total distance, 4 units per step.
		for (float i{ 0.f }; i < len; i += STEP) {
			// get the current step position.
			vec3_t point = start + (dir * i);

			// get the contents at this point.
			int contents = g_csgo.m_engine_trace->GetPointContents(point, MASK_SHOT_HULL);

			// contains nothing that can stop a bullet.
			if (!(contents & MASK_SHOT_HULL))
				continue;

			float mult = 1.f;

			if (i < (len * 0.05f)) { // was originally .45 when i pasted this, but .05 works so much better.
				mult = 1.25f;
				record->m_eye_angles.y = away + 190.f; // put head back if most of their body is out.
				return;

			}

			// over 65% of the total length.
			if (i > (len * 0.65f))

				mult = 1.25f;

			// over 75% of the total length.
			if (i > (len * 0.75f))

				mult = 1.5f;

			// over 90% of the total length.
			if (i > (len * 0.9f))
				mult = 2.f;

			// append 'penetrated distance'.
			it->m_dist += (STEP * mult);

			// mark that we found anything.
			valid = true;

		}
	}

	if (!valid) {
		record->m_eye_angles.y = away + 190.f;
		return;

	}

	// put the most distance at the front of the container.

	std::sort(angles.begin(), angles.end(),

		[](const AdaptiveAngle& a, const AdaptiveAngle& b) {

			return a.m_dist > b.m_dist;

		});


	// the best angle should be at the front now.
	AdaptiveAngle* best = &angles.front();

	// fix trying to override lby limits
	if (act != 979)
		math::clamp(best->m_yaw, lby - 105, lby + 105);

	if (data->m_freestand_index < 1)
		record->m_eye_angles.y = best->m_yaw;
	else if (bFacingright && data->m_freestand_index < 2)
		record->m_eye_angles.y = best->m_yaw + 90.f;
	else if (bFacingleft && data->m_freestand_index < 2)
		record->m_eye_angles.y = best->m_yaw - 90.f;
	else
		record->m_eye_angles.y = best->m_yaw;
	return;
}

bool Resolver::Spin_Detection(AimPlayer* data)
{
	if (data->m_records.empty())
		return false;

	spin_step = 0;

	size_t size{};

	// iterate records.
	for (const auto& it : data->m_records) {
		if (it->dormant())
			break;

		// increment total amount of data.
		++size;
	}

	if (size > 2) {
		LagRecord* record = data->m_records[0].get();

		spindelta = (record->m_body - data->m_records[1].get()->m_body) / data->m_records[1].get()->m_lag;
		spinbody = record->m_body;
		float delta2 = (data->m_records[1].get()->m_body - data->m_records[2].get()->m_body) / data->m_records[2].get()->m_lag;

		return spindelta == delta2 && spindelta > 0.5f;
	}
	else
		return false;
}

void Resolver::ResolveStand(Player* player, AimPlayer* data, LagRecord* record)
{
	float away = GetAwayAngle(record);
	float backwards = away + 190.f;
	LagRecord* move = &data->m_walk_record;
	LagRecord* previous = data->m_records[1].get();
	float delta = record->m_anim_time - move->m_anim_time;

	if (move->m_sim_time > 0.f && !move->dormant() && !record->dormant()) {
		vec3_t delta = move->m_origin - record->m_origin;
		if (delta.length() <= 128.f) {
			data->m_moved = true;
		}
	}

	// actual resolving down here.
	if (delta < 0.22f && delta > 0.01f && !record->m_fake_flick) // can't have lby if stopped less than .22 secs ago.
	{
		record->m_mode = Modes::RESOLVE_STOPPED_MOVING;
		///record->m_resolver_mode = XOR("moving");
		//record->m_resolver_mode2 = XOR("MOVING");
		record->m_eye_angles.y = move->m_body;
	}
	else if (record->m_anim_time >= data->m_body_update && data->m_body != data->m_old_body && delta > 0.22f && data->m_body_index < 2)
	{
		record->m_mode = Modes::RESOLVE_BODY;
		//record->m_resolver_mode = XOR("flick");
		//record->m_resolver_mode2 = XOR("FLICK");
		record->m_eye_angles.y = data->m_body;
		data->m_body_update = record->m_anim_time + g_csgo.m_globals->m_interval + 1.1; // default timer is a tick behind so I added one.
		iPlayers[record->m_player->index()] = false;
	}
	else if (data->m_moved && data->m_moving_index < 1 && delta < 1.1f && delta >= 0.22f && !record->m_fake_flick) // expire last move after 1.1 secs.
	{
		record->m_mode = Modes::RESOLVE_LAST_LBY;
	//	record->m_resolver_mode = XOR("last move");
	//	record->m_resolver_mode2 = XOR("LAST MOVE");
		record->m_eye_angles.y = move->m_body;
	}
	else if (data->m_moved && data->m_moving_index < 1 && delta > 1.1f && !ShouldUseFreestand(record)) // if in open.
	{
		record->m_mode = Modes::RESOLVE_LAST_LBY;
	//	record->m_resolver_mode = XOR("last move");
	//	record->m_resolver_mode2 = XOR("LAST MOVE");
		record->m_eye_angles.y = move->m_body;
	}
	else if (Spin_Detection(data) && data->m_spin_index < 1) // testing this.
	{
		record->m_mode = Modes::RESOLVE_SPIN;
		//record->m_resolver_mode = XOR("spin");
		//record->m_resolver_mode2 = XOR("SPIN");
		record->m_eye_angles.y = record->m_body;
	}
	else
	{
		if (ShouldUseFreestand(record)) // if not in open.
		{
			Freestand(player, data, record);
			return;
		}
		else if (data->m_moved && data->m_brute_index < 2) // brute based off last move.
		{
			record->m_mode = Modes::RESOLVE_BRUTEFORCE;
			//record->m_resolver_mode = XOR("bruteforce");
			//record->m_resolver_mode2 = XOR("BRUTE");
			switch (data->m_brute_index % 2) {

			case 0:
				record->m_eye_angles.y = move->m_body - 20.f;
				break;

			case 1:
				record->m_eye_angles.y = move->m_body + 20.f;
				break;

			default:
				break;
			}
		}
		else
		{
			record->m_mode = Modes::RESOLVE_BRUTEFORCE;
		//	record->m_resolver_mode = XOR("bruteforce");
		//	record->m_resolver_mode2 = XOR("BRUTE");
			switch (data->m_brute_index % 3) {

			case 0:
				record->m_eye_angles.y = backwards;
				break;

			case 1:
				record->m_eye_angles.y = backwards - 70.f;
				break;

			case 2:
				record->m_eye_angles.y = backwards + 70.f;
				break;

			default:
				break;
			}
		}
	}
}

void Resolver::ResolveWalk(AimPlayer* data, LagRecord* record)
{
	// apply lby to eyeangles.
	record->m_mode = RESOLVE_WALK;
	//record->m_resolver_mode = XOR("moving");
	//record->m_resolver_mode2 = XOR("MOVING");
	record->m_eye_angles.y = record->m_body;

	// delay body update.
	data->m_body_update = record->m_anim_time + 0.22f;

	// reset stand and body index.
	data->m_moving_index = 0;
	data->m_stand_index = 0;
	data->m_brute_index = 0;
	data->m_air_index = 0;
	data->m_body_index = 0;
	data->m_freestand_index = 0;
	data->m_spin_index = 0;

	std::memcpy(&data->m_walk_record, record, sizeof(LagRecord));
}

void Resolver::ResolveAir(Player* player, AimPlayer* data, LagRecord* record)
{
	LagRecord* move = &data->m_walk_record;
	float delta = record->m_anim_time - move->m_anim_time;
	float speed = record->m_velocity.length_2d();
	float velyaw = math::rad_to_deg(std::atan2(record->m_velocity.y, record->m_velocity.x));
	float away = GetAwayAngle(record);

	record->m_mode = Modes::RESOLVE_AIR;
	//record->m_resolver_mode = XOR("air");
	//record->m_resolver_mode2 = XOR("AIR");
	switch (data->m_air_index % 3) {
	case 0:
		record->m_eye_angles.y = record->m_body;
		break;

	case 1:
		record->m_eye_angles.y = math::normalize_float(velyaw + 110.f);
		break;

	case 2:
		record->m_eye_angles.y = math::normalize_float(velyaw - 110.f);
		break;
	}
}

void Resolver::ResolvePoses(Player* player, LagRecord* record)
{
	AimPlayer* data = &g_aimbot.m_players[player->index() - 1];

	// only do this bs when in air.
	if (record->m_mode == Modes::RESOLVE_AIR) {
		// ang = pose min + pose val x ( pose range )

		// lean_yaw
		player->m_flPoseParameter()[2] = g_csgo.RandomInt(0, 4) * 0.25f;

		// body_yaw
		player->m_flPoseParameter()[11] = g_csgo.RandomInt(1, 3) * 0.25f;
	}
}