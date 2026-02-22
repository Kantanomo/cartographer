#include "stdafx.h"
#include "character_physics_mode_sentinel.h"

/* public code */

void c_character_physics_mode_sentinel_datum::initialize(bool climbing, real_point3d* sentinel_physics_position)
{
	this->m_accumulated_sentinel_physics_translation = *global_zero_vector3d;
	this->m_sentinel_physics_position = *sentinel_physics_position;
	this->m_accumulated_sentinel_physics_translation_flag = 0;
	this->m_sentinel_velocity = *global_zero_vector3d;
	this->m_climbing = climbing;
	this->m_animation_id.set_subgraph(NONE);
	this->m_animation_id.set_index(NONE);
}

void c_character_physics_mode_sentinel_datum::dispose(datum object_index)
{
	// todo: server offset or rewrite (requires c_havok_component defined)
	INVOKE_TYPE(0x10AB62, 0, void(__thiscall*)(c_character_physics_mode_sentinel_datum*, datum), this, object_index);
}
