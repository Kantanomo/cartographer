#include "stdafx.h"
#include "rasterizer_dx9_primitives.h"

/* public code */

void __cdecl rasterizer_dx9_draw_primitive_quad(const rectangle2d* points, pixel32 rect_color)
{
	INVOKE(0x3B101, 0x0, rasterizer_dx9_draw_primitive_quad, points, rect_color);
	return;
}
