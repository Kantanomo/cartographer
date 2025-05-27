#pragma once
#include "memory/data.h"

s_data_array* __cdecl ui_list_data_new(const char* name, uint32 count, uint32 size);

uint8* __cdecl ui_pool_allocate_space(uint32 pool_size, int32 a2);
void __cdecl ui_pool_deallocate(uint8* object);
