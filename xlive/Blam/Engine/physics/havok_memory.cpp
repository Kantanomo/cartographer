#include "stdafx.h"
#include "havok_memory.h"

/* prototypes */

static bool* region_memory_being_borrowed_get(void);

/* public code */

bool havok_memory_allocator_locked(void)
{
	return INVOKE(0xF78F7, 0xDE732, havok_memory_allocator_locked);
}

bool is_havok_update_memory_initialized(void)
{
	return *region_memory_being_borrowed_get() == false;
}

void __cdecl havok_memory_garbage_collect(void)
{
	INVOKE(0xF7F78, 0xDEDB3, havok_memory_garbage_collect);
	return;
}

/* private code */

static bool* region_memory_being_borrowed_get(void)
{
	return Memory::GetAddress<bool*>(0x4D2D5C, 0x4F6C14);
}
