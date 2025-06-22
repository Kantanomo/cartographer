#include "stdafx.h"
#include "lights.h"

/* public code */

void __cdecl render_light_lens_flares(void)
{
	INVOKE(0x14D90E, 0x0, render_light_lens_flares);
	return;
}

void __cdecl lights_initialize_for_new_map(void)
{
	INVOKE(0x14C74D, 0x1494E5, lights_initialize_for_new_map);
	return;
}

void __cdecl lights_update(void)
{
	INVOKE(0x14F455, 0x14C1ED, lights_update);
	return;
}
