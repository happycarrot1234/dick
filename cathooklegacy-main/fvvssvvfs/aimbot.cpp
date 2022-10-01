#include "includes.h"
#define shift_ticks 17

Aimbot g_aimbot{ };;

bool CanFireWithExploit(int m_iShiftedTick)
{
	// curtime before shift
	float curtime = game::TICKS_TO_TIME(g_cl.m_local->m_nTickBase() - m_iShiftedTick);
	return g_cl.CanFireWeapon(curtime);
}

bool Aimbot::CanDT() {
	int idx = g_cl.m_weapon->m_iItemDefinitionIndex();
	return g_cl.m_local->alive()
		&& g_csgo.m_cl->m_choked_commands <= 1
		&& m_double_tap && !g_hvh.m_fakeduck;
}

void Aimbot::DoubleTap()
{
	// lol what
	static auto insta = g_csgo.m_cvar->FindVar(HASH("sv_maxusrcmdprocessticks"));
	insta->SetValue(g_menu.main.aimbot.dtinsta.get() ? 62 : 0);

	static bool did_shift_before = false;
	static int double_tapped = 0;
	static int prev_shift_ticks = 0;
	static bool reset = false;
	const int dt_speed = static_cast<int>(g_menu.main.aimbot.dtspeed.get());
	const int dt_tick = static_cast<int>(g_menu.main.aimbot.dtspeed.get());

	g_cl.m_tick_to_shift = 0;
	if (CanDT())
	{
		if (m_double_tap)
		{
			prev_shift_ticks = 0;

			auto can_shift_shot = CanFireWithExploit(dt_speed);
			auto can_shot = CanFireWithExploit(abs(-1 - dt_speed));

			if (can_shift_shot || !can_shot && !did_shift_before)
			{
				prev_shift_ticks = dt_speed;
				double_tapped = 0;
			}
			else {
				double_tapped++;
				prev_shift_ticks = 0;
			}

			if (prev_shift_ticks > 0)
			{
				if (g_cl.m_weapon->DTable() && CanFireWithExploit(dt_speed))
				{
					if (g_cl.m_cmd->m_buttons & IN_ATTACK)
					{
						g_cl.m_tick_to_shift = dt_speed;
						reset = true;
					}
					else {
						if ((!(g_cl.m_cmd->m_buttons & IN_ATTACK) || !g_cl.m_shot) && reset
							&& fabsf(g_cl.m_weapon->m_fLastShotTime() - game::TICKS_TO_TIME(g_cl.m_local->m_nTickBase())) > 0.5f) {
							g_cl.m_charged = false;
							g_cl.m_tick_to_recharge = dt_speed;
							reset = false;
						}
					}
				}
			}
			did_shift_before = prev_shift_ticks != 0;
		}
	}
}

void FixVelocity(Player* m_player, LagRecord* record, LagRecord* previous, float max_speed) {
	if (!previous) {
		if (record->m_layers[6].m_playback_rate > 0.0f && record->m_layers[6].m_weight != 0.f && record->m_velocity.length() > 0.1f) {
			auto v30 = max_speed;

			if (record->m_flags & 6)
				v30 *= 0.34f;
			else if (m_player->m_bIsWalking())
				v30 *= 0.52f;

			auto v35 = record->m_layers[6].m_weight * v30;
			record->m_velocity *= v35 / record->m_velocity.length();
		}
		else
			record->m_velocity.clear();

		if (record->m_flags & 1)
			record->m_velocity.z = 0.f;

		record->m_anim_velocity = record->m_velocity;
		return;
	}

	if ((m_player->m_fEffects() & 8) != 0
		|| m_player->m_ubEFNoInterpParity() != m_player->m_ubEFNoInterpParityOld()) {
		record->m_velocity.clear();
		record->m_anim_velocity.clear();
		return;
	}

	auto is_jumping = !(record->m_flags & FL_ONGROUND && previous->m_flags & FL_ONGROUND);

	if (record->m_lag > 1) {
		record->m_velocity.clear();
		auto origin_delta = (record->m_origin - previous->m_origin);

		if (!(previous->m_flags & FL_ONGROUND || record->m_flags & FL_ONGROUND))// if not previous on ground or on ground
		{
			auto currently_ducking = record->m_flags & FL_DUCKING;
			if ((previous->m_flags & FL_DUCKING) != currently_ducking) {
				float duck_modifier = 0.f;

				if (currently_ducking)
					duck_modifier = 9.f;
				else
					duck_modifier = -9.f;

				origin_delta.z -= duck_modifier;
			}
		}

		auto sqrt_delta = origin_delta.length_2d_sqr();

		if (sqrt_delta > 0.f && sqrt_delta < 1000000.f)
			record->m_velocity = origin_delta / game::TICKS_TO_TIME(record->m_lag);

		record->m_velocity.validate_vec();

		if (is_jumping) {
			if (record->m_flags & FL_ONGROUND && !g_csgo.sv_enablebunnyhopping->GetInt()) {

				// 260 x 1.1 = 286 units/s.
				float max = m_player->m_flMaxspeed() * 1.1f;

				// get current velocity.
				float speed = record->m_velocity.length();

				// reset velocity to 286 units/s.
				if (max > 0.f && speed > max)
					record->m_velocity *= (max / speed);
			}

			// assume the player is bunnyhopping here so set the upwards impulse.
			record->m_velocity.z = g_csgo.sv_jump_impulse->GetFloat();
		}
		// we are not on the ground
		// apply gravity and airaccel.
		else if (!(record->m_flags & FL_ONGROUND)) {
			// apply one tick of gravity.
			record->m_velocity.z -= g_csgo.sv_gravity->GetFloat() * g_csgo.m_globals->m_interval;
		}
	}

	record->m_anim_velocity = record->m_velocity;

	if (!record->m_fake_walk) {
		if (record->m_anim_velocity.length_2d() > 0 && (record->m_flags & FL_ONGROUND)) {
			float anim_speed = 0.f;

			if (!is_jumping
				&& record->m_layers[11].m_weight > 0.0f
				&& record->m_layers[11].m_weight < 1.0f
				&& record->m_layers[11].m_playback_rate == previous->m_layers[11].m_playback_rate) {
				// calculate animation speed yielded by anim overlays
				auto flAnimModifier = 0.35f * (1.0f - record->m_layers[11].m_weight);
				if (flAnimModifier > 0.0f && flAnimModifier < 1.0f)
					anim_speed = max_speed * (flAnimModifier + 0.55f);
			}

			// this velocity is valid ONLY IN ANIMFIX UPDATE TICK!!!
			// so don't store it to record as m_vecVelocity
			// -L3D451R7
			if (anim_speed > 0.0f) {
				anim_speed /= record->m_anim_velocity.length_2d();
				record->m_anim_velocity.x *= anim_speed;
				record->m_anim_velocity.y *= anim_speed;
			}
		}
	}
	else
		record->m_anim_velocity.clear();

	record->m_anim_velocity.validate_vec();
}

void AimPlayer::UpdateAnimations(LagRecord* record) {
	CCSGOPlayerAnimState* state = m_player->m_PlayerAnimState();
	if (!state)
		return;

	// player respawned.
	if (m_player->m_flSpawnTime() != m_spawn) {
		// reset animation state.
		game::ResetAnimationState(state);

		// note new spawn time.
		m_spawn = m_player->m_flSpawnTime();
	}

	// first off lets backup our globals.
	auto curtime = g_csgo.m_globals->m_curtime;
	auto realtime = g_csgo.m_globals->m_realtime;
	auto frametime = g_csgo.m_globals->m_frametime;
	auto absframetime = g_csgo.m_globals->m_abs_frametime;
	auto framecount = g_csgo.m_globals->m_frame;
	auto tickcount = g_csgo.m_globals->m_tick_count;
	auto interpolation = g_csgo.m_globals->m_interp_amt;

	// backup stuff that we do not want to fuck with.
	AnimationBackup_t backup;

	backup.m_origin = m_player->m_vecOrigin();
	backup.m_abs_origin = m_player->GetAbsOrigin();
	backup.m_velocity = m_player->m_vecVelocity();
	backup.m_abs_velocity = m_player->m_vecAbsVelocity();
	backup.m_flags = m_player->m_fFlags();
	backup.m_eflags = m_player->m_iEFlags();
	backup.m_duck = m_player->m_flDuckAmount();
	backup.m_body = m_player->m_flLowerBodyYawTarget();
	m_player->GetAnimLayers(backup.m_layers);

	LagRecord* previous = m_records.size() > 1 ? m_records[1].get() : nullptr;

	if (previous && (previous->dormant() || !previous->m_setup))
		previous = nullptr;

	// set globals.
	g_csgo.m_globals->m_curtime = record->m_anim_time;
	g_csgo.m_globals->m_realtime = record->m_anim_time;
	g_csgo.m_globals->m_frame = game::TIME_TO_TICKS(record->m_anim_time);
	g_csgo.m_globals->m_tick_count = game::TIME_TO_TICKS(record->m_anim_time);
	g_csgo.m_globals->m_frametime = g_csgo.m_globals->m_interval;
	g_csgo.m_globals->m_abs_frametime = g_csgo.m_globals->m_interval;
	g_csgo.m_globals->m_interp_amt = 0.f;

	// is player a bot?
	bool bot = game::IsFakePlayer(m_player->index());
	// reset resolver/fakewalk/fakeflick state.
	record->m_mode = Resolver::Modes::RESOLVE_NONE;
	record->m_fake_walk = false;
	record->m_fake_flick = false;

	// thanks llama.
	if (record->m_flags & FL_ONGROUND) {
		// they are on ground.
		state->m_ground = true;
		// no they didnt land.
		state->m_land = false;
	}

	// fix velocity.
	float max_speed = 260.f;
	if (record->m_lag > 0 && record->m_lag < 16 && m_records.size() >= 2) {
		FixVelocity(m_player, record, previous, max_speed);
	}

	// fix CGameMovement::FinishGravity
	if (!(m_player->m_fFlags() & FL_ONGROUND))
		record->m_velocity.z -= game::TICKS_TO_TIME(g_csgo.sv_gravity->GetFloat());
	else
		record->m_velocity.z = 0.0f;

	// set this fucker, it will get overriden.
	record->m_anim_velocity = record->m_velocity;

	// fix various issues with the game.
	// these issues can only occur when a player is choking data.
	if (record->m_lag > 1 && !bot) {

		auto speed = record->m_velocity.length();


		// detect fakewalking players
		if (speed > 1.f
			&& record->m_lag >= 12
			&& record->m_layers[12].m_weight == 0.0f
			&& record->m_layers[6].m_weight == 0.0f
			&& record->m_layers[6].m_playback_rate < 0.0001f
			&& (record->m_flags & FL_ONGROUND))
			record->m_fake_walk = true;

		// if they fakewalk scratch this shit.
		if (record->m_fake_walk)
			record->m_anim_velocity = record->m_velocity = { 0.f, 0.f, 0.f };

		// detect fakeflicking players
		//if (record->m_velocity.length() < 18.f
		//	&& record->m_layers[6].m_weight != 1.0f
		//	&& record->m_layers[6].m_weight != 0.0f
		//	&& record->m_layers[6].m_weight != m_records.front().get()->m_layers[6].m_weight
		//	&& (record->m_flags & FL_ONGROUND))
		//	record->m_fake_flick = true;

		// detect fakeflicking players
		if (record->m_velocity.length() < 0.1f != record->m_velocity.length() > 38.0f) { // check
			if (record->m_velocity.length() < 0.1f
				&& record->m_layers[6].m_weight != 1.0f
				&& record->m_layers[6].m_weight != 0.0f
				&& record->m_layers[6].m_weight != previous->m_layers[6].m_weight // correct weight??
				&& (record->m_flags & FL_ONGROUND))
				record->m_fake_flick = true;
		}

		// we need atleast 2 updates/records
		// to fix these issues.
		if (m_records.size() >= 2) {
			// get pointer to previous record.
			LagRecord* previous = m_records[1].get();

			// valid previous record.
			if (previous && !previous->dormant()) {
				// LOL.
				if ((record->m_origin - previous->m_origin).is_zero())
					record->m_anim_velocity = record->m_velocity = { 0.f, 0.f, 0.f };

				// jumpfall.
				bool bOnGround = record->m_flags & FL_ONGROUND;
				bool bJumped = false;
				bool bLandedOnServer = false;
				float flLandTime = 0.f;

				// magic llama code.
				if (record->m_layers[4].m_cycle < 0.5f && (!(record->m_flags & FL_ONGROUND) || !(previous->m_flags & FL_ONGROUND))) {
					flLandTime = record->m_sim_time - float(record->m_layers[4].m_playback_rate / record->m_layers[4].m_cycle);
					bLandedOnServer = flLandTime >= previous->m_sim_time;
				}

				// jump_fall fix
				if (bLandedOnServer && !bJumped) {
					if (flLandTime <= record->m_anim_time) {
						bJumped = true;
						bOnGround = true;
					}
					else {
						bOnGround = previous->m_flags & FL_ONGROUND;
					}
				}

				// do the fix. hahaha
				if (bOnGround) {
					m_player->m_fFlags() |= FL_ONGROUND;
				}
				else {
					m_player->m_fFlags() &= ~FL_ONGROUND;
				}

				// delta in duckamt and delta in time..
				float duck = record->m_duck - previous->m_duck;
				float time = record->m_sim_time - previous->m_sim_time;

				// get the duckamt change per tick.
				float change = (duck / time) * g_csgo.m_globals->m_interval;

				// fix crouching players.
				m_player->m_flDuckAmount() = previous->m_duck + change;

				//if (!record->m_fake_walk) {
				//	// fix the velocity till the moment of animation.
				//	vec3_t velo = record->m_velocity - previous->m_velocity;

				//	// accel per tick.
				//	vec3_t accel = (velo / time) * g_csgo.m_globals->m_interval;

				//	// set the anim velocity to the previous velocity.
				//	// and predict one tick ahead.
				//	record->m_anim_velocity = previous->m_velocity + accel;
				//}
			}
		}
	}

	// lol?
	for (int i = 0; i < 13; i++)
		m_player->m_AnimOverlay()[i].m_1owner = m_player;


	bool fake = g_menu.main.aimbot.correct.get() && !bot;

	// if using fake angles, correct angles.
	if (fake)
		g_resolver.ResolveAngles(m_player, record);

	// force to use correct abs origin and velocity ( no CalcAbsolutePosition and CalcAbsoluteVelocity calls )
	m_player->m_iEFlags() &= ~(EFL_DIRTY_ABSTRANSFORM | EFL_DIRTY_ABSVELOCITY);

	// set stuff before animating.
	m_player->m_vecOrigin() = record->m_origin;
	m_player->m_vecVelocity() = m_player->m_vecAbsVelocity() = record->m_anim_velocity;
	m_player->m_flLowerBodyYawTarget() = record->m_body;

	// write potentially resolved angles.
	m_player->m_angEyeAngles() = record->m_eye_angles;

	// fix animating in same frame.
	if (state->m_frame == g_csgo.m_globals->m_frame)
		state->m_frame -= 1;

	// get invalidated bone cache.
	static auto& invalidatebonecache = pattern::find(g_csgo.m_client_dll, XOR("C6 05 ? ? ? ? ? 89 47 70")).add(0x2);

	// make sure we keep track of the original invalidation state
	const auto oldbonecache = invalidatebonecache;

	// update animtions now.
	m_player->m_bClientSideAnimation() = true;
	m_player->UpdateClientSideAnimation();
	m_player->m_bClientSideAnimation() = false;

	// we don't want to enable cache invalidation by accident
	invalidatebonecache = oldbonecache;

	// player animations have updated.
	m_player->InvalidatePhysicsRecursive(InvalidatePhysicsBits_t::ANGLES_CHANGED);
	m_player->InvalidatePhysicsRecursive(InvalidatePhysicsBits_t::ANIMATION_CHANGED);
	m_player->InvalidatePhysicsRecursive(InvalidatePhysicsBits_t::BOUNDS_CHANGED);
	m_player->InvalidatePhysicsRecursive(InvalidatePhysicsBits_t::SEQUENCE_CHANGED);

	// if fake angles.
	if (fake) {
		// correct poses.
		g_resolver.ResolvePoses(m_player, record);
	}

	// store updated/animated poses and rotation in lagrecord.
	m_player->GetPoseParameters(record->m_poses);
	record->m_abs_ang = m_player->GetAbsAngles();

	// restore backup data.
	m_player->m_vecOrigin() = backup.m_origin;
	m_player->m_vecVelocity() = backup.m_velocity;
	m_player->m_vecAbsVelocity() = backup.m_abs_velocity;
	m_player->m_fFlags() = backup.m_flags;
	m_player->m_iEFlags() = backup.m_eflags;
	m_player->m_flDuckAmount() = backup.m_duck;
	m_player->m_flLowerBodyYawTarget() = backup.m_body;
	m_player->SetAbsOrigin(backup.m_abs_origin);
	m_player->SetAnimLayers(backup.m_layers);

	// restore globals.
	g_csgo.m_globals->m_curtime = curtime;
	g_csgo.m_globals->m_realtime = realtime;
	g_csgo.m_globals->m_frametime = frametime;
	g_csgo.m_globals->m_abs_frametime = absframetime;
	g_csgo.m_globals->m_frame = framecount;
	g_csgo.m_globals->m_tick_count = tickcount;
	g_csgo.m_globals->m_interp_amt = interpolation;

	// IMPORTANT: do not restore poses here, since we want to preserve them for rendering.
	// also dont restore the render angles which indicate the model rotation.
}


