#pragma once
#include "objects/objects.h"

enum e_item_data_definition_flags : uint32
{
	_item_data_definition_flag_bit_0 = 1,

	k_item_data_definition_flags
};

struct s_item_data_definition
{
	object_datum object;
	c_flags_no_init<e_item_data_definition_flags, uint32, k_item_data_definition_flags> flags;
	char field_0[28];
	datum parent_datum_index;
	int8 field_32[26];
};
ASSERT_STRUCT_SIZE(s_item_data_definition, 364);
