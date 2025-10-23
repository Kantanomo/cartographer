#pragma once
#include "data_reference.h"

inline void* tag_data_get_address(data_reference* data)
{
	void* result = NULL;
	if (data->data)
	{
		result = (void*)(data->data + *Memory::GetAddress<uintptr_t*>(0x482290, 0x4A6438));
	}
	return result;
}
