#pragma once
#include "devices.h"

/* enums */

enum e_machine_data_flags : uint32
{
	_machine_does_not_operate_automatically = 0,
	_machine_does_not_close_automatically = 1,
	_machine_is_one_sided = 2,
	_machine_is_onesided_for_player = 3,
	_machine_never_appears_locked = 4,
	_machine_opened_by_melee_attack = 5,
	k_machine_data_flags_count
};

enum e_machine_type : int16
{
	_machine_door = 0,
	_machine_platform = 1,
	_machine_gear = 2,
	NUMBER_OF_MACHINE_TYPES
};

enum e_machine_definition_flags : int16
{
	_machine_is_pathfinding_obstacle_bit = 0,
	_machine_is_not_pathfinding_obstacle_when_open_bit = 1,
	_machine_is_elevator_bit = 2,
	NUMBER_OF_MACHINE_FLAGS,
};

enum e_collision_response : int16
{
	_collision_response_pause_until_crushed = 0,
	_collision_response_reverse_directions = 1,
};

enum e_machine_pathfinding_policy : int16
{
	_machine_pathfinding_policy_discs = 0,
	_machine_pathfinding_policy_sectors = 1,
	_machine_pathfinding_policy_cut_out = 2,
	_machine_pathfinding_policy_none = 3
};

/* structures */

struct machine_datum
{
	device_datum device;
	c_flags_no_init<e_machine_data_flags, uint32, k_machine_data_flags_count> flags;
	uint32 flags;
	int32 door_open_time_ticks;
	int16 pathfinding_object_index;
	int8 pad[4];
	int16 game_portal;
	int16 field_1DE;
	int16 pad1;
};
ASSERT_STRUCT_SIZE(machine_datum, 480);


struct _machine_definition
{
	// Explaination("$$$ MACHINE $$$", "")

	e_machine_type type;
	e_machine_definition_flags flags;
	real32 door_open_time;  // Seconds

	real_vector2d door_occlusion_bounds;        // maps position [0,1] to occlusion

	e_collision_response collision_response;
	int16 elevator_node_index;

	e_machine_pathfinding_policy pathfindingPolicy;
	int16 pad;
};

struct machine_definition
{
	_object_definition object;
	_device_definition device;
	_machine_definition machine;
};
ASSERT_STRUCT_SIZE(machine_definition, 308);
