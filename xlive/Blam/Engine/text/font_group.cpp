#include "stdafx.h"
#include "font_group.h"

/* public code */

void __cdecl font_initialize(void)
{
	INVOKE(0x31DFF, 0x0, font_initialize);	// Not used on dedi
	return;
}

void __cdecl font_idle(void)
{
	INVOKE(0x31B5C, 0x0, font_idle);
	return;
}

int32 __cdecl font_get_line_height(e_font_id font_id)
{
	return INVOKE(0x31865, 0x0, font_get_line_height, font_id);
}
