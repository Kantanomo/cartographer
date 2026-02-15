#pragma once

/* enums */

enum e_ai_direction_type
{
	_ai_direction_type_movement,
	_ai_direction_type_prop_ref,
	_ai_direction_type_target,
	_ai_direction_type_point,
	_ai_direction_type_vector,
	_ai_direction_type_danger_zone,
	_ai_direction_type_object
};

/* structures */

union ai_direction_data
{
	datum prop_index;
	datum object_index;
	real_point3d point;
	real_vector3d vector;
};
ASSERT_STRUCT_SIZE(ai_direction_data, 12);

class c_ai_direction
{
public:
	e_ai_direction_type type;
	ai_direction_data data;
};
ASSERT_STRUCT_SIZE(c_ai_direction, 16);
