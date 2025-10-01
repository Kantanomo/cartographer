#include "stdafx.h"
#include "panorama_presence.h"

/* public code */

void __cdecl networking_panorama_presence_set_presence(uint32 context)
{
	INVOKE(0x1B07A0, 0x0, networking_panorama_presence_set_presence, context);
	return;
}
