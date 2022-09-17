#include "includes.h"
#include "hitsounds.h"
#include <urlmon.h>
#include <mmsystem.h>

bool didhiti;

#pragma comment(lib, "urlmon.lib")
#pragma comment(lib, "Winmm.lib")
#pragma comment( lib, "Iphlpapi.lib" )

Shots g_shots{ };

void Shots::OnFrameStage()
{
	CGameTrace trace;

	if (!g_cl.m_processing || m_shots.empty()) {
		if (!m_shots.empty())
			m_shots.clear();

		return;
	}

	for (auto it = m_shots.begin(); it != m_shots.end();) {
		if (it->m_time + 1.f < g_csgo.m_globals->m_curtime)
			it = m_shots.erase(it);
		else
			it = next(it);
	}



	for (auto it = m_shots.begin(); it != m_shots.end();) {
		if (it->m_matched && !it->m_hurt) {

			Player* target = it->m_target;
			if (!target) {
				it = m_shots.erase(it);
				continue;
			}

			// not gonna bother anymore.
			if (!target->alive()) {
				it = m_shots.erase(it);
				continue;
			}

			vec3_t start = it->m_pos;

			vec3_t dir = (it->m_server_pos - start).normalized();

			vec3_t end = start + dir * start.dist_to(it->m_record->m_origin) * 6.6f;

			g_csgo.m_engine_trace->ClipRayToEntity(Ray(start, end), MASK_SHOT, target, &trace);

			std::string name;

			player_info_t info;

			name = std::string(info.m_name).substr(0, 24);

			struct ShotMatch_t { float delta; ShotRecord* shot; };
			ShotMatch_t match{};
			ShotRecord* shot = match.shot;
			size_t mode = it->m_record->m_mode;
			float semen = shot->m_record->m_eye_angles.y;

			std::string balls = std::to_string(semen);


			//	if (!g_csgo.m_engine->GetPlayerInfo(target->index(), &info)) // poopoo
			//		return;

				// we processed this shot, let's delete it.
			it = m_shots.erase(it);
		}
		else {
			it = next(it);
		}

	}
}

void Shots::OnShotFire(Player* target, float damage, int bullets, LagRecord* record) {

	// iterate all bullets in this shot.
	for (int i{ }; i < bullets; ++i) {
		// setup new shot data.
		ShotRecord shot;
		shot.m_target = target;
		shot.m_record = record;
		shot.m_time = game::TICKS_TO_TIME(g_cl.m_local->m_nTickBase());
		shot.m_lat = g_cl.m_latency;
		shot.m_damage = damage;
		shot.m_matched = false;
		shot.m_hurt = false;
		shot.m_confirmed = false;
		shot.m_pos = g_cl.m_local->GetShootPosition();

		// we are not shooting manually.
		// and this is the first bullet, only do this once.
		if (target && i == 0) {
			// increment total shots on this player.
			AimPlayer* data = &g_aimbot.m_players[target->index() - 1];
			if (data)
				++data->m_shots;

			auto matrix = record->m_bones;

			if (matrix)
				shot.m_matrix = matrix;

		}

		// add to tracks.
		m_shots.push_front(shot);
	}

	// no need to keep an insane amount of shots.
	while (m_shots.size() > 128)
		m_shots.pop_back();
}

