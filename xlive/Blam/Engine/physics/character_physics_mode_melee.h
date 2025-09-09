#pragma once

#define k_valid_real_epsilon 0.001f
// in practice the actual acceleration takes place in just 3 ticks, but it is defined as 5 for some reason
#define k_acceleration_ticks_real 5.0f
#define k_acceleration_ticks ((int)k_acceleration_ticks_real)
#define k_deceleration_ticks_real 4.0f
#define k_deceleration_ticks ((int)k_deceleration_ticks_real)

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

struct s_character_physics_output
{
	DWORD flags;
	BYTE gap_4[8];
	real_vector3d translational_velocity;
	real_point3d field_18;
	real_vector3d field_24;
	BYTE gap_30[52];
};
ASSERT_STRUCT_SIZE(s_character_physics_output, 0x64);

struct alignas(4) c_character_physics_mode_melee_datum
{
	int m_time_to_target_in_ticks;
	int m_melee_tick;
	int32 m_maximum_counter;
	bool m_weapon_is_sword;
	bool m_has_target;
	bool field_E;
	BYTE field_F;
	real_point3d m_melee_start_origin;
	real_vector3d field_1C;
	real32 m_field_28;
	real32 m_distance;
	real_point3d m_target_point;
	real_vector3d m_aiming_direction;
	real32 m_maximum_distance;
	bool m_started_decelerating;
	BYTE field_4D[1];

	// in practice, this is actually structure padding, that we use flags to store information
	BYTE m_flags;
	BYTE m_deceleration_ticks;

	real32 m_velocity_to_decelerate;
	real32 m_distance_to_target_point_before_deceleration;
	real_vector3d field_58;
	int field_64;
	int field_68;
	real32 field_6C;
	int field_70;
	char gap_74[20];

	void set_time_to_target(real32 ticks)
	{
		m_time_to_target_in_ticks = (int32)ticks;
	}

	bool pin_localized_velocity(real_vector3d* output, real_vector3d* localized_velocity);
	void melee_deceleration_fixup(s_character_physics_output* output, real_point3d* object_origin, real_vector3d* current_velocity, real_vector3d* aiming_vector);

	void build_initial_melee_parameters(bool valid);
	void update_melee_parameters();
	void update_melee_deceleration();
	void __thiscall update_internal(s_character_physics_output* a2, bool a3, real32 distance_world_units, const real_vector3d* target_translational_velocity, const real_point3d* target_origin, const real_vector3d* aiming_vector, const real_vector3d* translational_velocity, const real_point3d* object_origin);

	static void apply_hooks();
};
ASSERT_STRUCT_SIZE(c_character_physics_mode_melee_datum, 0x88);

void call_character_melee_physics_input_update_internal();

extern bool melee_lunge_hook_enabled;