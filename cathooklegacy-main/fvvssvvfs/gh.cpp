#include "includes.h"
#include "gh.h"

void IEngineTrace::TraceLine(const vec3_t& src, const vec3_t& dst, int mask, IHandleEntity* entity, int collision_group, CGameTrace* trace) {
    static auto trace_filter_simple = pattern::find(g_csgo.m_client_dll, XOR("55 8B EC 83 E4 F0 83 EC 7C 56 52")) + 0x3D;

    std::uintptr_t filter[4] = { *reinterpret_cast<std::uintptr_t*>(trace_filter_simple), reinterpret_cast<std::uintptr_t>(entity), collision_group, 0 };

    TraceRay(Ray(src, dst), mask, reinterpret_cast<CTraceFilter*>(&filter), trace);
}

void IEngineTrace::TraceHull(const vec3_t& src, const vec3_t& dst, const vec3_t& mins, const vec3_t& maxs, int mask, IHandleEntity* entity, int collision_group, CGameTrace* trace) {
    static auto trace_filter_simple = pattern::find(g_csgo.m_client_dll, XOR("55 8B EC 83 E4 F0 83 EC 7C 56 52")) + 0x3D;

    std::uintptr_t filter[4] = { *reinterpret_cast<std::uintptr_t*>(trace_filter_simple), reinterpret_cast<std::uintptr_t>(entity), collision_group, 0 };

    TraceRay(Ray(src, dst, mins, maxs), mask, reinterpret_cast<CTraceFilter*>(&filter), trace);
}

void rotate_point(vec2_t& point, vec2_t origin, bool clockwise, float angle) {
    vec2_t delta = point - origin;
    vec2_t rotated;

    if (clockwise) {
        rotated = vec2_t(delta.x * cosf(angle) - delta.y * sinf(angle), delta.x * sinf(angle) + delta.y * cosf(angle));
    }
    else {
        rotated = vec2_t(delta.x * sinf(angle) - delta.y * cosf(angle), delta.x * cosf(angle) + delta.y * sinf(angle));
    }

    point = rotated + origin;
}

float& Player::get_creation_time() {
    return *reinterpret_cast<float*>(0x29B0);
}

void c_grenade_prediction::on_create_move(CUserCmd* cmd) {
    m_data = {};

    if (!g_cl.m_processing || !g_menu.main.visuals.tracers.get())
        return;

    const auto weapon = reinterpret_cast<Weapon*>(g_csgo.m_entlist->GetClientEntityFromHandle(g_cl.m_local->GetActiveWeapon()));
    if (!weapon || !weapon->m_bPinPulled() && weapon->m_fThrowTime() == 0.f)
        return;

    const auto weapon_data = weapon->GetWpnData();
    if (!weapon_data || weapon_data->m_weapon_type != 9)
        return;

    m_data.m_owner = g_cl.m_local;
    m_data.m_index = weapon->m_iItemDefinitionIndex();

    auto view_angles = cmd->m_view_angles;

    if (view_angles.x < -90.f) {
        view_angles.x += 360.f;
    }
    else if (view_angles.x > 90.f) {
        view_angles.x -= 360.f;
    }

    view_angles.x -= (90.f - std::fabsf(view_angles.x)) * 10.f / 90.f;

    auto direction = vec3_t();

   // math::AngleVectors(view_angles, direction);

    const auto throw_strength = std::clamp< float >(weapon->m_flThrowStrength(), 0.f, 1.f);
    const auto eye_pos = g_cl.m_shoot_pos;
    const auto src = vec3_t(eye_pos.x, eye_pos.y, eye_pos.z + (throw_strength * 12.f - 12.f));

    auto trace = CGameTrace();

    g_csgo.m_engine_trace->TraceHull(src, src + direction * 22.f, { -2.f, -2.f, -2.f }, { 2.f, 2.f, 2.f }, MASK_SOLID | CONTENTS_CURRENT_90, g_cl.m_local, COLLISION_GROUP_NONE, &trace);

    m_data.predict(trace.m_endpos - direction * 6.f, direction * (std::clamp< float >(weapon_data->m_throw_velocity * 0.9f, 15.f, 750.f) * (throw_strength * 0.7f + 0.3f)) + g_cl.m_local->m_vecVelocity() * 1.25f, g_csgo.m_globals->m_curtime, 0);
}

