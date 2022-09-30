#pragma once
#include "includes.h"
#include <deque>

class AdaptiveAngle {
public:
	float m_yaw;
	float m_dist;

public:
	// ctor.
	__forceinline AdaptiveAngle(float yaw, float penalty = 0.f) {
		// set yaw.
		m_yaw = math::NormalizedAngle(yaw);

		// init distance.
		m_dist = 0.f;

		// remove penalty.
		m_dist -= penalty;
	}
};

static constexpr long double M_PIRAD = 0.01745329251f;

class __declspec(align(16))VectorAligned : public vec3_t {
public:
	VectorAligned(float _x, float _y, float _z) {
		x = _x;
		y = _y;
		z = _z;
		w = 0;
	}

	VectorAligned() { }

	void operator=(const vec3_t& vOther) {
		x = vOther.x;
		y = vOther.y;
		z = vOther.z;
	}

	float w;
};

struct Ray_t
{
	VectorAligned m_Start;
	VectorAligned m_Delta;
	VectorAligned m_StartOffset;
	VectorAligned m_Extents;

	const matrix3x4_t* m_pWorldAxisTransform;

	bool m_IsRay;
	bool m_IsSwept;

	Ray_t() : m_pWorldAxisTransform(nullptr) { }

	void Init(const vec3_t& start, const vec3_t& end) {
		m_Delta = end - start;

		m_IsSwept = m_Delta.length_sqr() != 0;

		m_Extents = vec3_t(0.f, 0.f, 0.f);
		m_pWorldAxisTransform = nullptr;
		m_IsRay = true;

		m_StartOffset = vec3_t(0.f, 0.f, 0.f);
		m_Start = start;
	}

	void Init(const vec3_t& start, const vec3_t& end, const vec3_t& mins, const vec3_t& maxs) {
		m_Delta = end - start;

		m_pWorldAxisTransform = nullptr;
		m_IsSwept = m_Delta.length_sqr() != 0;

		m_Extents = maxs - mins;
		m_Extents *= 0.5f;
		m_IsRay = m_Extents.length_sqr() < 1e-6;

		m_StartOffset = mins + maxs;
		m_StartOffset *= 0.5f;
		m_Start = start + m_StartOffset;
		m_StartOffset *= -1.0f;
	}
};

enum AntiAimMode : size_t {
	STAND = 0,
	WALK,
	AIR,
};

class HVH {
public:
	size_t m_mode;
	int    m_pitch;
	int    m_yaw;
	float  m_jitter_range;
	float  m_rot_range;
	float  m_rot_speed;
	float  m_rand_update;
	int    m_dir;
	float  m_dir_custom;
	size_t m_base_angle;
	float  m_auto_time;

	bool   m_step_switch;
	bool   m_fakeduck;
	int    m_random_lag;
	float  m_next_random_update;
	float  m_random_angle;
	float  m_direction;
	float  m_auto;
	float  m_auto_dist;
	float  m_auto_last;
	float  m_view;
	bool   m_left, m_right, m_back;
private:
	std::deque< user_cmd_t > m_snapshot;

	user_cmd_t* get_last_cmd() {
		return m_snapshot.empty() ? nullptr : &m_snapshot.front();
	}

public:
	void IdealPitch();
	void fix_movement();
	void AntiAimPitch();
	void AutoDirection();
	void GetAntiAimDirection();
	bool DoEdgeAntiAim(Player* player, ang_t& out);
	void DistortionAntiAim(CUserCmd* cmd);
	void DoRealAntiAim();
	void DoFakeAntiAim();
	void AntiAim();
	void SendPacket();
};

extern HVH g_hvh;