void AimPlayer::OnNetUpdate(Player* player) {
	bool reset = (!g_menu.main.aimbot.enable.get() || player->m_lifeState() == LIFE_DEAD || !player->enemy(g_cl.m_local));
	bool disable = (!reset && !g_cl.m_processing);

	// if this happens, delete all the lagrecords.
	if (reset) {
		player->m_bClientSideAnimation() = true;
		m_records.clear();
		return;
	}

	// just disable anim if this is the case.
	if (disable) {
		player->m_bClientSideAnimation() = true;
		return;
	}

	// update player ptr if required.
	// reset player if changed.
	if (m_player != player)
		m_records.clear();

	// update player ptr.
	m_player = player;

	// indicate that this player has been out of pvs.
	// insert dummy record to separate records
	// to fix stuff like animation and prediction.
	if (player->dormant()) {
		bool insert = true;

		// we have any records already?
		if (!m_records.empty()) {

			LagRecord* front = m_records.front().get();

			// we already have a dormancy separator.
			if (front->dormant())
				insert = false;
		}

		if (insert) {
			// add new record.
			m_records.emplace_front(std::make_shared< LagRecord >(player));

			// get reference to newly added record.
			LagRecord* current = m_records.front().get();

			// mark as dormant.
			current->m_dormant = true;
		}
	}

	bool update = (m_records.empty() || player->m_flSimulationTime() > m_records.front().get()->m_sim_time);

	if (!update && !player->dormant() && player->m_vecOrigin() != player->m_vecOldOrigin()) {
		update = true;

		// fix data.
		player->m_flSimulationTime() = game::TICKS_TO_TIME(g_csgo.m_cl->m_server_tick);
	}

	// this is the first data update we are receving
	// OR we received data with a newer simulation context.
	if (update) {
		// add new record.
		m_records.emplace_front(std::make_shared< LagRecord >(player));

		// get reference to newly added record.
		LagRecord* current = m_records.front().get();

		// mark as non dormant.
		current->m_dormant = false;

		// update animations on current record.
		// call resolver.
		UpdateAnimations(current);

		// create bone matrix for this record.
		g_bones.setup(m_player, nullptr, current);
	}

	// no need to store insane amt of data.
	while (m_records.size() > 256)
		m_records.pop_back();
}

void AimPlayer::OnRoundStart(Player* player) {
	m_player = player;
	m_walk_record = LagRecord{ };
	m_shots = 0;
	m_missed_shots = 0;

	// reset stand and body index.
	m_stand_index = 0;
	m_stand_index2 = 0;
	m_body_index = 0;

	m_records.clear();
	m_hitboxes.clear();

	// IMPORTANT: DO NOT CLEAR LAST HIT SHIT.
}

