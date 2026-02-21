#include "stdafx.h"
#include "character_physics_mode_dead.h"

void c_character_physics_mode_dead_datum::initialize()
{
	this->m_resting_surface_normal = *global_up3d;
	this->m_contact_with_ground_ticks = 0;
	this->m_on_restable_slope_ticks = 0;
	this->m_relaxation_ticks = 0;
	this->m_active_ticks = 0;
	this->m_near_zero_velocity_ticks = 0;
	this->m_on_fixed_surface = false;
	this->m_dead_size = 0;
	this->m_support_object_index = NONE;
	this->m_support_node_index = NONE;
	this->m_local_physics_last_velocity = *global_zero_vector3d;
	this->m_time_of_death_node_acceleration = NONE;
	this->m_death_node_index = NONE;
	this->m_rigid_body_active = false;
	this->m_death_node_acceleration = *global_zero_vector3d;
	this->m_z_transition_start_time = NONE;
}

void c_character_physics_mode_dead_datum::dispose()
{
}
