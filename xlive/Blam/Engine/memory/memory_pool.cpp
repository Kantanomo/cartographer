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

int32 __cdecl memory_pool_block_free(s_memory_pool* memory_pool, void** payload_data)
{
	return INVOKE(0x8BD80, 0x81924, memory_pool_block_free, memory_pool, payload_data);
}