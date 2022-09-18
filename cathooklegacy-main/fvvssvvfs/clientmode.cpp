#include "includes.h"

bool Hooks::ShouldDrawParticles( ) {
	return g_hooks.m_client_mode.GetOldMethod< ShouldDrawParticles_t >( IClientMode::SHOULDDRAWPARTICLES )( this );
}

bool Hooks::ShouldDrawFog( ) {
	// remove fog.
	if( g_menu.main.visuals.removals.get(4))
		return false;

	return g_hooks.m_client_mode.GetOldMethod< ShouldDrawFog_t >( IClientMode::SHOULDDRAWFOG )( this );
}

void Hooks::OverrideView( CViewSetup* view ) {
	// damn son.
	g_cl.m_local = g_csgo.m_entlist->GetClientEntity< Player* >( g_csgo.m_engine->GetLocalPlayer( ) );

	// g_grenades.think( );
	g_visuals.ThirdpersonThink( );

    // call original.
	g_hooks.m_client_mode.GetOldMethod< OverrideView_t >( IClientMode::OVERRIDEVIEW )( this, view );

	//post proccess disable
	static auto post_cvar = g_csgo.m_cvar->FindVar(HASH("mat_postprocess_enable"));
	if (g_menu.main.visuals.removals.get(8))
		post_cvar->SetValue(0);
	else
		post_cvar->SetValue(1);

	// remove 3d skybox
	static auto r_3dsky = g_csgo.m_cvar->FindVar(HASH("r_3dsky"));
	if (g_menu.main.visuals.removals.get(7))
		r_3dsky->SetValue(0);
	else
		r_3dsky->SetValue(1);


	//sunset mode hopefully (a certain someone is gonna be happy)
	static auto cl_csm_rot_override = g_csgo.m_cvar->FindVar(HASH("cl_csm_rot_override"));
	static auto cl_csm_rot_x = g_csgo.m_cvar->FindVar(HASH("cl_csm_rot_x"));

	if (g_menu.main.visuals.customshadows.get()) {
		cl_csm_rot_override->SetValue(1);
		cl_csm_rot_x->SetValue(25);
	}
	else {
		cl_csm_rot_override->SetValue(0);
	}

    // remove scope edge blur.
	if( g_menu.main.visuals.removals.get(1) ) {
		if( g_cl.m_local && g_cl.m_local->m_bIsScoped( ) )
            view->m_edge_blur = 0;
	}
}

/*bool Hooks::CreateMove( float time, CUserCmd* cmd ) {
	Stack   stack;
	bool    ret;

	// let original run first.
	ret = g_hooks.m_client_mode.GetOldMethod< CreateMove_t >( IClientMode::CREATEMOVE )( this, time, cmd );

	// called from CInput::ExtraMouseSample -> return original.
	if( !cmd->m_command_number )
		return ret;

	// if we arrived here, called from -> CInput::CreateMove
	// call EngineClient::SetViewAngles according to what the original returns.
	if( ret )
		g_csgo.m_engine->SetViewAngles( cmd->m_view_angles );

	// random_seed isn't generated in ClientMode::CreateMove yet, we must set generate it ourselves.
	cmd->m_random_seed = g_csgo.MD5_PseudoRandom( cmd->m_command_number ) & 0x7fffffff;

	// get bSendPacket off the stack.
	g_cl.m_packet = stack.next( ).local( 0x1c ).as< bool* >( );

	// get bFinalTick off the stack.
	g_cl.m_final_packet = stack.next( ).local( 0x1b ).as< bool* >( );

	// invoke move function.
	g_cl.OnTick( cmd );

	return false;
}*/

bool Hooks::CreateMove(float time, CUserCmd* cmd) {
	Stack   stack;
	// called from CInput::ExtraMouseSample -> return original.
	if (!cmd || !cmd->m_command_number)
		return g_hooks.m_client_mode.GetOldMethod< CreateMove_t >(IClientMode::CREATEMOVE)(this, time, cmd);

	// if we arrived here, called from -> CInput::CreateMove
	// call EngineClient::SetViewAngles according to what the original returns.
	if (g_hooks.m_client_mode.GetOldMethod< CreateMove_t >(IClientMode::CREATEMOVE)(this, time, cmd))
		g_csgo.m_engine->SetViewAngles(cmd->m_view_angles);

	// random_seed isn't generated in ClientMode::CreateMove yet, we must set generate it ourselves.
	cmd->m_random_seed = g_csgo.MD5_PseudoRandom(cmd->m_command_number) & 0x7fffffff;

	// get bSendPacket off the stack.
	g_cl.m_packet = stack.next().local(0x1c).as< bool* >();

	// get bFinalTick off the stack.
	g_cl.m_final_packet = stack.next().local(0x1b).as< bool* >();

	if (g_gui.m_open) {
		// are we IN_ATTACK?
		if (cmd->m_buttons & IN_ATTACK)
			cmd->m_buttons &= ~IN_ATTACK;

		// are we IN_ATTACK2?
		if (cmd->m_buttons & IN_ATTACK2)
			cmd->m_buttons &= ~IN_ATTACK2;

		// are we IN_USE?
		if (cmd->m_buttons & IN_USE)
			cmd->m_buttons &= ~IN_USE;

	}

	// invoke move function.
	g_cl.OnTick(cmd);

	return false;
}

bool Hooks::DoPostScreenSpaceEffects( CViewSetup* setup ) {
	g_visuals.RenderGlow( );

	return g_hooks.m_client_mode.GetOldMethod< DoPostScreenSpaceEffects_t >( IClientMode::DOPOSTSPACESCREENEFFECTS )( this, setup );
}