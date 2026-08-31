#include "stdafx.h"
#include "kablam_level_cache.h"

bool __cdecl kablam_level_cache_try_find_map(wchar_t* map_name, int32* out_map_id, char** out_map_path)
{
	return INVOKE(0, 0x683C, kablam_level_cache_try_find_map, map_name, out_map_id, out_map_path);
}
