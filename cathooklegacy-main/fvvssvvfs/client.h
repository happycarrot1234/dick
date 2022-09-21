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

class Client {
public:
	// hack thread.
	static ulong_t __stdcall init(void* arg);

	void StartMove(CUserCmd* cmd);
	void EndMove(CUserCmd* cmd);
	void BackupPlayers(bool restore);
	void DoMove();
	void DrawHUD();
	void UnlockHiddenConvars();
	void ClanTag();
	void Skybox();
	void UpdateInformation();
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
	float			 m_left_thickness[64], m_right_thickness[64], m_at_target_angle[64];
	int	             m_tickbase;
	int	             m_fixed_tickbase;
	matrix3x4_t		 fake_matrix[128];

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