void AimPlayer::SetupHitboxes(LagRecord* record, bool history) {
	// reset hitboxes.
	m_hitboxes.clear();

	bool prefer_head = record->m_velocity.length_2d() > 71.f;

	if (g_cl.m_weapon_id == ZEUS) {
		// hitboxes for the zeus.
		m_hitboxes.push_back({ HITBOX_BODY, HitscanMode::PREFER });
		return;
	}
	else if (g_cl.m_weapon_id == GLOCK || g_cl.m_weapon_id == P2000 || g_cl.m_weapon_id == USPS || g_cl.m_weapon_id == ELITE || g_cl.m_weapon_id == P250 || g_cl.m_weapon_id == TEC9 || g_cl.m_weapon_id == CZ75A)
	{
		// prefer, always.
		if (g_menu.main.aimbot.pistol_preferbaim.get(0))
			m_hitboxes.push_back({ HITBOX_BODY, HitscanMode::PREFER });

		// prefer, lethal.
		if (g_menu.main.aimbot.pistol_preferbaim.get(1))
			m_hitboxes.push_back({ HITBOX_BODY, HitscanMode::LETHAL });

		// prefer, lethal x2.
		if (g_menu.main.aimbot.pistol_preferbaim.get(2))
			m_hitboxes.push_back({ HITBOX_BODY, HitscanMode::LETHAL2 });

		// prefer, fake.
		if (g_menu.main.aimbot.pistol_preferbaim.get(3) && record->m_mode != Resolver::Modes::RESOLVE_NONE && record->m_mode != Resolver::Modes::RESOLVE_WALK)
			m_hitboxes.push_back({ HITBOX_BODY, HitscanMode::PREFER });

		// prefer, in air.
		if (g_menu.main.aimbot.pistol_preferbaim.get(4) && !(record->m_pred_flags & FL_ONGROUND))
			m_hitboxes.push_back({ HITBOX_BODY, HitscanMode::PREFER });

		//head flags

		if (g_menu.main.aimbot.pistol_preferhead.get(0))
			m_hitboxes.push_back({ HITBOX_HEAD, HitscanMode::PREFER });

		if (g_menu.main.aimbot.pistol_preferhead.get(1) && prefer_head)
			m_hitboxes.push_back({ HITBOX_HEAD, HitscanMode::PREFER });

		if (g_menu.main.aimbot.pistol_preferhead.get(2) && !(record->m_mode != Resolver::Modes::RESOLVE_NONE && record->m_mode != Resolver::Modes::RESOLVE_WALK && record->m_mode != Resolver::Modes::RESOLVE_STOPPED_MOVING))
			m_hitboxes.push_back({ HITBOX_HEAD, HitscanMode::PREFER });

		if (g_menu.main.aimbot.pistol_preferhead.get(3) && !(record->m_pred_flags & FL_ONGROUND))
			m_hitboxes.push_back({ HITBOX_HEAD, HitscanMode::PREFER });

		bool only{ false };

		// only, always.
		if (g_menu.main.aimbot.pistol_forcebaim.get(0)) {
			only = true;
			m_hitboxes.push_back({ HITBOX_BODY, HitscanMode::PREFER });
		}

		// only, health.
		if (g_menu.main.aimbot.pistol_forcebaim.get(1) && m_player->m_iHealth() <= (int)g_menu.main.aimbot.pistol_forcehealth.get()) {
			only = true;
			m_hitboxes.push_back({ HITBOX_BODY, HitscanMode::PREFER });
		}

		// only, fake.
		if (g_menu.main.aimbot.pistol_forcebaim.get(2) && record->m_mode != Resolver::Modes::RESOLVE_NONE && record->m_mode != Resolver::Modes::RESOLVE_WALK) {
			only = true;
			m_hitboxes.push_back({ HITBOX_BODY, HitscanMode::PREFER });
		}

		// only, in air.
		if (g_menu.main.aimbot.pistol_forcebaim.get(3) && !(record->m_pred_flags & FL_ONGROUND)) {
			only = true;
			m_hitboxes.push_back({ HITBOX_BODY, HitscanMode::PREFER });
		}

		// only, on key.
		if (g_input.GetKeyState(g_menu.main.aimbot.baim_key.get())) {
			only = true;
			m_hitboxes.push_back({ HITBOX_BODY, HitscanMode::NORMAL });
			m_hitboxes.push_back({ HITBOX_CHEST, HitscanMode::NORMAL });
			m_hitboxes.push_back({ HITBOX_PELVIS, HitscanMode::NORMAL });
		}

		// only baim conditions have been met.
		// do not insert more hitboxes.
		if (only)
			return;

		std::vector< size_t > hitbox{ g_menu.main.aimbot.pistol_hitboxes.GetActiveIndices() };
		if (hitbox.empty())
			return;

		for (const auto& h : hitbox) {
			// head.
			if (h == 0)
				m_hitboxes.push_back({ HITBOX_HEAD, HitscanMode::NORMAL });

			// upper chest
			if (h == 1) {
				m_hitboxes.push_back({ HITBOX_UPPER_CHEST, HitscanMode::NORMAL });
			}

			// chest.
			if (h == 2) {
				m_hitboxes.push_back({ HITBOX_THORAX, HitscanMode::NORMAL });
				m_hitboxes.push_back({ HITBOX_CHEST, HitscanMode::NORMAL });
			}

			// stomach.
			if (h == 3) {
				m_hitboxes.push_back({ HITBOX_BODY, HitscanMode::NORMAL });
			}

			// pelvis.
			if (h == 4) {
				m_hitboxes.push_back({ HITBOX_PELVIS, HitscanMode::NORMAL });
			}

			// arms.
			if (h == 5) {
				m_hitboxes.push_back({ HITBOX_L_UPPER_ARM, HitscanMode::NORMAL });
				m_hitboxes.push_back({ HITBOX_R_UPPER_ARM, HitscanMode::NORMAL });
			}

			// legs.
			if (h == 6) {
				m_hitboxes.push_back({ HITBOX_L_THIGH, HitscanMode::NORMAL });
				m_hitboxes.push_back({ HITBOX_R_THIGH, HitscanMode::NORMAL });
				m_hitboxes.push_back({ HITBOX_L_CALF, HitscanMode::NORMAL });
				m_hitboxes.push_back({ HITBOX_R_CALF, HitscanMode::NORMAL });
			}

			if (h == 7) {
				m_hitboxes.push_back({ HITBOX_L_FOOT, HitscanMode::NORMAL });
				m_hitboxes.push_back({ HITBOX_R_FOOT, HitscanMode::NORMAL });
			}
		}
	}
	else if (g_cl.m_weapon_id == DEAGLE || g_cl.m_weapon_id == REVOLVER)
	{
		// prefer, always.
		if (g_menu.main.aimbot.heavypistol_preferbaim.get(0))
			m_hitboxes.push_back({ HITBOX_BODY, HitscanMode::PREFER });

		// prefer, lethal.
		if (g_menu.main.aimbot.heavypistol_preferbaim.get(1))
			m_hitboxes.push_back({ HITBOX_BODY, HitscanMode::LETHAL });

		// prefer, lethal x2.
		if (g_menu.main.aimbot.heavypistol_preferbaim.get(2))
			m_hitboxes.push_back({ HITBOX_BODY, HitscanMode::LETHAL2 });

		// prefer, fake.
		if (g_menu.main.aimbot.heavypistol_preferbaim.get(3) && record->m_mode != Resolver::Modes::RESOLVE_NONE && record->m_mode != Resolver::Modes::RESOLVE_WALK)
			m_hitboxes.push_back({ HITBOX_BODY, HitscanMode::PREFER });

		// prefer, in air.
		if (g_menu.main.aimbot.heavypistol_preferbaim.get(4) && !(record->m_pred_flags & FL_ONGROUND))
			m_hitboxes.push_back({ HITBOX_BODY, HitscanMode::PREFER });

		//prefer head
		if (g_menu.main.aimbot.heavypistol_preferhead.get(0))
			m_hitboxes.push_back({ HITBOX_HEAD, HitscanMode::PREFER });

		if (g_menu.main.aimbot.heavypistol_preferhead.get(1) && prefer_head)
			m_hitboxes.push_back({ HITBOX_HEAD, HitscanMode::PREFER });

		if (g_menu.main.aimbot.heavypistol_preferhead.get(2) && !(record->m_mode != Resolver::Modes::RESOLVE_NONE && record->m_mode != Resolver::Modes::RESOLVE_WALK && record->m_mode != Resolver::Modes::RESOLVE_STOPPED_MOVING))
			m_hitboxes.push_back({ HITBOX_HEAD, HitscanMode::PREFER });

		if (g_menu.main.aimbot.heavypistol_preferhead.get(3) && !(record->m_pred_flags & FL_ONGROUND))
			m_hitboxes.push_back({ HITBOX_HEAD, HitscanMode::PREFER });

		bool only{ false };

		// only, always.
		if (g_menu.main.aimbot.heavypistol_forcebaim.get(0)) {
			only = true;
			m_hitboxes.push_back({ HITBOX_BODY, HitscanMode::PREFER });
		}

		// only, health.
		if (g_menu.main.aimbot.heavypistol_forcebaim.get(1) && m_player->m_iHealth() <= (int)g_menu.main.aimbot.heavypistol_forcehealth.get()) {
			only = true;
			m_hitboxes.push_back({ HITBOX_BODY, HitscanMode::PREFER });
		}

		// only, fake.
		if (g_menu.main.aimbot.heavypistol_forcebaim.get(2) && record->m_mode != Resolver::Modes::RESOLVE_NONE && record->m_mode != Resolver::Modes::RESOLVE_WALK) {
			only = true;
			m_hitboxes.push_back({ HITBOX_BODY, HitscanMode::PREFER });
		}

		// only, in air.
		if (g_menu.main.aimbot.heavypistol_forcebaim.get(3) && !(record->m_pred_flags & FL_ONGROUND)) {
			only = true;
			m_hitboxes.push_back({ HITBOX_BODY, HitscanMode::PREFER });
		}

		// only, on key.
		if (g_input.GetKeyState(g_menu.main.aimbot.baim_key.get())) {
			only = true;
			m_hitboxes.push_back({ HITBOX_BODY, HitscanMode::NORMAL });
			m_hitboxes.push_back({ HITBOX_CHEST, HitscanMode::NORMAL });
			m_hitboxes.push_back({ HITBOX_PELVIS, HitscanMode::NORMAL });
		}

		// only baim conditions have been met.
		// do not insert more hitboxes.
		if (only)
			return;

		std::vector< size_t > hitbox{ g_menu.main.aimbot.heavypistol_hitboxes.GetActiveIndices() };
		if (hitbox.empty())
			return;

		for (const auto& h : hitbox) {
			// head.
			if (h == 0)
				m_hitboxes.push_back({ HITBOX_HEAD, HitscanMode::NORMAL });

			// upper chest
			if (h == 1) {
				m_hitboxes.push_back({ HITBOX_UPPER_CHEST, HitscanMode::NORMAL });
			}

			// chest.
			if (h == 2) {
				m_hitboxes.push_back({ HITBOX_THORAX, HitscanMode::NORMAL });
				m_hitboxes.push_back({ HITBOX_CHEST, HitscanMode::NORMAL });
			}

			// stomach.
			if (h == 3) {
				m_hitboxes.push_back({ HITBOX_BODY, HitscanMode::NORMAL });
			}

			// pelvis.
			if (h == 4) {
				m_hitboxes.push_back({ HITBOX_PELVIS, HitscanMode::NORMAL });
			}

			// arms.
			if (h == 5) {
				m_hitboxes.push_back({ HITBOX_L_UPPER_ARM, HitscanMode::NORMAL });
				m_hitboxes.push_back({ HITBOX_R_UPPER_ARM, HitscanMode::NORMAL });
			}

			// legs.
			if (h == 6) {
				m_hitboxes.push_back({ HITBOX_L_THIGH, HitscanMode::NORMAL });
				m_hitboxes.push_back({ HITBOX_R_THIGH, HitscanMode::NORMAL });
				m_hitboxes.push_back({ HITBOX_L_CALF, HitscanMode::NORMAL });
				m_hitboxes.push_back({ HITBOX_R_CALF, HitscanMode::NORMAL });
			}

			if (h == 7) {
				m_hitboxes.push_back({ HITBOX_L_FOOT, HitscanMode::NORMAL });
				m_hitboxes.push_back({ HITBOX_R_FOOT, HitscanMode::NORMAL });
			}
		}
	}
	else if (g_cl.m_weapon_id == SSG08)
	{
		// prefer, always.
		if (g_menu.main.aimbot.scout_preferbaim.get(0))
			m_hitboxes.push_back({ HITBOX_BODY, HitscanMode::PREFER });

		// prefer, lethal.
		if (g_menu.main.aimbot.scout_preferbaim.get(1))
			m_hitboxes.push_back({ HITBOX_BODY, HitscanMode::LETHAL });

		// prefer, lethal x2.
		if (g_menu.main.aimbot.scout_preferbaim.get(2))
			m_hitboxes.push_back({ HITBOX_BODY, HitscanMode::LETHAL2 });

		// prefer, fake.
		if (g_menu.main.aimbot.scout_preferbaim.get(3) && record->m_mode != Resolver::Modes::RESOLVE_NONE && record->m_mode != Resolver::Modes::RESOLVE_WALK)
			m_hitboxes.push_back({ HITBOX_BODY, HitscanMode::PREFER });

		// prefer, in air.
		if (g_menu.main.aimbot.scout_preferbaim.get(4) && !(record->m_pred_flags & FL_ONGROUND))
			m_hitboxes.push_back({ HITBOX_BODY, HitscanMode::PREFER });

		//prefer head
		if (g_menu.main.aimbot.scout_preferhead.get(0))
			m_hitboxes.push_back({ HITBOX_HEAD, HitscanMode::PREFER });

		if (g_menu.main.aimbot.scout_preferhead.get(1) && prefer_head)
			m_hitboxes.push_back({ HITBOX_HEAD, HitscanMode::PREFER });

		if (g_menu.main.aimbot.scout_preferhead.get(2) && !(record->m_mode != Resolver::Modes::RESOLVE_NONE && record->m_mode != Resolver::Modes::RESOLVE_WALK && record->m_mode != Resolver::Modes::RESOLVE_STOPPED_MOVING))
			m_hitboxes.push_back({ HITBOX_HEAD, HitscanMode::PREFER });

		if (g_menu.main.aimbot.scout_preferhead.get(3) && !(record->m_pred_flags & FL_ONGROUND))
			m_hitboxes.push_back({ HITBOX_HEAD, HitscanMode::PREFER });

		bool only{ false };

		// only, always.
		if (g_menu.main.aimbot.scout_forcebaim.get(0)) {
			only = true;
			m_hitboxes.push_back({ HITBOX_BODY, HitscanMode::PREFER });
		}

		// only, health.
		if (g_menu.main.aimbot.scout_forcebaim.get(1) && m_player->m_iHealth() <= (int)g_menu.main.aimbot.scout_forcehealth.get()) {
			only = true;
			m_hitboxes.push_back({ HITBOX_BODY, HitscanMode::PREFER });
		}

		// only, fake.
		if (g_menu.main.aimbot.scout_forcebaim.get(2) && record->m_mode != Resolver::Modes::RESOLVE_NONE && record->m_mode != Resolver::Modes::RESOLVE_WALK) {
			only = true;
			m_hitboxes.push_back({ HITBOX_BODY, HitscanMode::PREFER });
		}

		// only, in air.
		if (g_menu.main.aimbot.scout_forcebaim.get(3) && !(record->m_pred_flags & FL_ONGROUND)) {
			only = true;
			m_hitboxes.push_back({ HITBOX_BODY, HitscanMode::PREFER });
		}

		// only, on key.
		if (g_input.GetKeyState(g_menu.main.aimbot.baim_key.get())) {
			only = true;
			m_hitboxes.push_back({ HITBOX_BODY, HitscanMode::NORMAL });
			m_hitboxes.push_back({ HITBOX_CHEST, HitscanMode::NORMAL });
			m_hitboxes.push_back({ HITBOX_PELVIS, HitscanMode::NORMAL });
		}

		// only baim conditions have been met.
		// do not insert more hitboxes.
		if (only)
			return;

		std::vector< size_t > hitbox{ g_menu.main.aimbot.scout_hitboxes.GetActiveIndices() };
		if (hitbox.empty())
			return;

		for (const auto& h : hitbox) {
			// head.
			if (h == 0)
				m_hitboxes.push_back({ HITBOX_HEAD, HitscanMode::NORMAL });

			// upper chest
			if (h == 1) {
				m_hitboxes.push_back({ HITBOX_UPPER_CHEST, HitscanMode::NORMAL });
			}

			// chest.
			if (h == 2) {
				m_hitboxes.push_back({ HITBOX_THORAX, HitscanMode::NORMAL });
				m_hitboxes.push_back({ HITBOX_CHEST, HitscanMode::NORMAL });
			}

			// stomach.
			if (h == 3) {
				m_hitboxes.push_back({ HITBOX_BODY, HitscanMode::NORMAL });
			}

			// pelvis.
			if (h == 4) {
				m_hitboxes.push_back({ HITBOX_PELVIS, HitscanMode::NORMAL });
			}

			// arms.
			if (h == 5) {
				m_hitboxes.push_back({ HITBOX_L_UPPER_ARM, HitscanMode::NORMAL });
				m_hitboxes.push_back({ HITBOX_R_UPPER_ARM, HitscanMode::NORMAL });
			}

			// legs.
			if (h == 6) {
				m_hitboxes.push_back({ HITBOX_L_THIGH, HitscanMode::NORMAL });
				m_hitboxes.push_back({ HITBOX_R_THIGH, HitscanMode::NORMAL });
				m_hitboxes.push_back({ HITBOX_L_CALF, HitscanMode::NORMAL });
				m_hitboxes.push_back({ HITBOX_R_CALF, HitscanMode::NORMAL });
			}

			if (h == 7) {
				m_hitboxes.push_back({ HITBOX_L_FOOT, HitscanMode::NORMAL });
				m_hitboxes.push_back({ HITBOX_R_FOOT, HitscanMode::NORMAL });
			}
		}
	}
	else if (g_cl.m_weapon_id == G3SG1 || g_cl.m_weapon_id == SCAR20)
	{
		// prefer, always.
		if (g_menu.main.aimbot.auto_preferbaim.get(0))
			m_hitboxes.push_back({ HITBOX_BODY, HitscanMode::PREFER });

		// prefer, lethal.
		if (g_menu.main.aimbot.auto_preferbaim.get(1))
			m_hitboxes.push_back({ HITBOX_BODY, HitscanMode::LETHAL });

		// prefer, lethal x2.
		if (g_menu.main.aimbot.auto_preferbaim.get(2))
			m_hitboxes.push_back({ HITBOX_BODY, HitscanMode::LETHAL2 });

		// prefer, fake.
		if (g_menu.main.aimbot.auto_preferbaim.get(3) && record->m_mode != Resolver::Modes::RESOLVE_NONE && record->m_mode != Resolver::Modes::RESOLVE_WALK)
			m_hitboxes.push_back({ HITBOX_BODY, HitscanMode::PREFER });

		// prefer, in air.
		if (g_menu.main.aimbot.auto_preferbaim.get(4) && !(record->m_pred_flags & FL_ONGROUND))
			m_hitboxes.push_back({ HITBOX_BODY, HitscanMode::PREFER });

		//prefer head
		if (g_menu.main.aimbot.auto_preferhead.get(0))
			m_hitboxes.push_back({ HITBOX_HEAD, HitscanMode::PREFER });

		if (g_menu.main.aimbot.auto_preferhead.get(1) && prefer_head)
			m_hitboxes.push_back({ HITBOX_HEAD, HitscanMode::PREFER });

		if (g_menu.main.aimbot.auto_preferhead.get(2) && !(record->m_mode != Resolver::Modes::RESOLVE_NONE && record->m_mode != Resolver::Modes::RESOLVE_WALK && record->m_mode != Resolver::Modes::RESOLVE_STOPPED_MOVING))
			m_hitboxes.push_back({ HITBOX_HEAD, HitscanMode::PREFER });

		if (g_menu.main.aimbot.auto_preferhead.get(3) && !(record->m_pred_flags & FL_ONGROUND))
			m_hitboxes.push_back({ HITBOX_HEAD, HitscanMode::PREFER });

		bool only{ false };

		// only, always.
		if (g_menu.main.aimbot.auto_forcebaim.get(0)) {
			only = true;
			m_hitboxes.push_back({ HITBOX_BODY, HitscanMode::PREFER });
		}

		// only, health.
		if (g_menu.main.aimbot.auto_forcebaim.get(1) && m_player->m_iHealth() <= (int)g_menu.main.aimbot.auto_forcehealth.get()) {
			only = true;
			m_hitboxes.push_back({ HITBOX_BODY, HitscanMode::PREFER });
		}

		// only, fake.
		if (g_menu.main.aimbot.auto_forcebaim.get(2) && record->m_mode != Resolver::Modes::RESOLVE_NONE && record->m_mode != Resolver::Modes::RESOLVE_WALK) {
			only = true;
			m_hitboxes.push_back({ HITBOX_BODY, HitscanMode::PREFER });
		}

		// only, in air.
		if (g_menu.main.aimbot.auto_forcebaim.get(3) && !(record->m_pred_flags & FL_ONGROUND)) {
			only = true;
			m_hitboxes.push_back({ HITBOX_BODY, HitscanMode::PREFER });
		}

		// only, on key.
		if (g_input.GetKeyState(g_menu.main.aimbot.baim_key.get())) {
			only = true;
			m_hitboxes.push_back({ HITBOX_BODY, HitscanMode::NORMAL });
			m_hitboxes.push_back({ HITBOX_CHEST, HitscanMode::NORMAL });
			m_hitboxes.push_back({ HITBOX_PELVIS, HitscanMode::NORMAL });
		}

		// only baim conditions have been met.
		// do not insert more hitboxes.
		if (only)
			return;

		std::vector< size_t > hitbox{ g_menu.main.aimbot.auto_hitboxes.GetActiveIndices() };
		if (hitbox.empty())
			return;

		for (const auto& h : hitbox) {
			// head.
			if (h == 0)
				m_hitboxes.push_back({ HITBOX_HEAD, HitscanMode::NORMAL });

			// upper chest
			if (h == 1) {
				m_hitboxes.push_back({ HITBOX_UPPER_CHEST, HitscanMode::NORMAL });
			}

			// chest.
			if (h == 2) {
				m_hitboxes.push_back({ HITBOX_THORAX, HitscanMode::NORMAL });
				m_hitboxes.push_back({ HITBOX_CHEST, HitscanMode::NORMAL });
			}

			// stomach.
			if (h == 3) {
				m_hitboxes.push_back({ HITBOX_BODY, HitscanMode::NORMAL });
			}

			// pelvis.
			if (h == 4) {
				m_hitboxes.push_back({ HITBOX_PELVIS, HitscanMode::NORMAL });
			}

			// arms.
			if (h == 5) {
				m_hitboxes.push_back({ HITBOX_L_UPPER_ARM, HitscanMode::NORMAL });
				m_hitboxes.push_back({ HITBOX_R_UPPER_ARM, HitscanMode::NORMAL });
			}

			// legs.
			if (h == 6) {
				m_hitboxes.push_back({ HITBOX_L_THIGH, HitscanMode::NORMAL });
				m_hitboxes.push_back({ HITBOX_R_THIGH, HitscanMode::NORMAL });
				m_hitboxes.push_back({ HITBOX_L_CALF, HitscanMode::NORMAL });
				m_hitboxes.push_back({ HITBOX_R_CALF, HitscanMode::NORMAL });
			}

			if (h == 7) {
				m_hitboxes.push_back({ HITBOX_L_FOOT, HitscanMode::NORMAL });
				m_hitboxes.push_back({ HITBOX_R_FOOT, HitscanMode::NORMAL });
			}
		}
	}
	else if (g_cl.m_weapon_id == AWP)
	{
		// prefer, always.
		if (g_menu.main.aimbot.awp_preferbaim.get(0))
			m_hitboxes.push_back({ HITBOX_BODY, HitscanMode::PREFER });

		// prefer, lethal.
		if (g_menu.main.aimbot.awp_preferbaim.get(1))
			m_hitboxes.push_back({ HITBOX_BODY, HitscanMode::LETHAL });

		// prefer, lethal x2.
		if (g_menu.main.aimbot.awp_preferbaim.get(2))
			m_hitboxes.push_back({ HITBOX_BODY, HitscanMode::LETHAL2 });

		// prefer, fake.
		if (g_menu.main.aimbot.awp_preferbaim.get(3) && record->m_mode != Resolver::Modes::RESOLVE_NONE && record->m_mode != Resolver::Modes::RESOLVE_WALK)
			m_hitboxes.push_back({ HITBOX_BODY, HitscanMode::PREFER });

		// prefer, in air.
		if (g_menu.main.aimbot.awp_preferbaim.get(4) && !(record->m_pred_flags & FL_ONGROUND))
			m_hitboxes.push_back({ HITBOX_BODY, HitscanMode::PREFER });

		//prefer head
		if (g_menu.main.aimbot.awp_preferhead.get(0))
			m_hitboxes.push_back({ HITBOX_HEAD, HitscanMode::PREFER });

		if (g_menu.main.aimbot.awp_preferhead.get(1) && prefer_head)
			m_hitboxes.push_back({ HITBOX_HEAD, HitscanMode::PREFER });

		if (g_menu.main.aimbot.awp_preferhead.get(2) && !(record->m_mode != Resolver::Modes::RESOLVE_NONE && record->m_mode != Resolver::Modes::RESOLVE_WALK && record->m_mode != Resolver::Modes::RESOLVE_STOPPED_MOVING))
			m_hitboxes.push_back({ HITBOX_HEAD, HitscanMode::PREFER });

		if (g_menu.main.aimbot.awp_preferhead.get(3) && !(record->m_pred_flags & FL_ONGROUND))
			m_hitboxes.push_back({ HITBOX_HEAD, HitscanMode::PREFER });

		bool only{ false };

		// only, always.
		if (g_menu.main.aimbot.awp_forcebaim.get(0)) {
			only = true;
			m_hitboxes.push_back({ HITBOX_BODY, HitscanMode::PREFER });
		}

		// only, health.
		if (g_menu.main.aimbot.awp_forcebaim.get(1) && m_player->m_iHealth() <= (int)g_menu.main.aimbot.awp_forcehealth.get()) {
			only = true;
			m_hitboxes.push_back({ HITBOX_BODY, HitscanMode::PREFER });
		}

		// only, fake.
		if (g_menu.main.aimbot.awp_forcebaim.get(2) && record->m_mode != Resolver::Modes::RESOLVE_NONE && record->m_mode != Resolver::Modes::RESOLVE_WALK) {
			only = true;
			m_hitboxes.push_back({ HITBOX_BODY, HitscanMode::PREFER });
		}

		// only, in air.
		if (g_menu.main.aimbot.awp_forcebaim.get(3) && !(record->m_pred_flags & FL_ONGROUND)) {
			only = true;
			m_hitboxes.push_back({ HITBOX_BODY, HitscanMode::PREFER });
		}

		// only, on key.
		if (g_input.GetKeyState(g_menu.main.aimbot.baim_key.get())) {
			only = true;
			m_hitboxes.push_back({ HITBOX_BODY, HitscanMode::NORMAL });
			m_hitboxes.push_back({ HITBOX_CHEST, HitscanMode::NORMAL });
			m_hitboxes.push_back({ HITBOX_PELVIS, HitscanMode::NORMAL });
		}

		// only baim conditions have been met.
		// do not insert more hitboxes.
		if (only)
			return;

		std::vector< size_t > hitbox{ g_menu.main.aimbot.awp_hitboxes.GetActiveIndices() };
		if (hitbox.empty())
			return;

		for (const auto& h : hitbox) {
			// head.
			if (h == 0)
				m_hitboxes.push_back({ HITBOX_HEAD, HitscanMode::NORMAL });

			// upper chest
			if (h == 1) {
				m_hitboxes.push_back({ HITBOX_UPPER_CHEST, HitscanMode::NORMAL });
			}

			// chest.
			if (h == 2) {
				m_hitboxes.push_back({ HITBOX_THORAX, HitscanMode::NORMAL });
				m_hitboxes.push_back({ HITBOX_CHEST, HitscanMode::NORMAL });
			}

			// stomach.
			if (h == 3) {
				m_hitboxes.push_back({ HITBOX_BODY, HitscanMode::NORMAL });
			}

			// pelvis.
			if (h == 4) {
				m_hitboxes.push_back({ HITBOX_PELVIS, HitscanMode::NORMAL });
			}

			// arms.
			if (h == 5) {
				m_hitboxes.push_back({ HITBOX_L_UPPER_ARM, HitscanMode::NORMAL });
				m_hitboxes.push_back({ HITBOX_R_UPPER_ARM, HitscanMode::NORMAL });
			}

			// legs.
			if (h == 6) {
				m_hitboxes.push_back({ HITBOX_L_THIGH, HitscanMode::NORMAL });
				m_hitboxes.push_back({ HITBOX_R_THIGH, HitscanMode::NORMAL });
				m_hitboxes.push_back({ HITBOX_L_CALF, HitscanMode::NORMAL });
				m_hitboxes.push_back({ HITBOX_R_CALF, HitscanMode::NORMAL });
			}

			if (h == 7) {
				m_hitboxes.push_back({ HITBOX_L_FOOT, HitscanMode::NORMAL });
				m_hitboxes.push_back({ HITBOX_R_FOOT, HitscanMode::NORMAL });
			}
		}
	}
	else
	{
		// prefer, always.
		if (g_menu.main.aimbot.general_preferbaim.get(0))
			m_hitboxes.push_back({ HITBOX_BODY, HitscanMode::PREFER });

		// prefer, lethal.
		if (g_menu.main.aimbot.general_preferbaim.get(1))
			m_hitboxes.push_back({ HITBOX_BODY, HitscanMode::LETHAL });

		// prefer, lethal x2.
		if (g_menu.main.aimbot.general_preferbaim.get(2))
			m_hitboxes.push_back({ HITBOX_BODY, HitscanMode::LETHAL2 });

		// prefer, fake.
		if (g_menu.main.aimbot.general_preferbaim.get(3) && record->m_mode != Resolver::Modes::RESOLVE_NONE && record->m_mode != Resolver::Modes::RESOLVE_WALK)
			m_hitboxes.push_back({ HITBOX_BODY, HitscanMode::PREFER });

		// prefer, in air.
		if (g_menu.main.aimbot.general_preferbaim.get(4) && !(record->m_pred_flags & FL_ONGROUND))
			m_hitboxes.push_back({ HITBOX_BODY, HitscanMode::PREFER });

		//prefer head
		if (g_menu.main.aimbot.general_preferhead.get(0))
			m_hitboxes.push_back({ HITBOX_HEAD, HitscanMode::PREFER });

		if (g_menu.main.aimbot.general_preferhead.get(1) && prefer_head)
			m_hitboxes.push_back({ HITBOX_HEAD, HitscanMode::PREFER });

		if (g_menu.main.aimbot.general_preferhead.get(2) && !(record->m_mode != Resolver::Modes::RESOLVE_NONE && record->m_mode != Resolver::Modes::RESOLVE_WALK && record->m_mode != Resolver::Modes::RESOLVE_STOPPED_MOVING))
			m_hitboxes.push_back({ HITBOX_HEAD, HitscanMode::PREFER });

		if (g_menu.main.aimbot.general_preferhead.get(3) && !(record->m_pred_flags & FL_ONGROUND))
			m_hitboxes.push_back({ HITBOX_HEAD, HitscanMode::PREFER });

		bool only{ false };

		// only, always.
		if (g_menu.main.aimbot.general_forcebaim.get(0)) {
			only = true;
			m_hitboxes.push_back({ HITBOX_BODY, HitscanMode::PREFER });
		}

		// only, health.
		if (g_menu.main.aimbot.general_forcebaim.get(1) && m_player->m_iHealth() <= (int)g_menu.main.aimbot.general_forcehealth.get()) {
			only = true;
			m_hitboxes.push_back({ HITBOX_BODY, HitscanMode::PREFER });
		}

		// only, fake.
		if (g_menu.main.aimbot.general_forcebaim.get(2) && record->m_mode != Resolver::Modes::RESOLVE_NONE && record->m_mode != Resolver::Modes::RESOLVE_WALK) {
			only = true;
			m_hitboxes.push_back({ HITBOX_BODY, HitscanMode::PREFER });
		}

		// only, in air.
		if (g_menu.main.aimbot.general_forcebaim.get(3) && !(record->m_pred_flags & FL_ONGROUND)) {
			only = true;
			m_hitboxes.push_back({ HITBOX_BODY, HitscanMode::PREFER });
		}

		// only, on key.
		if (g_input.GetKeyState(g_menu.main.aimbot.baim_key.get())) {
			only = true;
			m_hitboxes.push_back({ HITBOX_BODY, HitscanMode::NORMAL });
			m_hitboxes.push_back({ HITBOX_CHEST, HitscanMode::NORMAL });
			m_hitboxes.push_back({ HITBOX_PELVIS, HitscanMode::NORMAL });
		}

		// only baim conditions have been met.
		// do not insert more hitboxes.
		if (only)
			return;

		std::vector< size_t > hitbox{ g_menu.main.aimbot.general_hitboxes.GetActiveIndices() };
		if (hitbox.empty())
			return;

		for (const auto& h : hitbox) {
			// head.
			if (h == 0)
				m_hitboxes.push_back({ HITBOX_HEAD, HitscanMode::NORMAL });

			// upper chest
			if (h == 1) {
				m_hitboxes.push_back({ HITBOX_UPPER_CHEST, HitscanMode::NORMAL });
			}

			// chest.
			if (h == 2) {
				m_hitboxes.push_back({ HITBOX_THORAX, HitscanMode::NORMAL });
				m_hitboxes.push_back({ HITBOX_CHEST, HitscanMode::NORMAL });
			}

			// stomach.
			if (h == 3) {
				m_hitboxes.push_back({ HITBOX_BODY, HitscanMode::NORMAL });
			}

			// pelvis.
			if (h == 4) {
				m_hitboxes.push_back({ HITBOX_PELVIS, HitscanMode::NORMAL });
			}

			// arms.
			if (h == 5) {
				m_hitboxes.push_back({ HITBOX_L_UPPER_ARM, HitscanMode::NORMAL });
				m_hitboxes.push_back({ HITBOX_R_UPPER_ARM, HitscanMode::NORMAL });
			}

			// legs.
			if (h == 6) {
				m_hitboxes.push_back({ HITBOX_L_THIGH, HitscanMode::NORMAL });
				m_hitboxes.push_back({ HITBOX_R_THIGH, HitscanMode::NORMAL });
				m_hitboxes.push_back({ HITBOX_L_CALF, HitscanMode::NORMAL });
				m_hitboxes.push_back({ HITBOX_R_CALF, HitscanMode::NORMAL });
			}

			if (h == 7) {
				m_hitboxes.push_back({ HITBOX_L_FOOT, HitscanMode::NORMAL });
				m_hitboxes.push_back({ HITBOX_R_FOOT, HitscanMode::NORMAL });
			}
		}
	}
}

