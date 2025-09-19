#pragma once
#include "devices/device_scenario_definitions.h"
#include "objects/object_scenario_definitions.h"

/* enums */

enum e_light_placement_flags : short
{
	light_placement_flag_custom_geometry = FLAG(0),
	light_placement_flag_unused = FLAG(1),
	light_placement_flag_cinematicOnly = FLAG(2)
};

enum e_lightmap_type : short
{
	lightmap_type_use_light_tag_setting = 0,
	lightmap_type_dynamic_only = 1,
	lightmap_type_dynamic_with_lightmaps = 2,
	lightmap_type_lightmaps_only = 3
};

enum e_lightmap_flags : short
{
	lightmap_placement_flag_unused = FLAG(0),
};

/* structures */

struct s_scenario_light_datum
{
	int16 type;	// e_light_type
	e_light_placement_flags flags;
	e_lightmap_type lightmap_type;
	e_lightmap_flags lightmap_flags;
	real32 lightmap_half_life;
	real32 lightmap_light_scale;
	real_point3d target_point;
	real32 width_world_units;
	real32 height_scale_world_units;
	real32 field_of_view_degrees;
	real32 falloff_distance_world_units;
	real32 cutoff_distance_world_units_from_far_plane;
};
ASSERT_STRUCT_SIZE(s_scenario_light_datum, 48);

// max count: MAXIMUM_LIGHTS_PER_SCENARIO 500
struct s_scenario_light
{
	// Explaination("~Controls", " ")

	// lflg
	// allg
	// filt
	// Block index: scenario_light_palette
	int16 type;
	// filt
	// Block index: scenario_object_name
	int16 name;

	s_scenario_object_datum object_data;
	s_scenario_device_datum device_data;
	s_scenario_light_datum light_data;
};
ASSERT_STRUCT_SIZE(s_scenario_light, 108);