void DrawBeamPaw(vec3_t src, vec3_t end, Color color)
{
    BeamInfo_t beamInfo;
    beamInfo.m_vecStart = src;
    beamInfo.m_vecEnd = end;
    beamInfo.m_pszModelName = "sprites/purplelaser1.vmt";//sprites/purplelaser1.vmt
    beamInfo.m_pszHaloName = "sprites/purplelaser1.vmt";//sprites/purplelaser1.vmt
    beamInfo.m_flHaloScale = 0;//0
    beamInfo.m_flWidth = 2;//11
    beamInfo.m_flEndWidth = 2;//11
    beamInfo.m_flFadeLength = 1.0f;
    beamInfo.m_flAmplitude = 2.3;
    beamInfo.m_flBrightness = 255.f;
    beamInfo.m_flSpeed = 0.2f;
    beamInfo.m_nStartFrame = 0.0;
    beamInfo.m_flFrameRate = 0.0;
    beamInfo.m_flRed = color.r();
    beamInfo.m_flGreen = color.g();
    beamInfo.m_flBlue = color.b();
    beamInfo.m_nSegments = 2;//40
    beamInfo.m_bRenderable = true;
    beamInfo.m_flLife = 0.03f;


    Beam_t* myBeam = g_csgo.m_beams->CreateBeamPoints(beamInfo);
    if (myBeam)
        g_csgo.m_beams->DrawBeam(myBeam);
}

void draw_arc(int x, int y, int radius, int start_angle, int percent, int thickness, Color color)
{
    auto precision = (2 * 3.14159265358979323846) / 30;
    auto step = 3.14159265358979323846 / 180;
    auto inner = radius - thickness;
    auto end_angle = (start_angle + percent) * step;
    auto start_angles = (start_angle * 3.14159265358979323846) / 180;

    for (; radius > inner; --radius) {
        for (auto angle = start_angles; angle < end_angle; angle += precision) {
            auto cx = std::round(x + radius * std::cos(angle));
            auto cy = std::round(y + radius * std::sin(angle));

            auto cx2 = std::round(x + radius * std::cos(angle + precision));
            auto cy2 = std::round(y + radius * std::sin(angle + precision));

            render::line(cx, cy, cx2, cy2, color);
        }
    }
}

const char* index_to_grenade_name_icon(int index)
{
    switch (index)
    {
    case SMOKE: return "k"; break;
    case HEGRENADE: return "j"; break;
    case MOLOTOV:return "l"; break;
    case 48:return "n"; break;
    }
}

float  DEG2RAD(float deg) {
    return deg * 3.14159265358979323846 / 180;
}

