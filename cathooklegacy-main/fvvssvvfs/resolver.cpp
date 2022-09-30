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
	if ((record->m_flags & FL_ONGROUND) && (speed < 0.1f || record->m_fake_walk || record->m_fake_flick))
	{
		record->m_eye_angles.x = 89.f;
		record->m_mode = Modes::RESOLVE_STAND;
	}

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

void Resolver::ResolveAngles(Player* player, LagRecord* record)
{
	// lets clean up the specator view a bit
	if (!g_cl.m_processing)
	{
		record->m_eye_angles.y = record->m_body;
		math::NormalizeAngle(record->m_eye_angles.y);
		return;
	}

	AimPlayer* data = &g_aimbot.m_players[player->index() - 1];

	// next up mark this record with a resolver mode that will be used.
	SetMode(record);

	if (record->m_mode == Modes::RESOLVE_WALK)
		ResolveWalk(data, record);

	else if (record->m_mode == Modes::RESOLVE_STAND)
		ResolveStand(data, record, player);

	else if (record->m_mode == Modes::RESOLVE_AIR)
		ResolveAir(player, data, record);

	// normalize the eye angles, doesn't really matter but its clean.
	math::NormalizeAngle(record->m_eye_angles.y);
}

float Resolver::AntiFreestanding(LagRecord* record) {
	// constants
	constexpr float STEP{ 4.f };
	constexpr float RANGE{ 32.f };

	// best target.
	vec3_t enemypos = record->m_player->GetShootPosition();
	float away = GetAwayAngle(record);

	// construct vector of angles to test.
	std::vector< AdaptiveAngle > angles{ };
	angles.emplace_back(away - 180.f);
	angles.emplace_back(away + 90.f);
	angles.emplace_back(away - 90.f);

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

			// over 50% of the total length, prioritize this shit.
			if (i > (len * 0.5f))
				mult = 1.25f;

			// over 90% of the total length, prioritize this shit.
			if (i > (len * 0.75f))
				mult = 1.25f;

			// over 90% of the total length, prioritize this shit.
			if (i > (len * 0.9f))
				mult = 2.f;

			// append 'penetrated distance'.
			it->m_dist += (STEP * mult);

			// mark that we found anything.
			valid = true;
		}
	}

	if (!valid) 
		return away + 180.f;
	
	// put the most distance at the front of the container.
	std::sort(angles.begin(), angles.end(),
		[](const AdaptiveAngle& a, const AdaptiveAngle& b) {
			return a.m_dist > b.m_dist;
		});

	// the best angle should be at the front now.
	AdaptiveAngle* best = &angles.front();
	return best->m_yaw;
}

