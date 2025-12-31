#pragma once
#include "tag_files/tag_groups.h"

/* enums */

enum e_unit_action : int32
{
	_unit_action_0,
	_unit_action_1,
	_action_primary_weapon_primary_recoil,
	_action_primary_weapon_secondary_recoil,
	_action_primary_weapon_primary_chamber,
	_action_primary_weapon_secondary_chamber,
	_action_primary_weapon_primary_charged,
	_action_primary_weapon_secondary_charged,
	_unit_action_8,
	_unit_action_9,
	_unit_action_10,
	_unit_action_11,
	_action_secondary_weapon_primary_recoil,
	_action_secondary_weapon_secondary_recoil,
	_action_secondary_weapon_primary_chamber,
	_action_secondary_weapon_secondary_chamber,
	_action_secondary_weapon_primary_charged,
	_action_secondary_weapon_secondary_charged,
	_unit_action_18,
	_unit_action_19,
	_unit_action_20,
	_unit_action_21,
	_unit_action_22,
	_unit_action_23,
	_unit_action_24,
	_unit_action_25,
	_unit_action_26,
	_unit_action_27,
	_unit_action_28,
	_unit_action_29,
	_unit_action_30,
	_unit_action_31,
	_unit_action_32,
	_unit_action_33,
	_unit_action_34,
	_unit_action_35,
	_unit_action_36,
	_unit_action_37,
	_unit_action_38,
	_unit_action_39,
	_unit_action_40,
	_unit_action_41,
	_unit_action_42,
	_unit_action_43,
	_unit_action_44,
	_unit_action_45,
	_unit_action_46,
	_unit_action_47,
	_unit_action_48,
	_unit_action_49,
	_unit_action_50,
	_unit_action_51,
	_unit_action_52,
	_unit_action_53,
	_unit_action_54,
	_unit_action_55,
	_unit_action_56,
	_unit_action_57,
	_unit_action_58,
	_unit_action_59,
	k_unit_action_count,
	k_unit_action_invalid = NONE
};

/* structures */

struct action_request
{
	e_unit_action type;

	union
	{
		struct 
		{
			uint8 gap_4[24];
		};
	} data;
};
ASSERT_STRUCT_SIZE(action_request, 28);

struct unit_action_definition
{
	bool(__cdecl* submit)(datum object_index, action_request* request);
	bool(__cdecl* update)(datum object_index, e_unit_action action);
	void(__cdecl* finish)(datum object_index, e_unit_action action);
	void(__cdecl* interrupt)(datum object_index, e_unit_action action);
};
ASSERT_STRUCT_SIZE(unit_action_definition, 16);

#define MAXIMUM_POSTURES_PER_UNIT 20

// max count: MAXIMUM_POSTURES_PER_UNIT 20
struct s_posture_definition
{
	string_id name;
	real_vector3d pill_offset;
};
ASSERT_STRUCT_SIZE(s_posture_definition, 16);

/* public functions */

bool __cdecl action_submit(datum unit_index, e_unit_action action);