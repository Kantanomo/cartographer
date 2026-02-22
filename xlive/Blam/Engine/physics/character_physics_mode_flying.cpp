#include "stdafx.h"
#include "character_physics_mode_flying.h"

/* public code */

void c_character_physics_mode_flying_datum::initialize()
{
	this->m_turning_disabled_counter = 0;
	this->m_upright_angular_velocity = *global_zero_vector3d;
}

void c_character_physics_mode_flying_datum::dispose()
{
}