void Aimbot::init() {
	// clear old targets.
	m_targets.clear();

	m_target = nullptr;
	m_aim = vec3_t{ };
	m_angle = ang_t{ };
	m_damage = 0.f;
	m_record = nullptr;
	m_stop = false;

	m_best_dist = std::numeric_limits< float >::max();
	m_best_fov = 180.f + 1.f;
	m_best_damage = 0.f;
	m_best_hp = 100 + 1;
	m_best_lag = std::numeric_limits< float >::max();
	m_best_height = std::numeric_limits< float >::max();
}

void Aimbot::StripAttack() {
	if (g_cl.m_weapon_id == REVOLVER)
		g_cl.m_cmd->m_buttons &= ~IN_ATTACK2;

	else
		g_cl.m_cmd->m_buttons &= ~IN_ATTACK;
}

void Aimbot::think() {
	// do all startup routines.
	init();

	// sanity.
	if (!g_cl.m_weapon)
		return;

	// no grenades or bomb.
	if (g_cl.m_weapon_type == WEAPONTYPE_GRENADE || g_cl.m_weapon_type == WEAPONTYPE_C4)
		return;

	if (!g_cl.m_weapon_fire)
		StripAttack();

	// we have no aimbot enabled.
	if (!g_menu.main.aimbot.enable.get())
		return;

	// animation silent aim, prevent the ticks with the shot in it to become the tick that gets processed.
	// we can do this by always choking the tick before we are able to shoot.
	bool revolver = g_cl.m_weapon_id == REVOLVER && g_cl.m_revolver_cock != 0;

	// one tick before being able to shoot.
	if (revolver && g_cl.m_revolver_cock > 0 && g_cl.m_revolver_cock == g_cl.m_revolver_query) {
		*g_cl.m_packet = false;
		return;
	}

	// we have a normal weapon or a non cocking revolver
	// choke if its the processing tick.
	if (g_cl.m_weapon_fire && !g_cl.m_lag && !revolver) {
		*g_cl.m_packet = false;
		StripAttack();
		return;
	}

	// no point in aimbotting if we cannot fire this tick.
	if (!g_cl.m_weapon_fire)
		return;

	// setup bones for all valid targets.
	for (int i{ 1 }; i <= g_csgo.m_globals->m_max_clients; ++i) {
		Player* player = g_csgo.m_entlist->GetClientEntity< Player* >(i);

		if (!IsValidTarget(player))
			continue;

		AimPlayer* data = &m_players[i - 1];
		if (!data)
			continue;

		// store player as potential target this tick.
		m_targets.emplace_back(data);
	}

	// run knifebot.
	if (g_cl.m_weapon_type == WEAPONTYPE_KNIFE && g_cl.m_weapon_id != ZEUS) {

		if (g_menu.main.aimbot.knifebot.get())
			knife();

		return;
	}

	// scan available targets... if we even have any.
	find();

	// finally set data when shooting.
	apply();
}

