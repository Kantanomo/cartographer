#pragma once
#include "objects/object_scenario_definitions.h"

/* enums */

enum e_item_datum_flags : int32
{
	_equipment_datum_flag_initially_at_rest_does_not_fall_bit = 0,
	_equipment_datum_flag_obsolete_bit,
	_equipment_datum_flag_does_accelerate_moves_due_to_explosions_bit
};

/* structures */

struct s_scenario_equipment_datum
{
	e_item_datum_flags flags;
};
ASSERT_STRUCT_SIZE(s_scenario_equipment_datum, 4);

// max count: MAXIMUM_EQUIPMENT_DATUMS_PER_SCENARIO 256
struct s_scenario_equipment
{
	// filt
	// Block index: scenario_equipment_palette
	int16 type;
	// filt
	// Block index: scenario_object_name
	int16 name;

	s_scenario_object_datum object_data;
	s_scenario_equipment_datum equipment_data;
};
ASSERT_STRUCT_SIZE(s_scenario_equipment, 56);
