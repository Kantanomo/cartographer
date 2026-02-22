#pragma once
#include "animations/animation_id.h"

class c_character_physics_mode_sentinel_datum
{
private:
	real_point3d m_sentinel_physics_position;
	real_vector3d m_accumulated_sentinel_physics_translation;
	real_vector3d m_sentinel_velocity;
	c_animation_id m_animation_id;
	bool m_climbing;
	uint8 m_accumulated_sentinel_physics_translation_flag;

public:
	void initialize(bool climbing, real_point3d* sentinel_physics_position);
	void dispose(datum object_index);
};
ASSERT_STRUCT_SIZE(c_character_physics_mode_sentinel_datum, 0x2C);