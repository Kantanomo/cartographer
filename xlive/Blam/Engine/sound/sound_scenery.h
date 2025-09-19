#pragma once
#include "objects/object_scenario_definitions.h"

/* enums */

enum e_sound_scenery_volume_type : int32
{
	sound_scenery_volume_type_sphere = 0,
	sound_scenery_volume_type_vertical_cylinder = 1
};

/* structures */

struct s_scenario_sound_scenery_datum
{
	e_sound_scenery_volume_type volume_type;
	real32 height;
	real_vector2d override_distance_bounds;
	real32 override_inner_cone_angle;
	real32 override_outer_cone_angle;
	real32 override_outer_cone_gain_db;
};
ASSERT_STRUCT_SIZE(s_scenario_sound_scenery_datum, 28);

// max count: MAXIMUM_SOUND_SCENERY_DATUMS_PER_SCENARIO 256
struct s_scenario_sound_scenery
{
	// filt
	// Block index: scenario_sound_scenery_palette
	int16 type;
	// filt
	// Block index: scenario_object_name
	int16 name;

	s_scenario_object_datum object_data;
	s_scenario_sound_scenery_datum soundscenery;
};
ASSERT_STRUCT_SIZE(s_scenario_sound_scenery, 80);

