#pragma once
#include "damage.h"
#include "emblems.h"
#include "object_identifier.h"
#include "object_location.h"

#include "math/color_math.h"
#include "tag_files/string_id.h"

enum e_bsp_policy : int8
{
	_bsp_policy_default = 0,
	_bsp_policy_always_placed = 1,
	_bsp_policy_manual_bsp_placement = 2
};

enum e_scenario_object_placement_flags : uint32
{
	_scenario_object_placement_bit_0 = 0,
	_scenario_object_placement_bit_1 = 1,
	_scenario_object_placement_bit_2 = 2,
	_scenario_object_placement_bit_3 = 3,
	_scenario_object_placement_bit_4 = 4,
	_scenario_object_placement_bit_5 = 5,
	_scenario_object_placement_bit_6 = 6,
	_scenario_object_placement_bit_7 = 7,
	_scenario_object_placement_bit_8 = 8,
	_scenario_object_placement_bit_9 = 9,
	_scenario_object_placement_bit_10 = 10,
	k_scenario_object_placement_flags
};

struct object_placement_data
{
	datum tag_index;
	c_object_identifier object_identifier;
	string_id variant_name;
	uint32 scenario_datum_index;
	e_bsp_policy placement_policy;
	uint8 unk_15;
	uint16 unk_16;
	c_flags_no_init<e_scenario_object_placement_flags, uint32, k_scenario_object_placement_flags> flags;
	real_point3d position;
	real_vector3d forward;
	real_vector3d up;
	real_vector3d translational_velocity;
	real_vector3d angular_velocity;
	real32 scale;
	int32 owner_player_index;
	datum owner_object_index;
	int32 owner_team_index;
	s_damage_owner damage_owner;
	uint32 active_change_colors_mask;
	real_rgb_color change_colors[4];
	s_emblem_info emblem_info;
	int8 pad1;
	int32 region_index;
	uint16 destroyed_constraints_flag;
	uint16 loosened_constraints_flag;
	uint16 field_B4;
	uint16 field_B6;
	bool object_is_inside_cluster;
	uint8 unk_22;
	uint16 unk_23;
	s_location location;
};
ASSERT_STRUCT_SIZE(object_placement_data, 0xC4);