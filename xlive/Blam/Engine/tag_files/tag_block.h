#pragma once
#include "tag_groups.h"

// ### TODO Rename to c_tag_block and refactor

template<typename T = void>
struct tag_block
{
	int32 count;
	int32 data;

	int32 data_size() const
	{
		return count * type_size();
	}
	int32 type_size() const
	{
		return sizeof(T);
	}

	inline T* operator[](int32 index) const
	{
		return (T*)tag_block_get_element_with_size((s_tag_block*)this, index, sizeof(T));
	}
};
ASSERT_STRUCT_SIZE(tag_block<>, 8);

struct s_tag_block_index
{
	int16 count;
};
ASSERT_STRUCT_SIZE(s_tag_block_index, 2);
