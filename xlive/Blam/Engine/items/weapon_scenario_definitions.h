#pragma once
#include "objects/object_scenario_definitions.h"

/* structures */

struct s_scenario_weapon_datum
{
	int16 rounds_left;
	int16 rounds_loaded;
	e_item_datum_flags flags;
};
ASSERT_STRUCT_SIZE(s_scenario_weapon_datum, 8);

// max count: MAXIMUM_WEAPON_DATUMS_PER_SCENARIO 128
struct s_scenario_weapon
{
	// filt
	// Block index: scenario_weapon_palette
	int16 type;
	// filt
	// Block index: scenario_object_name
	int16 name;

	s_scenario_object_datum object_data;
	s_scenario_object_permutation permutation_data;
	s_scenario_weapon_datum weapon_data;
};
ASSERT_STRUCT_SIZE(s_scenario_weapon, 84);