void Shots::OnImpact(IGameEvent* evt) {
	int        attacker;
	vec3_t     pos, dir, start, end;
	float      time;
	CGameTrace trace;

	// screw this.
	if (!evt || !g_cl.m_local)
		return;

	// get attacker, if its not us, screw it.
	attacker = g_csgo.m_engine->GetPlayerForUserID(evt->m_keys->FindKey(HASH("userid"))->GetInt());
	if (attacker != g_csgo.m_engine->GetLocalPlayer())
		return;

	// decode impact coordinates and convert to vec3.
	pos = {
		evt->m_keys->FindKey(HASH("x"))->GetFloat(),
		evt->m_keys->FindKey(HASH("y"))->GetFloat(),
		evt->m_keys->FindKey(HASH("z"))->GetFloat()
	};

	// get prediction time at this point.
	time = game::TICKS_TO_TIME(g_cl.m_local->m_nTickBase());

	// add to visual impacts if we have features that rely on it enabled.
	// todo - dex; need to match shots for this to have proper GetShootPosition, don't really care to do it anymore.
	if (g_menu.main.visuals.impact_beams.get())
		m_vis_impacts.push_back({ pos, g_cl.m_local->GetShootPosition(), g_cl.m_local->m_nTickBase() });

	// we did not take a shot yet.
	if (m_shots.empty())
		return;

	struct ShotMatch_t { float delta; ShotRecord* shot; };
	ShotMatch_t match;
	match.delta = std::numeric_limits< float >::max();
	match.shot = nullptr;

	// iterate all shots.
	for (auto& s : m_shots) {

		// this shot was already matched
		// with a 'bullet_impact' event.
		if (s.m_matched)
			continue;

		// add the latency to the time when we shot.
		// to predict when we would receive this event.
		float predicted = s.m_time + s.m_lat;

		// get the delta between the current time
		// and the predicted arrival time of the shot.
		float delta = std::abs(time - predicted);

		// fuck this.
		if (delta > 1.f)
			continue;

		// store this shot as being the best for now.
		if (delta < match.delta) {
			match.delta = delta;
			match.shot = &s;
		}
	}

	// no valid shotrecord was found.
	ShotRecord* shot = match.shot;
	if (!shot)
		return;

	// this shot was matched.
	shot->m_matched = true;
	shot->m_server_pos = pos;

	// create new impact instance that we can match with a player hurt.
	ImpactRecord impact;
	impact.m_shot = shot;
	impact.m_tick = g_cl.m_local->m_nTickBase();
	impact.m_pos = pos;

	// add to track.
	m_impacts.push_front(impact);

	// no need to keep an insane amount of impacts.
	while (m_impacts.size() > 128)
		m_impacts.pop_back();

	// nospread mode.
	if (g_menu.main.config.mode.get() == 1)
		return;

	// not in nospread mode, see if the shot missed due to spread.
	Player* target = shot->m_target;
	if (!target)
		return;

	// not gonna bother anymore.
	if (!target->alive())
		return;

	AimPlayer* data = &g_aimbot.m_players[target->index() - 1];
	if (!data)
		return;

	// this record was deleted already.
	if (!shot->m_record->m_bones)
		return;

	// we are going to alter this player.
	// store all his og data.
	BackupRecord backup;
	backup.store(target);

	// write historical matrix of the time that we shot
	// into the games bone cache, so we can trace against it.
	shot->m_record->cache();

	// start position of trace is where we took the shot.
	start = shot->m_pos;

	// the impact pos contains the spread from the server
	// which is generated with the server seed, so this is where the bullet
	// actually went, compute the direction of this from where the shot landed
	// and from where we actually took the shot.
	dir = (pos - start).normalized();

	// get end pos by extending direction forward.
	// todo; to do this properly should save the weapon range at the moment of the shot, cba..
	end = start + (dir * 8192.f);

	// intersect our historical matrix with the path the shot took.
	g_csgo.m_engine_trace->ClipRayToEntity(Ray(start, end), MASK_SHOT, target, &trace);

	float semen = shot->m_record->m_eye_angles.y;

	std::string balls = std::to_string(semen);

	std::string name;

	player_info_t info;

	name = std::string(info.m_name).substr(0, 24);

	// we did not hit jackshit, or someone else.
	if (!trace.m_entity || !trace.m_entity->IsPlayer() || trace.m_entity != target) {
		g_notify.add(XOR("shot missed due to spread\n"));
	}
	// we should have 100% hit this player..
	// this is a miss due to wrong angles.
	else if (trace.m_entity == target) {
		size_t mode = shot->m_record->m_mode;

		// if we miss a shot on body update.
		// we can chose to stop shooting at them.
		if (mode == Resolver::Modes::RESOLVE_BODY) {
			g_notify.add(tfm::format(XOR("[Resolver info: LBY update | Angle Shot: %s]\n"), balls));
		}

		else if (mode == Resolver::Modes::RESOLVE_LASTMOVE) {
			g_notify.add(tfm::format(XOR("[Resolver info: Last move | Angle Shot: %s]\n"), balls));
		}

		else if (mode == Resolver::Modes::RESOLVE_STOPPED_MOVING) {
			g_notify.add(tfm::format(XOR("[Resolver info: Stoped Moving | Angle Shot: %s]\n"), balls));
		}

		//else if (mode == Resolver::Modes::RESOLVE_FREESTAND) {
		//	g_notify.add(tfm::format(XOR("[Resolver info: Freestanding | Angle Shot: %s]\n"), balls));
	//	}

		else if (mode == Resolver::Modes::RESOLVE_BRUTEFORCE) {
			g_notify.add(tfm::format(XOR("[Resolver info: Bruteforce | Angle Shot: %s]\n"), balls));
		}

		else if (mode == Resolver::Modes::RESOLVE_AIR) {
			g_notify.add(tfm::format(XOR("[Resolver info Air | Angle Shot: %s]\n"), balls));
		}

		else if (mode == Resolver::Modes::RESOLVE_WALK) {
			g_notify.add(tfm::format(XOR("[Resolver info: Walking | Angle Shot: %s]\n"), balls));
		}


		++data->m_missed_shots;
	}
	else
		iHit = true, ++data->m_missed_shots;

	// restore player to his original state.
	backup.restore(target);
}