void Aimbot::find() 
{
	std::cout << "aimbot find called\n";

	struct BestTarget_t { Player* player; vec3_t pos; float damage; LagRecord* record; };

	vec3_t       tmp_pos;
	float        tmp_damage;
	BestTarget_t best;
	best.player = nullptr;
	best.damage = -1.f;
	best.pos = vec3_t{ };
	best.record = nullptr;

	if (m_targets.empty())
		return;

	if (g_cl.m_weapon_id == ZEUS && !g_menu.main.aimbot.zeusbot.get())
		return;

	// iterate all targets.
	for (const auto& t : m_targets) {
		if (t->m_records.empty())
			continue;

		// this player broke lagcomp.
		// his bones have been resetup by our lagcomp.
		// therfore now only the front record is valid.
		if (g_menu.main.aimbot.lagfix.get() && g_lagcomp.StartPrediction(t)) {
			LagRecord* front = t->m_records.front().get();

			t->SetupHitboxes(front, false);
			if (t->m_hitboxes.empty())
				continue;

			// rip something went wrong..
			if (t->GetBestAimPosition(tmp_pos, tmp_damage, front) && SelectTarget(front, tmp_pos, tmp_damage)) {

				// if we made it so far, set shit.
				best.player = t->m_player;
				best.pos = tmp_pos;
				best.damage = tmp_damage;
				best.record = front;
			}
		}

		// player did not break lagcomp.
		// history aim is possible at this point.
		else {
			LagRecord* ideal = g_resolver.FindIdealRecord(t);
			if (!ideal)
				continue;

			t->SetupHitboxes(ideal, false);
			if (t->m_hitboxes.empty())
				continue;

			// try to select best record as target.
			if (t->GetBestAimPosition(tmp_pos, tmp_damage, ideal) && SelectTarget(ideal, tmp_pos, tmp_damage)) {
				// if we made it so far, set shit.
				best.player = t->m_player;
				best.pos = tmp_pos;
				best.damage = tmp_damage;
				best.record = ideal;
			}

			LagRecord* last = g_resolver.FindLastRecord(t);
			if (!last || last == ideal)
				continue;

			t->SetupHitboxes(last, true);
			if (t->m_hitboxes.empty())
				continue;

			// rip something went wrong..
			if (t->GetBestAimPosition(tmp_pos, tmp_damage, last) && SelectTarget(last, tmp_pos, tmp_damage)) {
				// if we made it so far, set shit.
				best.player = t->m_player;
				best.pos = tmp_pos;
				best.damage = tmp_damage;
				best.record = last;
			}
		}
	}

	// verify our target and set needed data.
	if (best.player && best.record) {
		// calculate aim angle.
		math::VectorAngles(best.pos - g_cl.m_shoot_pos, m_angle);

		// set member vars.
		m_target = best.player;
		m_aim = best.pos;
		m_damage = best.damage;
		m_record = best.record;

		// write data, needed for traces / etc.
		m_record->cache();

		// set autostop shit.
		m_stop = !(g_cl.m_buttons & IN_JUMP);

		bool can_hit_on_fd = !g_hvh.m_fakeduck || g_hvh.m_fakeduck && g_cl.m_local->m_flDuckAmount() == 0.f;

		bool on;

		if (g_cl.m_weapon_id == SSG08)
			on = g_menu.main.aimbot.scout_hitchanceval.get() && g_menu.main.misc.mode.get() == 0;
		else if (g_cl.m_weapon_id == G3SG1 || g_cl.m_weapon_id == SCAR20)
			on = g_menu.main.aimbot.auto_hitchanceval.get() && g_menu.main.misc.mode.get() == 0;
		else if (g_cl.m_weapon_id == AWP)
			on = g_menu.main.aimbot.awp_hitchanceval.get() && g_menu.main.misc.mode.get() == 0;
		else
			on = g_menu.main.aimbot.general_hitchanceval.get() && g_menu.main.misc.mode.get() == 0;

		bool hit = on && CheckHitchance(m_target, m_angle);

		// if we can scope.
		bool can_scope = !g_cl.m_local->m_bIsScoped() && (g_cl.m_weapon_id == AUG || g_cl.m_weapon_id == SG553 || g_cl.m_weapon_type == WEAPONTYPE_SNIPER_RIFLE);

		if (can_scope) {
			// always.
			if (g_menu.main.aimbot.zoom.get() == 1 && g_cl.m_local->GetActiveWeapon()->m_zoomLevel() != 2) {
				g_cl.m_cmd->m_buttons |= IN_ATTACK2;
				return;
			}

			// hitchance fail.
			else if (g_menu.main.aimbot.zoom.get() == 2 && on && !hit && g_cl.m_local->GetActiveWeapon()->m_zoomLevel() != 2) {
				g_cl.m_cmd->m_buttons |= IN_ATTACK2;
				return;
			}
		}

		if (hit || !on) {
			// right click attack.
			if (g_menu.main.misc.mode.get() == 1 && g_cl.m_weapon_id == REVOLVER)
				g_cl.m_cmd->m_buttons |= IN_ATTACK2;

			// left click attack.
			else
				g_cl.m_cmd->m_buttons |= IN_ATTACK;
		}
	}
	std::cout << "aimbot cant find anything\n";
}

bool Aimbot::CanHit(vec3_t start, vec3_t end, LagRecord* record, int box, studiohdr_t* hdr)
{
	if (!record || !record->m_player)
		return false;

	// backup player
	const auto backup_origin = record->m_player->m_vecOrigin();
	const auto backup_abs_origin = record->m_player->GetAbsOrigin();
	const auto backup_abs_angles = record->m_player->GetAbsAngles();
	const auto backup_obb_mins = record->m_player->m_vecMins();
	const auto backup_obb_maxs = record->m_player->m_vecMaxs();
	const auto backup_cache = record->m_player->m_iBoneCache();

	// always try to use our aimbot matrix first.
	auto matrix = record->m_bones;

	// this is basically for using a custom matrix.
	//if (in_shot)
		//matrix = bones;

	if (!matrix)
		return false;

	const model_t* model = record->m_player->GetModel();
	if (!model)
		return false;

	studiohdr_t* hdr2 = g_csgo.m_model_info->GetStudioModel(model);
	if (!hdr2)
		return false;

	mstudiohitboxset_t* set = hdr2->GetHitboxSet(record->m_player->m_nHitboxSet());
	if (!set)
		return false;

	mstudiobbox_t* bbox = set->GetHitbox(box);
	if (!bbox)
		return false;

	vec3_t min, max;
	const auto IsCapsule = bbox->m_radius != -1.f;

	if (IsCapsule) {
		math::VectorTransform(bbox->m_mins, matrix[bbox->m_bone], min);
		math::VectorTransform(bbox->m_maxs, matrix[bbox->m_bone], max);
		const auto dist = math::SegmentToSegment(start, end, min, max);

		if (dist < bbox->m_radius) {
			return true;
		}
	}
	else {
		CGameTrace tr;

		// setup trace data
		record->m_player->m_vecOrigin() = record->m_origin;
		record->m_player->SetAbsOrigin(record->m_origin);
		record->m_player->SetAbsAngles(record->m_abs_ang);
		record->m_player->m_vecMins() = record->m_mins;
		record->m_player->m_vecMaxs() = record->m_maxs;
		record->m_player->m_iBoneCache() = reinterpret_cast<matrix3x4_t**>(matrix);

		// setup ray and trace.
		g_csgo.m_engine_trace->ClipRayToEntity(Ray(start, end), MASK_SHOT, record->m_player, &tr);

		record->m_player->m_vecOrigin() = backup_origin;
		record->m_player->SetAbsOrigin(backup_abs_origin);
		record->m_player->SetAbsAngles(backup_abs_angles);
		record->m_player->m_vecMins() = backup_obb_mins;
		record->m_player->m_vecMaxs() = backup_obb_maxs;
		record->m_player->m_iBoneCache() = backup_cache;

		// check if we hit a valid player / hitgroup on the player and increment total hits.
		if (tr.m_entity == record->m_player && game::IsValidHitgroup(tr.m_hitgroup))
			return true;
	}

	return false;
}

bool Aimbot::CheckHitchance(Player* player, const ang_t& angle) {
	constexpr float HITCHANCE_MAX = 100.f;
	constexpr int   SEED_MAX = 255;

	vec3_t     start{ g_cl.m_shoot_pos }, end, fwd, right, up, dir, wep_spread;
	float      inaccuracy, spread;
	CGameTrace tr;
	size_t     total_hits{ },
		needed_general_hits{ (size_t)std::ceil((g_menu.main.aimbot.general_hitchanceval.get() * SEED_MAX) / HITCHANCE_MAX) },
		needed_pistol_hits{ (size_t)std::ceil((g_menu.main.aimbot.pistol_hitchanceval.get() * SEED_MAX) / HITCHANCE_MAX) },
		needed_heavy_hits{ (size_t)std::ceil((g_menu.main.aimbot.heavypistol_hitchanceval.get() * SEED_MAX) / HITCHANCE_MAX) },
		needed_scout_hits{ (size_t)std::ceil((g_menu.main.aimbot.scout_hitchanceval.get() * SEED_MAX) / HITCHANCE_MAX) },
		needed_auto_hits{ (size_t)std::ceil((g_menu.main.aimbot.auto_hitchanceval.get() * SEED_MAX) / HITCHANCE_MAX) },
		needed_awp_hits{ (size_t)std::ceil((g_menu.main.aimbot.awp_hitchanceval.get() * SEED_MAX) / HITCHANCE_MAX) },
		needed_zeus_hits{ (size_t)std::ceil((g_menu.main.aimbot.zeusbot_hitchance.get() * SEED_MAX) / HITCHANCE_MAX) };

	// get needed directional vectors.
	math::AngleVectors(angle, &fwd, &right, &up);

	// store off inaccuracy / spread ( these functions are quite intensive and we only need them once ).
	inaccuracy = g_cl.m_weapon->GetInaccuracy();
	spread = g_cl.m_weapon->GetSpread();

	// iterate all possible seeds.
	for (int i{ }; i <= SEED_MAX; ++i) {
		// get spread.
		wep_spread = g_cl.m_weapon->CalculateSpread(i, inaccuracy, spread);

		// get spread direction.
		dir = (fwd + (right * wep_spread.x) + (up * wep_spread.y)).normalized();

		// get end of trace.
		end = start + (dir * g_cl.m_weapon_info->m_range);

		// setup ray and trace.
		g_csgo.m_engine_trace->ClipRayToEntity(Ray(start, end), MASK_SHOT, player, &tr);

		// check if we hit a valid player / hitgroup on the player and increment total hits.
		if (tr.m_entity == player && game::IsValidHitgroup(tr.m_hitgroup))
			++total_hits;

		// we made it.
		if (g_cl.m_weapon_id == ZEUS)
		{
			if (total_hits >= needed_zeus_hits)
				return true;
		}
		else if (g_cl.m_weapon_id == GLOCK || g_cl.m_weapon_id == P2000 || g_cl.m_weapon_id == USPS || g_cl.m_weapon_id == ELITE || g_cl.m_weapon_id == P250 || g_cl.m_weapon_id == TEC9 || g_cl.m_weapon_id == CZ75A)
		{
			if (total_hits >= needed_pistol_hits)
				return true;
		}
		else if (g_cl.m_weapon_id == DEAGLE || g_cl.m_weapon_id == REVOLVER)
		{
			if (total_hits >= needed_heavy_hits)
				return true;
		}
		else if (g_cl.m_weapon_id == SSG08)
		{
			if (total_hits >= needed_scout_hits)
				return true;
		}
		else if (g_cl.m_weapon_id == G3SG1 || g_cl.m_weapon_id == SCAR20)
		{
			if (total_hits >= needed_auto_hits)
				return true;
		}
		else if (g_cl.m_weapon_id == AWP)
		{
			if (total_hits >= needed_awp_hits)
				return true;
		}
		else
		{
			if (total_hits >= needed_general_hits)
				return true;
		}

		int needed_hits[7] = { needed_general_hits, needed_pistol_hits, needed_heavy_hits, needed_scout_hits, needed_auto_hits, needed_awp_hits, needed_zeus_hits };

		// we cant make it anymore.
		if ((SEED_MAX - i + total_hits) < math::minimum(needed_hits, 7))
			return false;
	}

	return false;
}

