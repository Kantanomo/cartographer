#include "stdafx.h"
#include "xbox_sound_cache.h"

/* public code */

void __cdecl sound_cache_close(
	void)
{
	INVOKE(0x3C2E9, 0x4E975, sound_cache_close);

	return;
}
