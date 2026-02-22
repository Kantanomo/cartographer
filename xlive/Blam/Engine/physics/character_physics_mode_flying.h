#pragma once

class c_character_physics_mode_flying_datum
{
private:
	int32 m_turning_disabled_counter;
	real_vector3d m_upright_angular_velocity;

public:
	void initialize();
	void dispose();
};
ASSERT_STRUCT_SIZE(c_character_physics_mode_flying_datum, 16);