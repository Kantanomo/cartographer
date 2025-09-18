#pragma once

// TODO: rename to tag_data and move to tag_group_access.h
struct data_reference
{
	int32 size;
	int32 data;
};
ASSERT_STRUCT_SIZE(data_reference, 8);