bool IsVisible(Entity* pEntity, vec3_t vEnd) {
	CGameTrace tr;
	CTraceFilterWorldOnly filter;

	g_csgo.m_engine_trace->TraceRay(Ray(g_cl.m_local->GetEyePosition(), vEnd), 0x4600400B, &filter, &tr);

	return (tr.m_entity == pEntity || tr.m_fraction == 1.0f);
}

// mutiny v2
float GetFov(const ang_t& viewAngle, const ang_t& aimAngle) {
	vec3_t ang, aim;

	math::AngleVectors(viewAngle, &aim);
	math::AngleVectors(aimAngle, &ang);

	return math::rad_to_deg(acos(aim.dot(ang) / aim.length_sqr()));
}

bool AimPlayer::SetupHitboxPoints(LagRecord* record, BoneArray* bones, int index, std::vector< vec3_t >& points) {
	// reset points.
	points.clear();

	const model_t* model = m_player->GetModel();
	if (!model)
		return false;

	studiohdr_t* hdr = g_csgo.m_model_info->GetStudioModel(model);
	if (!hdr)
		return false;

	mstudiohitboxset_t* set = hdr->GetHitboxSet(m_player->m_nHitboxSet());
	if (!set)
		return false;

	mstudiobbox_t* bbox = set->GetHitbox(index);
	if (!bbox)
		return false;

	// get hitbox scales.
	float headscale = 0.f, bodyscale = 0.f;

	if (g_cl.m_weapon_id == GLOCK || g_cl.m_weapon_id == P2000 || g_cl.m_weapon_id == USPS || g_cl.m_weapon_id == ELITE || g_cl.m_weapon_id == P250 || g_cl.m_weapon_id == TEC9 || g_cl.m_weapon_id == CZ75A)
		headscale = g_menu.main.aimbot.pistol_headscale.get() / 100.f;
	else if (g_cl.m_weapon_id == DEAGLE || g_cl.m_weapon_id == REVOLVER)
		headscale = g_menu.main.aimbot.heavypistol_headscale.get() / 100.f;
	else if (g_cl.m_weapon_id == SSG08)
		headscale = g_menu.main.aimbot.scout_headscale.get() / 100.f;
	else if (g_cl.m_weapon_id == G3SG1 || g_cl.m_weapon_id == SCAR20)
		headscale = g_menu.main.aimbot.auto_headscale.get() / 100.f;
	else if (g_cl.m_weapon_id == AWP)
		headscale = g_menu.main.aimbot.awp_headscale.get() / 100.f;
	else
		headscale = g_menu.main.aimbot.general_headscale.get() / 100.f;

	// big inair fix.
	if ((~record->m_pred_flags) & FL_ONGROUND)
		headscale = 0.7f;

	if (g_cl.m_weapon_id == GLOCK || g_cl.m_weapon_id == P2000 || g_cl.m_weapon_id == USPS || g_cl.m_weapon_id == ELITE || g_cl.m_weapon_id == P250 || g_cl.m_weapon_id == TEC9 || g_cl.m_weapon_id == CZ75A)
		bodyscale = g_menu.main.aimbot.pistol_bodyscale.get() / 100.f;
	else if (g_cl.m_weapon_id == DEAGLE || g_cl.m_weapon_id == REVOLVER)
		bodyscale = g_menu.main.aimbot.heavypistol_bodyscale.get() / 100.f;
	else if (g_cl.m_weapon_id == SSG08)
		bodyscale = g_menu.main.aimbot.scout_bodyscale.get() / 100.f;
	else if (g_cl.m_weapon_id == G3SG1 || g_cl.m_weapon_id == SCAR20)
		bodyscale = g_menu.main.aimbot.auto_bodyscale.get() / 100.f;
	else if (g_cl.m_weapon_id == AWP)
		bodyscale = g_menu.main.aimbot.awp_bodyscale.get() / 100.f;
	else
		bodyscale = g_menu.main.aimbot.general_bodyscale.get() / 100.f;

	// these indexes represent boxes.
	if (bbox->m_radius <= 0.f) {
		// references: 
		//      https://developer.valvesoftware.com/wiki/Rotation_Tutorial
		//      CBaseAnimating::GetHitboxBonePosition
		//      CBaseAnimating::DrawServerHitboxes

		// convert rotation angle to a matrix.
		matrix3x4_t rot_matrix;
		g_csgo.AngleMatrix(bbox->m_angle, rot_matrix);

		// apply the rotation to the entity input space (local).
		matrix3x4_t matrix;
		math::ConcatTransforms(bones[bbox->m_bone], rot_matrix, matrix);

		// extract origin from matrix.
		vec3_t origin = matrix.GetOrigin();

		// compute raw center point.
		vec3_t center = (bbox->m_mins + bbox->m_maxs) / 2.f;

		// the feet hiboxes have a side, heel and the toe.
		if (index == HITBOX_R_FOOT || index == HITBOX_L_FOOT) {
			float d1 = (bbox->m_mins.z - center.z) * 0.875f;

			// invert.
			if (index == HITBOX_L_FOOT)
				d1 *= -1.f;

			// side is more optimal then center.
			points.push_back({ center.x, center.y, center.z + d1 });

			if (g_cl.m_weapon_id == GLOCK || g_cl.m_weapon_id == P2000 || g_cl.m_weapon_id == USPS || g_cl.m_weapon_id == ELITE || g_cl.m_weapon_id == P250 || g_cl.m_weapon_id == TEC9 || g_cl.m_weapon_id == CZ75A)
			{
				if (g_menu.main.aimbot.pistol_multipoint.get(3)) {
					// get point offset relative to center point
					// and factor in hitbox scale.
					float d2 = (bbox->m_mins.x - center.x) * headscale;
					float d3 = (bbox->m_maxs.x - center.x) * headscale;

					// heel.
					points.push_back({ center.x + d2, center.y, center.z });

					// toe.
					points.push_back({ center.x + d3, center.y, center.z });
				}
			}
			else if (g_cl.m_weapon_id == DEAGLE || g_cl.m_weapon_id == REVOLVER)
			{
				if (g_menu.main.aimbot.heavypistol_multipoint.get(3)) {
					// get point offset relative to center point
					// and factor in hitbox scale.
					float d2 = (bbox->m_mins.x - center.x) * headscale;
					float d3 = (bbox->m_maxs.x - center.x) * headscale;

					// heel.
					points.push_back({ center.x + d2, center.y, center.z });

					// toe.
					points.push_back({ center.x + d3, center.y, center.z });
				}
			}
			else if (g_cl.m_weapon_id == SSG08)
			{
				if (g_menu.main.aimbot.scout_multipoint.get(3)) {
					// get point offset relative to center point
					// and factor in hitbox scale.
					float d2 = (bbox->m_mins.x - center.x) * headscale;
					float d3 = (bbox->m_maxs.x - center.x) * headscale;

					// heel.
					points.push_back({ center.x + d2, center.y, center.z });

					// toe.
					points.push_back({ center.x + d3, center.y, center.z });
				}
			}
			else if (g_cl.m_weapon_id == G3SG1 || g_cl.m_weapon_id == SCAR20)
			{
				if (g_menu.main.aimbot.auto_multipoint.get(3)) {
					// get point offset relative to center point
					// and factor in hitbox scale.
					float d2 = (bbox->m_mins.x - center.x) * headscale;
					float d3 = (bbox->m_maxs.x - center.x) * headscale;

					// heel.
					points.push_back({ center.x + d2, center.y, center.z });

					// toe.
					points.push_back({ center.x + d3, center.y, center.z });
				}
			}
			else if (g_cl.m_weapon_id == AWP)
			{
				if (g_menu.main.aimbot.awp_multipoint.get(3)) {
					// get point offset relative to center point
					// and factor in hitbox scale.
					float d2 = (bbox->m_mins.x - center.x) * headscale;
					float d3 = (bbox->m_maxs.x - center.x) * headscale;

					// heel.
					points.push_back({ center.x + d2, center.y, center.z });

					// toe.
					points.push_back({ center.x + d3, center.y, center.z });
				}
			}
			else
			{
				if (g_menu.main.aimbot.general_multipoint.get(3)) {
					// get point offset relative to center point
					// and factor in hitbox scale.
					float d2 = (bbox->m_mins.x - center.x) * headscale;
					float d3 = (bbox->m_maxs.x - center.x) * headscale;

					// heel.
					points.push_back({ center.x + d2, center.y, center.z });

					// toe.
					points.push_back({ center.x + d3, center.y, center.z });
				}
			}
		}

		// nothing to do here we are done.
		if (points.empty())
			return false;

		// rotate our bbox points by their correct angle
		// and convert our points to world space.
		for (auto& p : points) {
			// VectorRotate.
			// rotate point by angle stored in matrix.
			p = { p.dot(matrix[0]), p.dot(matrix[1]), p.dot(matrix[2]) };

			// transform point to world space.
			p += origin;
		}
	}

	// these hitboxes are capsules.
	else {
		// factor in the pointscale.
		float r = bbox->m_radius * headscale;
		float br = bbox->m_radius * bodyscale;

		// compute raw center point.
		vec3_t center = (bbox->m_mins + bbox->m_maxs) / 2.f;

		if (g_cl.m_weapon_id == GLOCK || g_cl.m_weapon_id == P2000 || g_cl.m_weapon_id == USPS || g_cl.m_weapon_id == ELITE || g_cl.m_weapon_id == P250 || g_cl.m_weapon_id == TEC9 || g_cl.m_weapon_id == CZ75A)
		{
			// head has 5 points.
			if (index == HITBOX_HEAD) {
				// add center.
				points.push_back(center);

				if (g_menu.main.aimbot.pistol_multipoint.get(0)) {
					// rotation matrix 45 degrees.
					// https://math.stackexchange.com/questions/383321/rotating-x-y-points-45-degrees
					// std::cos( deg_to_rad( 45.f ) )
					constexpr float rotation = 0.70710678f;

					// top/back 45 deg.
					// this is the best spot to shoot at.
					points.push_back({ bbox->m_maxs.x + (rotation * r), bbox->m_maxs.y + (-rotation * r), bbox->m_maxs.z });

					// right.
					points.push_back({ bbox->m_maxs.x, bbox->m_maxs.y, bbox->m_maxs.z + r });

					// left.
					points.push_back({ bbox->m_maxs.x, bbox->m_maxs.y, bbox->m_maxs.z - r });

					// back.
					points.push_back({ bbox->m_maxs.x, bbox->m_maxs.y - r, bbox->m_maxs.z });

					// get animstate ptr.
					CCSGOPlayerAnimState* state = record->m_player->m_PlayerAnimState();

					// add this point only under really specific circumstances.
					// if we are standing still and have the lowest possible pitch pose.
					if (state && record->m_anim_velocity.length() <= 0.1f && record->m_eye_angles.x <= state->m_min_pitch) {

						// bottom point.
						points.push_back({ bbox->m_maxs.x - r, bbox->m_maxs.y, bbox->m_maxs.z });
					}
				}
			}

			// body has 5 points.
			else if (index == HITBOX_BODY) {
				// center.
				points.push_back(center);

				// back.
				if (g_menu.main.aimbot.pistol_multipoint.get(2))
					points.push_back({ center.x, bbox->m_maxs.y - br, center.z });
			}

			else if (index == HITBOX_PELVIS || index == HITBOX_UPPER_CHEST) {
				// back.
				points.push_back({ center.x, bbox->m_maxs.y - r, center.z });
			}

			// other stomach/chest hitboxes have 2 points.
			else if (index == HITBOX_THORAX || index == HITBOX_CHEST) {
				// add center.
				points.push_back(center);

				// add extra point on back.
				if (g_menu.main.aimbot.pistol_multipoint.get(1))
					points.push_back({ center.x, bbox->m_maxs.y - r, center.z });
			}

			else if (index == HITBOX_R_CALF || index == HITBOX_L_CALF) {
				// add center.
				points.push_back(center);

				// half bottom.
				if (g_menu.main.aimbot.pistol_multipoint.get(3))
					points.push_back({ bbox->m_maxs.x - (bbox->m_radius / 2.f), bbox->m_maxs.y, bbox->m_maxs.z });
			}

			else if (index == HITBOX_R_THIGH || index == HITBOX_L_THIGH) {
				// add center.
				points.push_back(center);
			}

			// arms get only one point.
			else if (index == HITBOX_R_UPPER_ARM || index == HITBOX_L_UPPER_ARM) {
				// elbow.
				points.push_back({ bbox->m_maxs.x + bbox->m_radius, center.y, center.z });
			}

			// nothing left to do here.
			if (points.empty())
				return false;

			// transform capsule points.
			for (auto& p : points)
				math::VectorTransform(p, bones[bbox->m_bone], p);
		}
		else if (g_cl.m_weapon_id == DEAGLE || g_cl.m_weapon_id == REVOLVER)
		{
			// head has 5 points.
			if (index == HITBOX_HEAD) {
				// add center.
				points.push_back(center);

				if (g_menu.main.aimbot.heavypistol_multipoint.get(0)) {
					// rotation matrix 45 degrees.
					// https://math.stackexchange.com/questions/383321/rotating-x-y-points-45-degrees
					// std::cos( deg_to_rad( 45.f ) )
					constexpr float rotation = 0.70710678f;

					// top/back 45 deg.
					// this is the best spot to shoot at.
					points.push_back({ bbox->m_maxs.x + (rotation * r), bbox->m_maxs.y + (-rotation * r), bbox->m_maxs.z });

					// right.
					points.push_back({ bbox->m_maxs.x, bbox->m_maxs.y, bbox->m_maxs.z + r });

					// left.
					points.push_back({ bbox->m_maxs.x, bbox->m_maxs.y, bbox->m_maxs.z - r });

					// back.
					points.push_back({ bbox->m_maxs.x, bbox->m_maxs.y - r, bbox->m_maxs.z });

					// get animstate ptr.
					CCSGOPlayerAnimState* state = record->m_player->m_PlayerAnimState();

					// add this point only under really specific circumstances.
					// if we are standing still and have the lowest possible pitch pose.
					if (state && record->m_anim_velocity.length() <= 0.1f && record->m_eye_angles.x <= state->m_min_pitch) {

						// bottom point.
						points.push_back({ bbox->m_maxs.x - r, bbox->m_maxs.y, bbox->m_maxs.z });
					}
				}
			}

			// body has 5 points.
			else if (index == HITBOX_BODY) {
				// center.
				points.push_back(center);

				// back.
				if (g_menu.main.aimbot.heavypistol_multipoint.get(2))
					points.push_back({ center.x, bbox->m_maxs.y - br, center.z });
			}

			else if (index == HITBOX_PELVIS || index == HITBOX_UPPER_CHEST) {
				// back.
				points.push_back({ center.x, bbox->m_maxs.y - r, center.z });
			}

			// other stomach/chest hitboxes have 2 points.
			else if (index == HITBOX_THORAX || index == HITBOX_CHEST) {
				// add center.
				points.push_back(center);

				// add extra point on back.
				if (g_menu.main.aimbot.heavypistol_multipoint.get(1))
					points.push_back({ center.x, bbox->m_maxs.y - r, center.z });
			}

			else if (index == HITBOX_R_CALF || index == HITBOX_L_CALF) {
				// add center.
				points.push_back(center);

				// half bottom.
				if (g_menu.main.aimbot.heavypistol_multipoint.get(3))
					points.push_back({ bbox->m_maxs.x - (bbox->m_radius / 2.f), bbox->m_maxs.y, bbox->m_maxs.z });
			}

			else if (index == HITBOX_R_THIGH || index == HITBOX_L_THIGH) {
				// add center.
				points.push_back(center);
			}

			// arms get only one point.
			else if (index == HITBOX_R_UPPER_ARM || index == HITBOX_L_UPPER_ARM) {
				// elbow.
				points.push_back({ bbox->m_maxs.x + bbox->m_radius, center.y, center.z });
			}

			// nothing left to do here.
			if (points.empty())
				return false;

			// transform capsule points.
			for (auto& p : points)
				math::VectorTransform(p, bones[bbox->m_bone], p);
		}
		else if (g_cl.m_weapon_id == SSG08)
		{
			// head has 5 points.
			if (index == HITBOX_HEAD) {
				// add center.
				points.push_back(center);

				if (g_menu.main.aimbot.scout_multipoint.get(0)) {
					// rotation matrix 45 degrees.
					// https://math.stackexchange.com/questions/383321/rotating-x-y-points-45-degrees
					// std::cos( deg_to_rad( 45.f ) )
					constexpr float rotation = 0.70710678f;

					// top/back 45 deg.
					// this is the best spot to shoot at.
					points.push_back({ bbox->m_maxs.x + (rotation * r), bbox->m_maxs.y + (-rotation * r), bbox->m_maxs.z });

					// right.
					points.push_back({ bbox->m_maxs.x, bbox->m_maxs.y, bbox->m_maxs.z + r });

					// left.
					points.push_back({ bbox->m_maxs.x, bbox->m_maxs.y, bbox->m_maxs.z - r });

					// back.
					points.push_back({ bbox->m_maxs.x, bbox->m_maxs.y - r, bbox->m_maxs.z });

					// get animstate ptr.
					CCSGOPlayerAnimState* state = record->m_player->m_PlayerAnimState();

					// add this point only under really specific circumstances.
					// if we are standing still and have the lowest possible pitch pose.
					if (state && record->m_anim_velocity.length() <= 0.1f && record->m_eye_angles.x <= state->m_min_pitch) {

						// bottom point.
						points.push_back({ bbox->m_maxs.x - r, bbox->m_maxs.y, bbox->m_maxs.z });
					}
				}
			}

			// body has 5 points.
			else if (index == HITBOX_BODY) {
				// center.
				points.push_back(center);

				// back.
				if (g_menu.main.aimbot.scout_multipoint.get(2))
					points.push_back({ center.x, bbox->m_maxs.y - br, center.z });
			}

			else if (index == HITBOX_PELVIS || index == HITBOX_UPPER_CHEST) {
				// back.
				points.push_back({ center.x, bbox->m_maxs.y - r, center.z });
			}

			// other stomach/chest hitboxes have 2 points.
			else if (index == HITBOX_THORAX || index == HITBOX_CHEST) {
				// add center.
				points.push_back(center);

				// add extra point on back.
				if (g_menu.main.aimbot.scout_multipoint.get(1))
					points.push_back({ center.x, bbox->m_maxs.y - r, center.z });
			}

			else if (index == HITBOX_R_CALF || index == HITBOX_L_CALF) {
				// add center.
				points.push_back(center);

				// half bottom.
				if (g_menu.main.aimbot.scout_multipoint.get(3))
					points.push_back({ bbox->m_maxs.x - (bbox->m_radius / 2.f), bbox->m_maxs.y, bbox->m_maxs.z });
			}

			else if (index == HITBOX_R_THIGH || index == HITBOX_L_THIGH) {
				// add center.
				points.push_back(center);
			}

			// arms get only one point.
			else if (index == HITBOX_R_UPPER_ARM || index == HITBOX_L_UPPER_ARM) {
				// elbow.
				points.push_back({ bbox->m_maxs.x + bbox->m_radius, center.y, center.z });
			}

			// nothing left to do here.
			if (points.empty())
				return false;

			// transform capsule points.
			for (auto& p : points)
				math::VectorTransform(p, bones[bbox->m_bone], p);
		}
		else if (g_cl.m_weapon_id == G3SG1 || g_cl.m_weapon_id == SCAR20)
		{
			// head has 5 points.
			if (index == HITBOX_HEAD) {
				// add center.
				points.push_back(center);

				if (g_menu.main.aimbot.auto_multipoint.get(0)) {
					// rotation matrix 45 degrees.
					// https://math.stackexchange.com/questions/383321/rotating-x-y-points-45-degrees
					// std::cos( deg_to_rad( 45.f ) )
					constexpr float rotation = 0.70710678f;

					// top/back 45 deg.
					// this is the best spot to shoot at.
					points.push_back({ bbox->m_maxs.x + (rotation * r), bbox->m_maxs.y + (-rotation * r), bbox->m_maxs.z });

					// right.
					points.push_back({ bbox->m_maxs.x, bbox->m_maxs.y, bbox->m_maxs.z + r });

					// left.
					points.push_back({ bbox->m_maxs.x, bbox->m_maxs.y, bbox->m_maxs.z - r });

					// back.
					points.push_back({ bbox->m_maxs.x, bbox->m_maxs.y - r, bbox->m_maxs.z });

					// get animstate ptr.
					CCSGOPlayerAnimState* state = record->m_player->m_PlayerAnimState();

					// add this point only under really specific circumstances.
					// if we are standing still and have the lowest possible pitch pose.
					if (state && record->m_anim_velocity.length() <= 0.1f && record->m_eye_angles.x <= state->m_min_pitch) {

						// bottom point.
						points.push_back({ bbox->m_maxs.x - r, bbox->m_maxs.y, bbox->m_maxs.z });
					}
				}
			}

			// body has 5 points.
			else if (index == HITBOX_BODY) {
				// center.
				points.push_back(center);

				// back.
				if (g_menu.main.aimbot.auto_multipoint.get(2))
					points.push_back({ center.x, bbox->m_maxs.y - br, center.z });
			}

			else if (index == HITBOX_PELVIS || index == HITBOX_UPPER_CHEST) {
				// back.
				points.push_back({ center.x, bbox->m_maxs.y - r, center.z });
			}

			// other stomach/chest hitboxes have 2 points.
			else if (index == HITBOX_THORAX || index == HITBOX_CHEST) {
				// add center.
				points.push_back(center);

				// add extra point on back.
				if (g_menu.main.aimbot.auto_multipoint.get(1))
					points.push_back({ center.x, bbox->m_maxs.y - r, center.z });
			}

			else if (index == HITBOX_R_CALF || index == HITBOX_L_CALF) {
				// add center.
				points.push_back(center);

				// half bottom.
				if (g_menu.main.aimbot.auto_multipoint.get(3))
					points.push_back({ bbox->m_maxs.x - (bbox->m_radius / 2.f), bbox->m_maxs.y, bbox->m_maxs.z });
			}

			else if (index == HITBOX_R_THIGH || index == HITBOX_L_THIGH) {
				// add center.
				points.push_back(center);
			}

			// arms get only one point.
			else if (index == HITBOX_R_UPPER_ARM || index == HITBOX_L_UPPER_ARM) {
				// elbow.
				points.push_back({ bbox->m_maxs.x + bbox->m_radius, center.y, center.z });
			}

			// nothing left to do here.
			if (points.empty())
				return false;

			// transform capsule points.
			for (auto& p : points)
				math::VectorTransform(p, bones[bbox->m_bone], p);
		}
		else if (g_cl.m_weapon_id == AWP)
		{
			// head has 5 points.
			if (index == HITBOX_HEAD) {
				// add center.
				points.push_back(center);

				if (g_menu.main.aimbot.awp_multipoint.get(0)) {
					// rotation matrix 45 degrees.
					// https://math.stackexchange.com/questions/383321/rotating-x-y-points-45-degrees
					// std::cos( deg_to_rad( 45.f ) )
					constexpr float rotation = 0.70710678f;

					// top/back 45 deg.
					// this is the best spot to shoot at.
					points.push_back({ bbox->m_maxs.x + (rotation * r), bbox->m_maxs.y + (-rotation * r), bbox->m_maxs.z });

					// right.
					points.push_back({ bbox->m_maxs.x, bbox->m_maxs.y, bbox->m_maxs.z + r });

					// left.
					points.push_back({ bbox->m_maxs.x, bbox->m_maxs.y, bbox->m_maxs.z - r });

					// back.
					points.push_back({ bbox->m_maxs.x, bbox->m_maxs.y - r, bbox->m_maxs.z });

					// get animstate ptr.
					CCSGOPlayerAnimState* state = record->m_player->m_PlayerAnimState();

					// add this point only under really specific circumstances.
					// if we are standing still and have the lowest possible pitch pose.
					if (state && record->m_anim_velocity.length() <= 0.1f && record->m_eye_angles.x <= state->m_min_pitch) {

						// bottom point.
						points.push_back({ bbox->m_maxs.x - r, bbox->m_maxs.y, bbox->m_maxs.z });
					}
				}
			}

			// body has 5 points.
			else if (index == HITBOX_BODY) {
				// center.
				points.push_back(center);

				// back.
				if (g_menu.main.aimbot.awp_multipoint.get(2))
					points.push_back({ center.x, bbox->m_maxs.y - br, center.z });
			}

			else if (index == HITBOX_PELVIS || index == HITBOX_UPPER_CHEST) {
				// back.
				points.push_back({ center.x, bbox->m_maxs.y - r, center.z });
			}

			// other stomach/chest hitboxes have 2 points.
			else if (index == HITBOX_THORAX || index == HITBOX_CHEST) {
				// add center.
				points.push_back(center);

				// add extra point on back.
				if (g_menu.main.aimbot.awp_multipoint.get(1))
					points.push_back({ center.x, bbox->m_maxs.y - r, center.z });
			}

			else if (index == HITBOX_R_CALF || index == HITBOX_L_CALF) {
				// add center.
				points.push_back(center);

				// half bottom.
				if (g_menu.main.aimbot.awp_multipoint.get(3))
					points.push_back({ bbox->m_maxs.x - (bbox->m_radius / 2.f), bbox->m_maxs.y, bbox->m_maxs.z });
			}

			else if (index == HITBOX_R_THIGH || index == HITBOX_L_THIGH) {
				// add center.
				points.push_back(center);
			}

			// arms get only one point.
			else if (index == HITBOX_R_UPPER_ARM || index == HITBOX_L_UPPER_ARM) {
				// elbow.
				points.push_back({ bbox->m_maxs.x + bbox->m_radius, center.y, center.z });
			}

			// nothing left to do here.
			if (points.empty())
				return false;

			// transform capsule points.
			for (auto& p : points)
				math::VectorTransform(p, bones[bbox->m_bone], p);
		}
		else
		{
			// head has 5 points.
			if (index == HITBOX_HEAD) {
				// add center.
				points.push_back(center);

				if (g_menu.main.aimbot.general_multipoint.get(0)) {
					// rotation matrix 45 degrees.
					// https://math.stackexchange.com/questions/383321/rotating-x-y-points-45-degrees
					// std::cos( deg_to_rad( 45.f ) )
					constexpr float rotation = 0.70710678f;

					// top/back 45 deg.
					// this is the best spot to shoot at.
					points.push_back({ bbox->m_maxs.x + (rotation * r), bbox->m_maxs.y + (-rotation * r), bbox->m_maxs.z });

					// right.
					points.push_back({ bbox->m_maxs.x, bbox->m_maxs.y, bbox->m_maxs.z + r });

					// left.
					points.push_back({ bbox->m_maxs.x, bbox->m_maxs.y, bbox->m_maxs.z - r });

					// back.
					points.push_back({ bbox->m_maxs.x, bbox->m_maxs.y - r, bbox->m_maxs.z });

					// get animstate ptr.
					CCSGOPlayerAnimState* state = record->m_player->m_PlayerAnimState();

					// add this point only under really specific circumstances.
					// if we are standing still and have the lowest possible pitch pose.
					if (state && record->m_anim_velocity.length() <= 0.1f && record->m_eye_angles.x <= state->m_min_pitch) {

						// bottom point.
						points.push_back({ bbox->m_maxs.x - r, bbox->m_maxs.y, bbox->m_maxs.z });
					}
				}
			}

			// body has 5 points.
			else if (index == HITBOX_BODY) {
				// center.
				points.push_back(center);

				// back.
				if (g_menu.main.aimbot.general_multipoint.get(2))
					points.push_back({ center.x, bbox->m_maxs.y - br, center.z });
			}

			else if (index == HITBOX_PELVIS || index == HITBOX_UPPER_CHEST) {
				// back.
				points.push_back({ center.x, bbox->m_maxs.y - r, center.z });
			}

			// other stomach/chest hitboxes have 2 points.
			else if (index == HITBOX_THORAX || index == HITBOX_CHEST) {
				// add center.
				points.push_back(center);

				// add extra point on back.
				if (g_menu.main.aimbot.general_multipoint.get(1))
					points.push_back({ center.x, bbox->m_maxs.y - r, center.z });
			}

			else if (index == HITBOX_R_CALF || index == HITBOX_L_CALF) {
				// add center.
				points.push_back(center);

				// half bottom.
				if (g_menu.main.aimbot.general_multipoint.get(3))
					points.push_back({ bbox->m_maxs.x - (bbox->m_radius / 2.f), bbox->m_maxs.y, bbox->m_maxs.z });
			}

			else if (index == HITBOX_R_THIGH || index == HITBOX_L_THIGH) {
				// add center.
				points.push_back(center);
			}

			// arms get only one point.
			else if (index == HITBOX_R_UPPER_ARM || index == HITBOX_L_UPPER_ARM) {
				// elbow.
				points.push_back({ bbox->m_maxs.x + bbox->m_radius, center.y, center.z });
			}

			// nothing left to do here.
			if (points.empty())
				return false;

			// transform capsule points.
			for (auto& p : points)
				math::VectorTransform(p, bones[bbox->m_bone], p);
		}
	}

	return true;
}

