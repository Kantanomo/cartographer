#pragma once
#include "objects/object_scenario_definitions.h"

/* enums */

enum e_scenario_device_datum_flags : int32
{
	_scenario_device_initially_open_bit = 0,
	_scenario_device_initially_off_bit,
	_scenario_device_changes_only_once_bit,
	_scenario_device_position_reversed_bit,
	_scenario_device_not_usable_bit,
	SCENARIO_DEVICE_DATUM_FLAGS,
};

enum e_scenario_control_datum_flags : int32
{
	_control_usable_from_both_sides_bit = 0
};

enum e_scenario_machine_datum_flags : int32
{
	_scenario_machine_datum_flag_does_not_operate_automatically_bit = 0,
	_scenario_machine_datum_flag_one_sided_bit,
	_scenario_machine_datum_flag_never_appears_locked_bit,
	_scenario_machine_datum_flag_opened_by_melee_attack_bit,
	_scenario_machine_datum_flag_one_sided_for_player_bit,
	_scenario_machine_datum_flag_does_not_close_automatically_bit
};

/* structures */

struct s_scenario_device_datum
{
	int16 power_group;       // Block index: scenario_device_group
	int16 position_group;    // Block index: scenario_device_group
	e_scenario_device_datum_flags flags;
};
ASSERT_STRUCT_SIZE(s_scenario_device_datum, 8);

struct s_scenario_control_datum
{
	e_scenario_control_datum_flags flags;
	int16 dont_touch_this;
	int16 pad;
};
ASSERT_STRUCT_SIZE(s_scenario_control_datum, 8);

// max count: MAXIMUM_CONTROL_DATUMS_PER_SCENARIO 100
struct s_scenario_control
{
	// filt
	// Block index: scenario_control_palette
	int16 type;
	// filt
	// Block index: scenario_object_name
	int16 name;

	s_scenario_object_datum object_data;
	s_scenario_device_datum device_data;
	s_scenario_control_datum control_data;
};
ASSERT_STRUCT_SIZE(s_scenario_control, 68);


// max count: MAXIMUM_DEVICE_GROUPS_PER_SCENARIO 128
struct scenario_device_group
{
	char name[k_tag_string_length];
	real32 initial_value;
	e_device_group_flags flags;
};
ASSERT_STRUCT_SIZE(scenario_device_group, 40);

struct s_scenario_machine_datum
{
	e_scenario_machine_datum_flags flags;
	s_tag_block pathfinding_references;		// struct: pathfinding_object_index_list
};
ASSERT_STRUCT_SIZE(s_scenario_machine_datum, 12);

// max count: MAXIMUM_MACHINE_DATUMS_PER_SCENARIO 400
struct scenario_machine_block
{
	// filt
	// Block index: scenario_machine_palette
	int16 type;
	// filt
	// Block index: scenario_object_name
	int16 name;

	s_scenario_object_datum object_data;
	s_scenario_device_datum device_data;
	s_scenario_machine_datum machine_data;
};
ASSERT_STRUCT_SIZE(scenario_machine_block, 72);
