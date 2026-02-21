#pragma once

/* classes */

class c_character_physics_mode_ground_datum
{
private:
	real_vector3d m_last_localized_physics_velocity;
	int32 m_last_time_animation_velocity;
	real_vector3d m_last_animation_velocity;
	datum m_support_havok_component_index;
	datum m_support_havok_component_rigid_body_index;
	real_matrix4x3 m_support_havok_component_matrix;
	int32 m_surface_index;
	datum m_support_object_index;
	int16 m_ground_material_type;
	int8 m_ground_physics_update_ticks;
	uint8 m_flags;
	real_vector3d m_ground_plane;
	real32 m_landing_velocity;

public:
	void initialize();
	void dispose();
};
ASSERT_STRUCT_SIZE(c_character_physics_mode_ground_datum, 116);

/* prototypes */

void character_physics_mode_ground_apply_patches(void);
