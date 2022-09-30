#pragma once

class Sequence {
public:
	float m_time;
	int   m_state;
	int   m_seq;

public:
	__forceinline Sequence() : m_time{}, m_state{}, m_seq{} {};
	__forceinline Sequence(float time, int state, int seq) : m_time{ time }, m_state{ state }, m_seq{ seq } {};
};

class NetPos {
public:
	float  m_time;
	vec3_t m_pos;

public:
	__forceinline NetPos() : m_time{}, m_pos{} {};
	__forceinline NetPos(float time, vec3_t pos) : m_time{ time }, m_pos{ pos } {};
};

template <typename t>
static t lerp2(float progress, const t& t1, const t& t2) {
	return t1 + (t2 - t1) * progress;
}

class user_cmd_t {
public:
	virtual ~user_cmd_t() { };

	int		m_cmd_nr; // 0x04 For matching server and client commands for debugging
	int		m_tick_count; // 0x08 the tick the client created this command
	vec3_t	m_viewangles; // 0x0C Player instantaneous view angles.
	vec3_t	m_aimdirection; // 0x18
	float	m_forwardmove; // 0x24
	float	m_sidemove; // 0x28
	float	m_upmove; // 0x2C
	int		m_buttons; // 0x30 Attack button states
	uint8_t m_impulse; // 0x34
	int		m_weaponselect; // 0x38 Current weapon id
	int		m_weaponsubtype; // 0x3C
	int		m_random_seed; // 0x40 For shared random functions
	short	m_mousedx; // 0x44 mouse accum in x from create move
	short	m_mousedy; // 0x46 mouse accum in y from create move
	bool	m_predicted; // 0x48 Client only, tracks whether we've predicted this command at least once
	vec3_t  headangles; // 0x49
	vec3_t	headoffset; // 0x55

	__forceinline user_cmd_t clamp(bool angles = true) {
		if (angles)
			m_viewangles.normalize();

		m_forwardmove = std::clamp(m_forwardmove, -450.f, 450.f);
		m_sidemove = std::clamp(m_sidemove, -450.f, 450.f);
		m_upmove = std::clamp(m_upmove, -450.f, 450.f);

		return *this;
	}
private:

};

class Client {
public:
	// hack thread.
	static ulong_t __stdcall init(void* arg);

	void StartMove(CUserCmd* cmd);
	void EndMove(CUserCmd* cmd);
	void BackupPlayers(bool restore);
	void DoMove();
	void DrawHUD();
	void ApplyUpdatedAnimation();
	void UpdateLocalAnimations();
	void UnlockHiddenConvars();
	void ClanTag();
	void Skybox();
	void SetAngles();
	void UpdateAnimations();
	void KillFeed();

	void OnPaint();
	void OnMapload();
	void OnTick(CUserCmd* cmd);

	// debugprint function.
	void print(const std::string text, ...);

	//bool CanFireWeapon();

	bool CanFireWeapon(float curtime);

	// check if we are able to fire this tick.
//	bool CanFireWeapon();
	void UpdateRevolverCock();
	void UpdateIncomingSequences();

public:
	// local player variables.
	Player* m_local;
	int get_fps() {
		return (int)std::round(1.f / g_csgo.m_globals->m_frametime);
	}

	// writeusr variables.
	int				 m_tick_to_shift;
	int				 m_tick_to_recharge;
	int              m_visual_shift_ticks;
	bool			 can_recharge;
	bool			 can_dt_shoot;
	bool			 m_charged;

	// cl_move vars
	int				 lastShiftedCmdNr;
	bool			 ignoreallcmds;
	int				 doubletapCharge;
	bool			 isShifting;
	int				 ticksToShift;
	bool             m_didFakeFlick;
	bool             isCharged;
	bool             m_fake_flick_side;

	//Player*          resolve_data;
	bool	         m_processing;
	int	             m_flags;
	vec3_t	         m_shoot_pos;
	bool	         m_player_fire;
	bool	         m_shot;
	bool	         m_old_shot;
	bool			 m_pressing_move;
	float            m_abs_yaw;
	float            m_poses[24];
	C_AnimationLayer m_layers[13];
	CCSGOPlayerAnimState* fake_state = 0;
	matrix3x4_t fake_matrix[128];
	ang_t last_real_viewangles;
	float			 m_left_thickness[64], m_right_thickness[64], m_at_target_angle[64];
	int	             m_tickbase;
	int	             m_fixed_tickbase;

	// active weapon variables.
	Weapon* m_weapon;
	int         m_weapon_id;
	WeaponInfo* m_weapon_info;
	int         m_weapon_type;
	bool        m_weapon_fire;

	// revolver variables.
	int	 m_revolver_cock;
	int	 m_revolver_query;
	bool m_revolver_fire;

	// general game varaibles.
	bool     m_round_end;
	Stage_t	 m_stage;
	int	     m_max_lag;
	int      m_lag;
	int	     m_old_lag;
	bool* m_packet;
	bool* m_final_packet;
	bool	 m_old_packet;
	float	 m_lerp;
	float    m_latency;
	int      m_latency_ticks;
	int      m_server_tick;
	int      m_arrival_tick;
	int      m_width, m_height;

	// animations.
	float  m_spawn_time;
	bool   m_update_local_animation;

	// usercommand variables.
	CUserCmd* m_cmd;
	int	      m_tick;
	int	      m_rate;
	int	      m_buttons;
	int       m_old_buttons;
	ang_t     m_view_angles;
	ang_t	  m_strafe_angles;
	vec3_t	  m_forward_dir;

	penetration::PenetrationOutput_t m_pen_data;

	std::deque< Sequence > m_sequences;
	std::deque< NetPos >   m_net_pos;

	// animation variables.
	ang_t  m_angle;
	ang_t  m_rotation;
	ang_t  m_radar;
	float  m_body;
	float  m_body_pred;
	float  m_speed;
	float  m_anim_time;
	float  m_anim_frame;
	bool   m_ground;
	bool   m_lagcomp;

	// hack username.
	std::string m_user;
};

extern Client g_cl;