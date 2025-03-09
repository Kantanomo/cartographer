#include "stdafx.h"
#include "rasterizer_text.h"

/* public code */

void __cdecl rasterizer_draw_unicode_string(rectangle2d const* bounds, wchar_t const* string)
{
	INVOKE(0x271BC8, 0, rasterizer_draw_unicode_string, bounds, string);
	return;
}

bool __cdecl rasterizer_text_cache_initialize(void)
{
	return INVOKE(0x271254, 0x0, rasterizer_text_cache_initialize);
}

void __cdecl rasterizer_draw_string(const rectangle2d* bounds, const wchar_t* string, real32 scale)
{
	INVOKE(0x271B74, 0, rasterizer_draw_string, bounds, string, scale);
	return;
}
