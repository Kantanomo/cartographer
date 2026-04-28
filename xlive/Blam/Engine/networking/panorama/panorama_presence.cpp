#include "stdafx.h"
#include "panorama_presence.h"

/* public code */

void __cdecl panorama_presence_set(uint32 context)
{
	INVOKE(0x1B07A0, 0x0, panorama_presence_set, context);
	return;
}
