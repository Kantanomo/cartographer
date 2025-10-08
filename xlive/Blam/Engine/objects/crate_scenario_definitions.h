#pragma once
#include "objects/object_scenario_definitions.h"

/* structures */

// max count: MAXIMUM_CRATE_DATUMS_PER_SCENARIO 1024
struct s_scenario_crate
{
	// filt
	// Block index: scenario_crate_palette
	int16 type;
	// filt
	// Block index: scenario_object_name
	int16 name;

	s_scenario_object_datum object_data;
	s_scenario_object_permutation permutation_data;
};
ASSERT_STRUCT_SIZE(s_scenario_crate, 76);

