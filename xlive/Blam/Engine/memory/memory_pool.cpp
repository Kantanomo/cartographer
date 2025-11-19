#include "stdafx.h"
#include "memory_pool.h"

/* public code */

uint32 memory_pool_get_free_size(const s_memory_pool* memory_pool)
{
	return memory_pool->offset_to_data;
}

uint32 memory_pool_get_contiguous_free_size(const s_memory_pool* memory_pool)
{
	uint32 result;
	if (memory_pool->last_block_handle)
	{
		result = memory_pool->free_size - ((uintptr_t)memory_pool->last_block_handle + (uintptr_t)memory_pool->last_block_handle - memory_pool->size);
	}
	else
	{
		result = memory_pool->free_size;
	}
	
	return result;
}

bool memory_pool_block_reallocate(s_memory_pool* pool, void** ptr, int32 new_size)
{
	return memory_pool_block_reallocate(pool, ptr, new_size, NULL, 0);
}

bool memory_pool_block_reallocate(s_memory_pool* pool, void** ptr, int32 new_size, const char* file, int32 line)
{
	return INVOKE_TYPE(0x8C12C, 0x81CD0, bool(__cdecl*)(s_memory_pool*, void**, int32, const char*, int32), pool, ptr, new_size, file, line);
}

int32 __cdecl memory_pool_block_free(s_memory_pool* memory_pool, void** payload_data)
{
	return INVOKE(0x8BD80, 0x81924, memory_pool_block_free, memory_pool, payload_data);
}