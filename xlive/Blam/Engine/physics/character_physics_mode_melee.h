#pragma once

#define k_valid_real_epsilon 0.001f

// in practice the actual acceleration takes place in just 3 ticks, but it is defined as 5 for some reason
#define k_acceleration_ticks_real 5.0f
#define k_acceleration_ticks ((int32)k_acceleration_ticks_real)
#define k_deceleration_ticks_real 4.0f
#define k_deceleration_ticks ((int32)k_deceleration_ticks_real)

struct alignas(4) s_melee_animation_datum
{
	DWORD field_0;
	DWORD melee_flags;
	BYTE gap_8[36];
	int melee_type_string_id;
	char field_30;
	char field_31;
	BYTE melee_animation_update;
	BYTE animation_action_index;
	char unk;
	BYTE max_animation_range;
};

struct s_character_physics_update_output_datum
{
	uint32 flags;
	BYTE gap_4[8];
	real_vector3d new_velocity;
	real_point3d forward;
	real_vector3d up;
	real_matrix3x3 local_space_rotation;
	uint8 gap_30[28];
};
ASSERT_STRUCT_SIZE(s_character_physics_update_output_datum, 0x70);

struct alignas(4) c_character_physics_mode_melee_datum
{
	int32 m_time_to_target;
	int32 m_counter;
	int32 m_maximum_counter;
	bool m_weapon_is_sword;
	bool m_has_target;
	bool field_E;
	bool field_F;
	real_point3d m_initial_position;
	real_vector3d m_target_expected_velocity_vector;
	real32 m_initial_target_velocity_dot_vector;
	real32 m_target_distance;
	real_point3d m_target_position;
	real_vector3d m_initial_aiming_vector;
	real32 m_maximum_distance;
	bool m_started_decelerating;

	int8 m_deceleration_counter;

	real32 m_velocity_to_decelerate;
	real32 m_distance_to_target_point_before_deceleration;
	real_vector3d field_58;
	int32 field_64;
	int32 field_68;
	real32 field_6C;
	int32 field_70;
	char gap_74[20];

	void set_time_to_target(real32 ticks)
	{
		m_time_to_target = (int32)ticks;
	}

	bool pin_localized_velocity(real_vector3d* output, real_vector3d* localized_velocity);

	void build_initial_melee_parameters(bool valid);
	void update_melee_parameters();
	void update_melee_deceleration();
	void __thiscall update_internal(
		s_character_physics_update_output_datum* physics_output,
		bool has_target,
		real32 target_distance,
		const real_vector3d* target_velocity,
		const real_point3d* target_position,
		const real_vector3d* aiming,
		const real_vector3d* velocity,
		const real_point3d* position);

	static void apply_hooks();
};
ASSERT_STRUCT_SIZE(c_character_physics_mode_melee_datum, 0x88);
ASSERT_STRUCT_OFFSET(c_character_physics_mode_melee_datum, m_initial_aiming_vector, 0x3C);

void call_character_melee_physics_input_update_internal();

extern bool melee_lunge_hook_enabled;