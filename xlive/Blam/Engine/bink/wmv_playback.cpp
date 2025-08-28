#include "stdafx.h"
#include "wmv_playback.h"

/* public code */

void __cdecl wmv_update(void)
{
	INVOKE(0x3A125, 0x0, wmv_update);
	return;
}

bool __cdecl wmv_playback_in_progress(void)
{
	return INVOKE(0x39F09, 0x0, wmv_playback_in_progress);
}

void __cdecl wmv_render(void)
{
	INVOKE(0x3A264, 0x0, wmv_render);
	return;
}
