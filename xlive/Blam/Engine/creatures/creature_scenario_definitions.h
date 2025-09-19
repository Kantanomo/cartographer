#pragma once
#include "objects/object_scenario_definitions.h"

/* structures */

// max count: MAXIMUM_CREATURE_DATUMS_PER_SCENARIO 128
struct s_scenario_creature
{
	// filt
	// Block index: scenario_creature_palette
	int16 type;
	// filt
	// Block index: scenario_object_name
	int16 name;
	s_scenario_object_datum object_data;
};
ASSERT_STRUCT_SIZE(s_scenario_creature, 52);
