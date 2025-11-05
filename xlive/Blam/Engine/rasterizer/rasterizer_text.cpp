#include "stdafx.h"
#include "rasterizer_text.h"

#include "interface/hud.h"
#include "rasterizer/dx9/rasterizer_dx9_dynamic_geometry.h"
#include "text/unicode.h"

/* prototypes */

static void rasterizer_draw_utf32_string(
	const rectangle2d* bounds,
	const rectangle2d* secondary_bounds,
	const point2d* cursor_reference,
	int16 height_adjust,
	const utf32* string,
	real32 scale,
	const s_dynamic_geometry_map_color_parameters* color,
	const s_dynamic_geometry_map_color_parameters* shadow_color,
	bool a9);

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
	INVOKE_TYPE(0x271B74, 0, void(__cdecl*)(const rectangle2d*, const wchar_t*, real32), bounds, string, scale);
	return;
}

void rasterizer_draw_string(const rectangle2d* bounds, const rectangle2d* secondary_bounds, const point2d* cursor_reference, int16 height_adjust, const char* string, const s_dynamic_geometry_map_color_parameters* color)
{
	utf32 utf32_string[2048];

	ascii_string_to_utf32_string(string, utf32_string, NUMBEROF(utf32_string));
    rasterizer_draw_utf32_string(bounds, secondary_bounds, cursor_reference, height_adjust, utf32_string, *get_secondary_hud_scale(), color, NULL, NULL);
	return;
}

static void rasterizer_draw_utf32_string(
	const rectangle2d* bounds,
	const rectangle2d* secondary_bounds,
	const point2d* cursor_reference,
	int16 height_adjust,
	const utf32* string,
	real32 scale,
	const s_dynamic_geometry_map_color_parameters* color,
	const s_dynamic_geometry_map_color_parameters* shadow_color,
	bool a9)
{
	INVOKE(0x27186D, 0x0, rasterizer_draw_utf32_string, bounds, secondary_bounds, cursor_reference, height_adjust, string, scale, color, shadow_color, a9);
	return;
}