bool AimPlayer::GetBestAimPosition(vec3_t& aim, float& damage, LagRecord* record) {
	bool                  done, pen;
	float                 dmg, pendmg;
	HitscanData_t         scan;
	std::vector< vec3_t > points;

	// get player hp.
	int hp = std::min(100, m_player->m_iHealth());

	if (g_cl.m_weapon_id == ZEUS) {
		dmg = hp;
		pen = false;
	}
	// pistol
	else if (g_cl.m_weapon_id == GLOCK || g_cl.m_weapon_id == P2000 || g_cl.m_weapon_id == USPS || g_cl.m_weapon_id == ELITE || g_cl.m_weapon_id == P250 || g_cl.m_weapon_id == TEC9 || g_cl.m_weapon_id == CZ75A && !g_input.GetKeyState(g_menu.main.aimbot.override_dmg_value.get()))
	{
		dmg = g_menu.main.aimbot.pistol_mindmg.get();
		if (dmg > 100)
			dmg = hp + (dmg - 100);

		pendmg = g_menu.main.aimbot.pistol_awallmindmg.get();
		if (pendmg > 100)
			pendmg = hp + (pendmg - 100);

		pen = g_menu.main.aimbot.pistol_autowall.get();
	}
	// heavypistol
	else if (g_cl.m_weapon_id == DEAGLE || g_cl.m_weapon_id == REVOLVER && !g_input.GetKeyState(g_menu.main.aimbot.override_dmg_value.get()))
	{
		dmg = g_menu.main.aimbot.heavypistol_mindmg.get();
		if (dmg > 100)
			dmg = hp + (dmg - 100);

		pendmg = g_menu.main.aimbot.heavypistol_awallmindmg.get();
		if (pendmg > 100)
			pendmg = hp + (pendmg - 100);

		pen = g_menu.main.aimbot.heavypistol_autowall.get();
	}
	// scout
	else if (g_cl.m_weapon_id == SSG08 && !g_input.GetKeyState(g_menu.main.aimbot.override_dmg_value.get()))
	{
		dmg = g_menu.main.aimbot.scout_mindmg.get();
		if (dmg > 100)
			dmg = hp + (dmg - 100);

		pendmg = g_menu.main.aimbot.scout_awallmindmg.get();
		if (pendmg > 100)
			pendmg = hp + (pendmg - 100);

		pen = g_menu.main.aimbot.scout_autowall.get();
	}
	// auto
	else if (g_cl.m_weapon_id == G3SG1 || g_cl.m_weapon_id == SCAR20 && !g_input.GetKeyState(g_menu.main.aimbot.override_dmg_value.get()))
	{
		dmg = g_menu.main.aimbot.auto_mindmg.get();
		if (dmg > 100)
			dmg = hp + (dmg - 100);

		pendmg = g_menu.main.aimbot.auto_awallmindmg.get();
		if (pendmg > 100)
			pendmg = hp + (pendmg - 100);

		pen = g_menu.main.aimbot.auto_autowall.get();
	}
	// awp
	else if (g_cl.m_weapon_id == AWP && !g_input.GetKeyState(g_menu.main.aimbot.override_dmg_value.get()))
	{
		dmg = g_menu.main.aimbot.awp_mindmg.get();
		if (dmg > 100)
			dmg = hp + (dmg - 100);

		pendmg = g_menu.main.aimbot.awp_awallmindmg.get();
		if (pendmg > 100)
			pendmg = hp + (pendmg - 100);

		pen = g_menu.main.aimbot.awp_autowall.get();
	}
	else if (g_input.GetKeyState(g_menu.main.aimbot.override_dmg_value.get()))
	{
		dmg = 1;
		pendmg = 1;
		if (g_cl.m_weapon_id == GLOCK || g_cl.m_weapon_id == P2000 || g_cl.m_weapon_id == USPS || g_cl.m_weapon_id == ELITE || g_cl.m_weapon_id == P250 || g_cl.m_weapon_id == TEC9 || g_cl.m_weapon_id == CZ75A)
			pen = g_menu.main.aimbot.pistol_autowall.get();
		else if (g_cl.m_weapon_id == DEAGLE || g_cl.m_weapon_id == REVOLVER)
			pen = g_menu.main.aimbot.heavypistol_autowall.get();
		else if (g_cl.m_weapon_id == SSG08)
			pen = g_menu.main.aimbot.scout_autowall.get();
		else if (g_cl.m_weapon_id == G3SG1 || g_cl.m_weapon_id == SCAR20)
			pen = g_menu.main.aimbot.auto_autowall.get();
		else if (g_cl.m_weapon_id == AWP)
			pen = g_menu.main.aimbot.awp_autowall.get();
	}
	else
	{
		dmg = g_menu.main.aimbot.general_mindmg.get();
		if (dmg > 100)
			dmg = hp + (dmg - 100);

		pendmg = g_menu.main.aimbot.general_awallmindmg.get();
		if (pendmg > 100)
			pendmg = hp + (pendmg - 100);

		pen = g_menu.main.aimbot.general_autowall.get();
	}

	// write all data of this record l0l.
	record->cache();

	// iterate hitboxes.
	for (const auto& it : m_hitboxes) {
		done = false;

		// setup points on hitbox.
		if (!SetupHitboxPoints(record, record->m_bones, it.m_index, points))
			continue;

		// iterate points on hitbox.
		for (const auto& point : points) {
			penetration::PenetrationInput_t in;

			in.m_damage = dmg;
			in.m_damage_pen = pendmg;
			in.m_can_pen = pen;
			in.m_target = m_player;
			in.m_from = g_cl.m_local;
			in.m_pos = point;

			// ignore mindmg.
			if (it.m_mode == HitscanMode::LETHAL || it.m_mode == HitscanMode::LETHAL2)
				in.m_damage = in.m_damage_pen = 1.f;

			penetration::PenetrationOutput_t out;

			// we can hit p!
			if (penetration::run(&in, &out)) {

				// nope we did not hit head..
				if (it.m_index == HITBOX_HEAD && out.m_hitgroup != HITGROUP_HEAD)
					continue;

				// prefered hitbox, just stop now.
				if (it.m_mode == HitscanMode::PREFER)
					done = true;

				// this hitbox requires lethality to get selected, if that is the case.
				// we are done, stop now.
				else if (it.m_mode == HitscanMode::LETHAL && out.m_damage >= m_player->m_iHealth())
					done = true;

				// 2 shots will be sufficient to kill.
				else if (it.m_mode == HitscanMode::LETHAL2 && (out.m_damage * 2.f) >= m_player->m_iHealth())
					done = true;

				// this hitbox has normal selection, it needs to have more damage.
				else if (it.m_mode == HitscanMode::NORMAL) {
					// we did more damage.
					if (out.m_damage > scan.m_damage) {
						// save new best data.
						scan.m_damage = out.m_damage;
						scan.m_pos = point;

						// if the first point is lethal
						// screw the other ones.
						if (point == points.front() && out.m_damage >= m_player->m_iHealth())
							break;
					}
				}

				// we found a preferred / lethal hitbox.
				if (done) {
					// save new best data.
					scan.m_damage = out.m_damage;
					scan.m_pos = point;
					break;
				}
			}
		}

		// ghetto break out of outer loop.
		if (done)
			break;
	}

	// we found something that we can damage.
	// set out vars.
	if (scan.m_damage > 0.f) {
		aim = scan.m_pos;
		damage = scan.m_damage;
		return true;
	}

	return false;
}

