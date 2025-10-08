#pragma once

/* enums */

enum e_scenario_unit_datum_flags : int32
{
	_scenario_unit_datum_flag_dead_bit 0,
	_scenario_unit_datum_flag_closed_bit,
	_scenario_unit_datum_flag_not_enterable_by_player_bit,
};

/* structures */

struct s_scenario_unit_datum
{
	real32 body_vitality;
	e_scenario_unit_datum_flags flags;
};
ASSERT_STRUCT_SIZE(s_scenario_unit_datum, 8);
