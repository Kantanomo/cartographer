#include "stdafx.h"
#include "sort.h"

/* public code */

void __cdecl qsort_4byte(void* base, uint32 num, bool(*compare)(const void*, const void*, const void*), const void* compare_data)
{
	INVOKE(0x8D12B, 0x0, qsort_4byte, base, num, compare, compare_data);
	return;
}
