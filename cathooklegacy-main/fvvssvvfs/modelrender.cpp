#include "includes.h"

void Hooks::DrawModelExecute( uintptr_t ctx, const DrawModelState_t& state, const ModelRenderInfo_t& info, matrix3x4_t* bone ) {

	int(__thiscall * OriginalFn)(void*, void* ctx, void* state, const ModelRenderInfo_t & pInfo, matrix3x4_t * pCustomBoneToWorld);
	// disable rendering of shadows.
	//if (strstr(info.m_model->m_name, XOR("shadow")) != nullptr && g_menu.main.visuals.removals.get(8))
		//return;

	// disable rendering of sleeves.
	if (strstr(info.m_model->m_name, XOR("sleeve")) != nullptr && g_menu.main.visuals.removals.get(6))
		return;

	// disables csgo's hdr effect.
	static auto hdr = g_csgo.m_material_system->FindMaterial(XOR("dev/blurfiltery_nohdr"), XOR("Other textures"));
	hdr->SetFlag(MATERIAL_VAR_NO_DRAW, true);

	// do chams.
	if( g_chams.DrawModel( ctx, state, info, bone ) ) {
		g_hooks.m_model_render.GetOldMethod< Hooks::DrawModelExecute_t >( IVModelRender::DRAWMODELEXECUTE )( this, ctx, state, info, bone );
	}

	// disable material force for next call.
	//g_csgo.m_studio_render->ForcedMaterialOverride( nullptr );
}