bool Aimbot::SelectTarget(LagRecord* record, const vec3_t& aim, float damage) {
	float dist, height;
	int   hp;

	switch (g_menu.main.aimbot.selection.get()) {

		// distance.
	case 0:
		dist = (record->m_pred_origin - g_cl.m_shoot_pos).length();

		if (dist < m_best_dist) {
			m_best_dist = dist;
			return true;
		}

		break;

		// damage.
	case 1:
		if (damage > m_best_damage) {
			m_best_damage = damage;
			return true;
		}

		break;

		// lowest hp.
	case 2:
		// fix for retarded servers?
		hp = std::min(100, record->m_player->m_iHealth());

		if (hp < m_best_hp) {
			m_best_hp = hp;
			return true;
		}

		break;

		// least lag.
	case 3:
		if (record->m_lag < m_best_lag) {
			m_best_lag = record->m_lag;
			return true;
		}

		break;

		// height.
	case 4:
		height = record->m_pred_origin.z - g_cl.m_local->m_vecOrigin().z;

		if (height < m_best_height) {
			m_best_height = height;
			return true;
		}

		break;

	default:
		return false;
	}

	return false;
}

void Aimbot::apply() {
	int hitbox;
	bool attack, attack2;

	// attack states.
	attack = (g_cl.m_cmd->m_buttons & IN_ATTACK);
	attack2 = (g_cl.m_weapon_id == REVOLVER && g_cl.m_cmd->m_buttons & IN_ATTACK2);

	// ensure we're attacking.
	if (attack || attack2) {
		// choke every shot.
		if (!g_hvh.m_fakeduck)
			*g_cl.m_packet = true;

		if (m_target) {
			// make sure to aim at un-interpolated data.
			// do this so BacktrackEntity selects the exact record.
			if (m_record && !m_record->m_broke_lc)
				g_cl.m_cmd->m_tick = game::TIME_TO_TICKS(m_record->m_sim_time + g_cl.m_lerp);

			// set angles to target.
			g_cl.m_cmd->m_view_angles = m_angle;

			// store matrix for shot record chams
			g_visuals.AddMatrix(m_target, m_target->bone_cache().base());

			// if not silent aim, apply the viewangles.
			if (!g_menu.main.aimbot.silent.get())
				g_csgo.m_engine->SetViewAngles(m_angle);

			if (g_menu.main.aimbot.debugaim.get())
				g_visuals.DrawHitboxMatrix(m_record, g_menu.main.aimbot.debugaimcol.get(), 5.f);
		}

		// nospread.
		if (g_menu.main.aimbot.nospread.get() && g_menu.main.misc.mode.get() == 1)
			NoSpread();

		// norecoil.
		if (g_menu.main.aimbot.norecoil.get())
			g_cl.m_cmd->m_view_angles -= g_cl.m_local->m_aimPunchAngle() * g_csgo.weapon_recoil_scale->GetFloat();

		// store fired shot.
		g_shots.OnShotFire(m_target ? m_target : nullptr, m_target ? m_damage : -1.f, g_cl.m_weapon_info->m_bullets, m_target ? m_record : nullptr);

		// set that we fired.
		g_cl.m_shot = true;
	}
}

void Aimbot::NoSpread() {
	bool    attack2;
	vec3_t  spread, forward, right, up, dir;

	// revolver state.
	attack2 = (g_cl.m_weapon_id == REVOLVER && (g_cl.m_cmd->m_buttons & IN_ATTACK2));

	// get spread.
	spread = g_cl.m_weapon->CalculateSpread(g_cl.m_cmd->m_random_seed, attack2);

	// compensate.
	g_cl.m_cmd->m_view_angles -= { -math::rad_to_deg(std::atan(spread.length_2d())), 0.f, math::rad_to_deg(std::atan2(spread.x, spread.y)) };
}
