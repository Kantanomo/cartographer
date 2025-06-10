#include "stdafx.h"
#include "rockall_heap_manager.h"

/* public code */

c_normal_allocation* normal_allocation_global_get(void)
{
	return *Memory::GetAddress<c_normal_allocation**>(0x4127C0, 0x3B5168);
}