void Shots::OnHurt(IGameEvent* evt) {
	int         attacker, victim, group, hp;
	float       damage;
	std::string name;

	if (!evt || !g_cl.m_local)
		return;

	attacker = g_csgo.m_engine->GetPlayerForUserID(evt->m_keys->FindKey(HASH("attacker"))->GetInt());
	victim = g_csgo.m_engine->GetPlayerForUserID(evt->m_keys->FindKey(HASH("userid"))->GetInt());

	// skip invalid player indexes.
	// should never happen? world entity could be attacker, or a nade that hits you.
	if (attacker < 1 || attacker > 64 || victim < 1 || victim > 64)
		return;

	// we were not the attacker or we hurt ourselves.
	else if (attacker != g_csgo.m_engine->GetLocalPlayer() || victim == g_csgo.m_engine->GetLocalPlayer())
		return;

	// get hitgroup.
	// players that get naded ( DMG_BLAST ) or stabbed seem to be put as HITGROUP_GENERIC.
	group = evt->m_keys->FindKey(HASH("hitgroup"))->GetInt();

	// invalid hitgroups ( note - dex; HITGROUP_GEAR isn't really invalid, seems to be set for hands and stuff? ).
	if (group == HITGROUP_GEAR)
		return;

	// get the player that was hurt.
	Player* target = g_csgo.m_entlist->GetClientEntity< Player* >(victim);
	if (!target)
		return;

	// get player info.
	player_info_t info;
	if (!g_csgo.m_engine->GetPlayerInfo(victim, &info))
		return;

	// get player name;
	name = std::string(info.m_name).substr(0, 24);

	// get damage reported by the server.
	damage = (float)evt->m_keys->FindKey(HASH("dmg_health"))->GetInt();

	// get remaining hp.
	hp = evt->m_keys->FindKey(HASH("health"))->GetInt();

	// setup headshot marker
	if (group == HITGROUP_HEAD)
		iHeadshot = true;
	else
		iHeadshot = false;

	// hitmarker.
	if (g_menu.main.misc.hitmarker.get()) {
		g_visuals.m_hit_duration = 2.f;
		g_visuals.m_hit_start = g_csgo.m_globals->m_curtime;
		g_visuals.m_hit_end = g_visuals.m_hit_start + g_visuals.m_hit_duration;

		// bind to draw
		iHitDmg = damage;

		// get interpolated origin.
		iPlayerOrigin = target->GetAbsOrigin();

		// get hitbox bounds.
		// hehe boy
		target->ComputeHitboxSurroundingBox(&iPlayermins, &iPlayermaxs);

		// correct x and y coordinates.
		iPlayermins = { iPlayerOrigin.x, iPlayerOrigin.y, iPlayermins.z };
		iPlayermaxs = { iPlayerOrigin.x, iPlayerOrigin.y, iPlayermaxs.z + 8.f };

		switch (g_menu.main.misc.hitsounds.get() % 10) {
		case 0: break;
		case 1:
			g_csgo.m_sound->EmitAmbientSound(XOR("buttons/arena_switch_press_02.wav"), 1.f); break;
		case 2:
			PlaySoundA(reinterpret_cast<const char*>(millionvar), nullptr, SND_MEMORY | SND_ASYNC); break;
		case 3:
			PlaySoundA(reinterpret_cast<const char*>(cod), nullptr, SND_MEMORY | SND_ASYNC); break;
		case 4:
			PlaySoundA(reinterpret_cast<const char*>(primordial), nullptr, SND_MEMORY | SND_ASYNC); break;
		}
	}

	// print this shit.
	if (g_menu.main.misc.notifications.get(1)) {
		std::string out = tfm::format(XOR("Shot: %s | Hitbox %s | Damage: %i \n"), name, m_groups[group], (int)damage);
		g_notify.add(out);
		didhiti = true;
	}
	else {
		didhiti = false;
	}

	if (group == HITGROUP_GENERIC)
		return;

	// if we hit a player, mark vis impacts.
	if (!m_vis_impacts.empty()) {
		for (auto& i : m_vis_impacts) {
			if (i.m_tickbase == g_cl.m_local->m_nTickBase())
				i.m_hit_player = true;
		}
	}

	struct ShotMatch_t { float delta; ShotRecord* shot; };
	ShotMatch_t match;
	match.delta = std::numeric_limits< float >::max();
	match.shot = nullptr;

	// iterate all shots.
	for (auto& s : m_shots) {

		// this shot was already matched
		// with a 'player_hurt' event.
		if (s.m_hurt)
			continue;

		// add the latency to the time when we shot.
		// to predict when we would receive this event.
		float predicted = s.m_time + s.m_lat;

		// get the delta between the current time
		// and the predicted arrival time of the shot.
		float delta = std::abs(g_csgo.m_globals->m_curtime - predicted);

		// fuck this.
		if (delta > 1.f)
			continue;

		// store this shot as being the best for now.
		if (delta < match.delta) {
			match.delta = delta;
			match.shot = &s;
		}
	}

	// no valid shotrecord was found.
	ShotRecord* shot = match.shot;
	if (!shot)
		return;

	shot->m_hurt = true;


	// no impacts to match.
	if (m_impacts.empty())
		return;

	ImpactRecord* impact{ nullptr };



	// iterate stored impacts.
	for (auto& i : m_impacts) {

		// this impact doesnt match with our current hit.
		if (i.m_tick != g_cl.m_local->m_nTickBase())
			continue;

		// wrong player.
		if (i.m_shot->m_target != target)
			continue;

		// shit fond.
		impact = &i;
		break;
	}

	// no impact matched.
	if (!impact)
		return;

	// setup new data for hit track and push to hit track.
	HitRecord hit;
	hit.m_impact = impact;
	hit.m_group = group;
	hit.m_damage = damage;



	m_hits.push_front(hit);

	while (m_hits.size() > 128)
		m_hits.pop_back();

	AimPlayer* data = &g_aimbot.m_players[target->index() - 1];
	if (!data)
		return;

	//// we hit, reset missed shots counter.
	//data->m_missed_shots = 0;



	// if we hit head
	// shoot at this 5 more times.
	if (group == HITGROUP_HEAD) {
		LagRecord* record = hit.m_impact->m_shot->m_record;

		//switch( record->m_mode ) {
		//case Resolver::Modes::RESOLVE_STAND:
		//	data->m_prefer_stand.clear( );
		//	data->m_prefer_stand.push_front( math::NormalizedAngle( record->m_eye_angles.y - record->m_lbyt ) );
		//	break;

		//case Resolver::Modes::RESOLVE_AIR:
		//	if( g_menu.main.config.mode.get( ) == 1 ) {
		//		data->m_prefer_air.clear( );
		//		data->m_prefer_air.push_front( math::NormalizedAngle( record->m_eye_angles.y - record->m_away ) );
		//
		//		g_notify.add( tfm::format( "air hit %f\n", data->m_prefer_air.front( ) ) );
		//	}
		//
		//	break;

		//default:
		//	break;
		//}
	}
}

