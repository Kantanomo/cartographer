#pragma once
#include "unit_scenario_definitions.h"

#include "objects/object_scenario_definitions.h"

/* structures */

// max count: MAXIMUM_BIPED_DATUMS_PER_SCENARIO 128
struct s_scenario_biped
{
	// filt
	// Block index: scenario_biped_palette
	int16 type;
	// filt
	// Block index: scenario_object_name
	int16 name;

	s_scenario_object_datum object_data;
	s_scenario_object_permutation permutation_data;
	s_scenario_unit_datum unit_data;
};
ASSERT_STRUCT_SIZE(s_scenario_biped, 84);