void Resolver::ResolveStand(AimPlayer* data, LagRecord* record, Player* player) {

	data->m_moved = false;

	// for no-spread call a seperate resolver.
	//if (g_menu.main.misc.mode.get() == 1) {
		//StandNS(data, record);
		//;
	//}

	// get predicted away angle for the player.
	float away = GetAwayAngle(record);

	// pointer for easy access.
	LagRecord* move = &data->m_walk_record;

	C_AnimationLayer* curr = &record->m_layers[3];
	int act = data->m_player->GetSequenceActivity(curr->m_sequence);


	// we have a valid moving record.
	///if (move->m_sim_time > 0.f && !move->dormant() && !record->dormant() && data->m_last_move < 1) { // move->m_sim_time > 0.f && !move->dormant() && !record->dormant() && data->m_last_move < 1
	if (move->m_sim_time > 0.f && !move->dormant() && !record->dormant() && data->m_last_move < 1) {
		vec3_t delta = move->m_origin - record->m_origin;

		// check if moving record is close.
		if (delta.length() <= 100.f) {
			// indicate that we are using the moving lby.
			data->m_moved = true;
		}
	}
	bool breaking = CheckLBY(data->m_player, record, FindLastRecord(data));
	// a valid moving context was found
	if (data->m_moved == true) {
		float diff = math::NormalizedAngle(record->m_body - move->m_body);
		float delta = record->m_anim_time - move->m_anim_time;



		if (data->m_last_move < 1) {
			record->m_mode = Modes::RESOLVE_LASTMOVE;
			float diff = math::NormalizedAngle(record->m_body - move->m_body);
			float delta = record->m_anim_time - move->m_anim_time;
			//if (data->m_last_move < 1) {
			record->m_mode = Modes::RESOLVE_LASTMOVE;
			//data->m_last_move

			//record->m_resolver_mode = "last lby";

			record->m_eye_angles.y = move->m_body;

			//->m_resolver_mode = "last lby";
			//}
		}



		// it has not been time for this first update yet.
		if (data->m_records.size() >= 2)
		{
			LagRecord* previous = data->m_records[1].get();

			if (previous)
			{
				if (record->m_body != previous->m_body && data->m_body_index < 1)
				{
					record->m_eye_angles.y = record->m_body;
					data->m_body_update = record->m_anim_time + 1.1f;
					iPlayers[record->m_player->index()] = false;
					record->m_mode = Modes::RESOLVE_BODY;
					//record->m_resolver_mode = "lby flicking";
				}
			}
		}
	}
	else
		AntiFreestanding(record);
	//GetDirectionAngle(player->index(), player, record);

}
void Resolver::ResolveWalk(AimPlayer* data, LagRecord* record)
{
	// apply lby to eyeangles.
	record->m_mode = RESOLVE_WALK;
	record->m_eye_angles.y = record->m_body;

	// delay body update.
	data->m_body_update = record->m_anim_time + 0.22f;

	// reset stand and body index.
	data->m_stand_index = 0;
	data->m_body_index = 0;

	std::memcpy(&data->m_walk_record, record, sizeof(LagRecord));
}

bool Resolver::CheckLBY(Player* player, LagRecord* record, LagRecord* prev_record)
{
	if (player->m_vecVelocity().length_2d() > 1.1f)
		return false; // cant break here

	bool choking = fabs(player->m_flSimulationTime() - player->m_flOldSimulationTime()) > g_csgo.m_globals->m_interval;

	if (int i = 0; i < 13, i++)
	{
		auto layer = record->m_layers[i];
		auto prev_layer = prev_record->m_layers[i];

		// make sure that the animation happened
		if (layer.m_cycle != prev_layer.m_cycle)
		{
			if (layer.m_cycle > 0.9 || layer.m_weight == 1.f) // triggered layer
			{
				if (i == 3) // adjust layer sanity check. If it is the adjust layer, they are most likely breaking LBY
					return true;

				// lby flick lol!
				if (choking && fabs(math::NormalizedAngle(record->m_body - prev_record->m_body)) > 5.f)
					return true;
			}
			else if (choking) // for improper LBY breakers
			{
				if (player->GetSequenceActivity(layer.m_sequence) == 979)
				{
					if (player->GetSequenceActivity(prev_layer.m_sequence) == 979)
					{
						return true; // we can be pretty sure that they are breaking LBY
					}
				}
			}
			return false;
		}
		return false;
	}
	return false;
}

void Resolver::ResolveYawBruteforce(LagRecord* record, Player* player, AimPlayer* data)
{
	auto local_player = g_cl.m_local;
	if (!local_player)
		return;

	const float at_target_yaw = math::CalcAngle(player->m_vecOrigin(), local_player->m_vecOrigin()).y;

	switch (data->m_missed_shots % 4)
	{
	case 1:
		record->m_eye_angles.y = at_target_yaw + 180.f;
		break;
	case 2:
		record->m_eye_angles.y = at_target_yaw - 75.f;
		break;
	case 3:
		record->m_eye_angles.y = at_target_yaw + 75.f;
		break;
	}

}

void Resolver::ResolveAir(Player* player, AimPlayer* data, LagRecord* record)
{
	record->m_mode = Modes::RESOLVE_AIR;
	record->m_eye_angles.y = record->m_body;
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