bool c_grenade_prediction::data_t::draw() const
{
    if (!g_menu.main.visuals.grenade_warning.get())
        return false;

    if (m_path.size() <= 1u || g_csgo.m_globals->m_curtime >= m_expire_time)
        return false;

    int dist = g_cl.m_local->m_vecOrigin().dist_to(m_origin) / 12;

    auto prev_screen = vec2_t();
    auto prev_on_screen = render::WorldToScreen(std::get< vec3_t >(m_path.front()), prev_screen);

    for (auto i = 1u; i < m_path.size(); ++i) {
        auto cur_screen = vec2_t();
        const auto cur_on_screen = render::WorldToScreen(std::get< vec3_t >(m_path.at(i)), cur_screen);

        if (prev_on_screen && cur_on_screen) {

            if (g_menu.main.visuals.grenade_tracer_warning.get()) {
                DrawBeamPaw(std::get< vec3_t >(m_path.at(i - 1)), std::get< vec3_t >(m_path.at(i)), Color(255, 255, 255, 255)); // beamcolor
            }
        }

        prev_screen = cur_screen;
        prev_on_screen = cur_on_screen;
    }

    float percent = ((m_expire_time - g_csgo.m_globals->m_curtime) / game::TICKS_TO_TIME(m_tick));
    int alpha_damage = 0;

    if (m_index == HEGRENADE && dist <= 20) {
        alpha_damage = 50 - 255 * (dist / 20);
    }

    if ((m_index == MOLOTOV || m_index == FIREBOMB) && dist <= 15) {
        alpha_damage = 50 - 255 * (dist / 15);
    }

    //if (dist < 150) {
    render::circle(prev_screen.x, prev_screen.y - 10, 20, 360, Color(26, 26, 30, 200));
    draw_arc(prev_screen.x, prev_screen.y - 10, 20, 0, 360 * percent, 2, Color(255, 255, 255, 225));
    render::grenade.string(prev_screen.x - 5, prev_screen.y - 20, { 255,255,255,255 }, index_to_grenade_name_icon(m_index));
    // }

    auto is_on_screen = [](vec3_t origin, vec2_t& screen) -> bool
    {
        if (!render::WorldToScreen(origin, screen))
            return false;

        return (screen.x > 0 && screen.x < g_cl.m_width) && (g_cl.m_height > screen.y && screen.y > 0);
    };

    vec2_t screenPos;
    vec3_t vEnemyOrigin = m_origin;
    vec3_t vLocalOrigin = g_cl.m_local->GetAbsOrigin();
    if (!g_cl.m_local->alive())
        vLocalOrigin = g_csgo.m_input->m_camera_offset;

    if (!is_on_screen(vEnemyOrigin, screenPos))
    {
        const float wm = g_cl.m_width / 2, hm = g_cl.m_height / 2;
        vec3_t last_pos = std::get< vec3_t >(m_path.at(m_path.size() - 1));

        ang_t dir;

        g_csgo.m_engine->GetViewAngles(dir);

        float view_angle = dir.y;

        if (view_angle < 0)
            view_angle += 360;

        view_angle = DEG2RAD(view_angle);

        auto entity_angle = math::CalcAngle(vLocalOrigin, vEnemyOrigin);
        entity_angle.normalize();

        if (entity_angle.y < 0.f)
            entity_angle.y += 360.f;

        entity_angle.y = DEG2RAD(entity_angle.y);
        entity_angle.y -= view_angle;

        auto position = vec2_t(wm, hm);
        position.x -= std::clamp(vLocalOrigin.dist_to(vEnemyOrigin), 400.f, hm - 40);

        rotate_point(position, vec2_t(wm, hm), false, entity_angle.y);

        if (dist < 45) {
            render::circle(position.x, position.y - 10, 20, 360, Color(26, 26, 30, 200));
            draw_arc(position.x, position.y - 10, 20, 0, 360 * percent, 2, Color(255, 255, 255, 225));
            render::grenade.string(position.x - 5, position.y - 20, { 255,255,255,255 }, index_to_grenade_name_icon(m_index));
        }

    }
    return true;
}

void c_grenade_prediction::grenade_warning(Player* entity)
{
    auto& predicted_nades = g_grenades_pred.get_list();

    static auto last_server_tick = g_csgo.m_cl->m_server_tick;
    if (last_server_tick != g_csgo.m_cl->m_server_tick) {
        predicted_nades.clear();

        last_server_tick = g_csgo.m_cl->m_server_tick;
    }

    if (entity->dormant() || !g_menu.main.visuals.tracers.get())
        return;

    const auto client_class = entity->GetClientClass();
    if (!client_class || client_class->m_ClassID != 114 && client_class->m_ClassID != 9)
        return;

    if (client_class->m_ClassID == 9) {
        const auto model = entity->GetModel();
        if (!model)
            return;

        const auto studio_model = g_csgo.m_model_info->GetStudioModel(model);
        if (!studio_model || std::string_view(studio_model->m_name).find("fraggrenade") == std::string::npos)
            return;
    }

    const auto handle = entity->GetRefEHandle();

    if (entity->m_nExplodeEffectTickBegin()) {
        predicted_nades.erase(handle);
        return;
    }

    if (predicted_nades.find(handle) == predicted_nades.end()) {
        predicted_nades.emplace(std::piecewise_construct, std::forward_as_tuple(handle), std::forward_as_tuple(reinterpret_cast<Weapon*>(entity)->m_hThrower(), client_class->m_ClassID == 114 ? MOLOTOV : HEGRENADE, entity->m_vecOrigin(), reinterpret_cast<Player*>(entity)->m_vecVelocity(), entity->get_creation_time(), game::TIME_TO_TICKS(reinterpret_cast<Player*>(entity)->m_flSimulationTime() - entity->get_creation_time())));
    }

    if (predicted_nades.at(handle).draw())
        return;

    predicted_nades.erase(handle);
}