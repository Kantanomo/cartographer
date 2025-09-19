#pragma once

/* prototypes */

void __cdecl qsort_4byte(void* base, uint32 num, bool(*compare)(const void*, const void*, const void*), const void* compare_data);
