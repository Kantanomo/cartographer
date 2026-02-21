#pragma once

class c_character_physics_mode_dead_datum
{
private:
	real_vector3d m_resting_surface_normal;
	uint8 m_on_restable_slope_ticks;
	uint8 m_contact_with_ground_ticks;
	uint8 m_relaxation_ticks;
	uint8 m_active_ticks;
	uint8 m_near_zero_velocity_ticks;
	bool m_on_fixed_surface;
	bool m_rigid_body_active;
	uint8 m_dead_size;
	datum m_support_object_index;
	int32 m_support_node_index;
	real_matrix4x3 m_support_matrix;
	real_vector3d m_local_physics_last_velocity;
	int32 m_z_transition_start_time;
	int32 m_time_of_death_node_acceleration;
	int32 m_death_node_index;
	real_vector3d m_death_node_acceleration;

public:
	void initialize();
	void dispose();
};
ASSERT_STRUCT_SIZE(c_character_physics_mode_dead_datum, 116);
