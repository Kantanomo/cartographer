#pragma once
#include "havok/common/base/container/array/hkArray.h"
#include "havok/common/base/math/matrix/hkTransform.h"

/* constants */

enum
{
	k_maximum_havok_component_count = 512
};

/* classes */

class c_havok_contact_point
{
	uint16 m_contact_point_id;
	uint32 m_environment_shape_key;
	int32 m_impact_datum_index;
	int32 m_surface_index;
	int16 m_global_material_type_a;
	int16 m_global_material_type_b;
	int32 m_contact_havok_component_index;
	int32 m_contact_point_object_index;
	real_point3d m_position;
	real_vector3d m_normal;
	real32 m_distance;
	real32 m_friction;
	real32 m_shape_friction;
	real32 m_restitution;
	uint8 m_surface_flags;
	int8 m_rigid_body_index_a;
	int8 m_rigid_body_index_b;
};
ASSERT_STRUCT_SIZE(c_havok_contact_point, 0x48);

class c_havok_component
{
private:
	class c_rigid_body
	{
		real_point3d m_pre_simulation_position;
		real_vector3d m_pre_simulation_linear_velocity;
		real_vector3d m_pre_simulation_angular_velocity;
		int8 m_unknown_24[12];
		int8 m_unknown_30[16];
		void* m_rigid_body; // hkRigidBody
		int8 m_flags;
		int8 m_flags2;
		hkInplaceArray<int8, 4> rigid_body_indexes;
		int8 _unknown_58[8];
	};
	ASSERT_STRUCT_SIZE(c_rigid_body, 0x60)

	class c_constraint
	{
		int16 m_constraint_type;
		int16 m_constraint_type_index;
		int32 m_field_04;
		void* m_constraint; // hkConstraint
	};
	ASSERT_STRUCT_SIZE(c_constraint, 0xC)

	int16 identifier;
	uint32 m_flags;
	datum m_object_index;
	int32 m_field_0C;
	int32 m_last_contact_time;
	real32 m_contact_magnitude;
	uint8 m_object_position_controlling_rigid_body_index;
	uint8 m_object_position_controlling_node_index;
	uint8 m_phantom_power_intensity;
	uint8 m_phantom_power_strength;
	uint8 m_maximum_rigid_body_size;
	int32 m_impact_array_index;
	uint8 m_field_24[12]; // some havok class
	hkTransform m_transform;
	hkArray<c_rigid_body> m_rigid_bodies_data;
	hkArray<c_constraint> m_constraints_data;
	hkArray<c_havok_contact_point> m_havok_contact_points_data;
	int32 m_bodies_in_phantom;
	int32 m_simple_shape_phantom;
	int32 m_aabb_phantom;
};
ASSERT_STRUCT_SIZE(c_havok_component, 0xA0);