void Shots::OnFire(IGameEvent* evt)
{
	int attacker;

	// screw this.
	if (!evt || !g_cl.m_local)
		return;

	// get attacker, if its not us, screw it.
	attacker = g_csgo.m_engine->GetPlayerForUserID(evt->m_keys->FindKey(HASH("userid"))->GetInt());
	if (attacker != g_csgo.m_engine->GetLocalPlayer())
		return;

	struct ShotMatch_t { float delta; ShotRecord* shot; };
	ShotMatch_t match;
	match.delta = std::numeric_limits< float >::max();
	match.shot = nullptr;

	// iterate all shots.
	for (auto& s : m_shots) {

		// this shot was already matched
		// with a 'weapon_fire' event.
		if (s.m_confirmed)
			continue;

		// add the latency to the time when we shot.
		// to predict when we would receive this event.
		float predicted = s.m_time + s.m_lat;

		// get the delta between the current time
		// and the predicted arrival time of the shot.
		float delta = std::abs(g_csgo.m_globals->m_curtime - predicted);

		// fuck this.
		if (delta > 1.f)
			continue;

		// store this shot as being the best for now.
		if (delta < match.delta) {
			match.delta = delta;
			match.shot = &s;
		}
	}

	// no valid shotrecord was found.
	ShotRecord* shot = match.shot;
	if (!shot)
		return;

	shot->m_confirmed = true;
}