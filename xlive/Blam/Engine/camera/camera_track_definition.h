#pragma once
#include "tag_files/tag_block.h"

/* constants */

enum
{
	k_maximum_number_of_camera_track_control_points = 16
};

/* structures */

// max: k_maximum_number_of_camera_track_control_points
struct s_camera_track_control_point
{
	real_vector3d position;
	real_quaternion orientation;
};
ASSERT_STRUCT_SIZE(s_camera_track_control_point, 28);

struct s_camera_track_definition
{
	uint32 unused_flags;
	tag_block<s_camera_track_control_point> control_points;
};
ASSERT_STRUCT_SIZE(s_camera_track_definition, 12);
