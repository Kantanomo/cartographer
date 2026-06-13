#pragma once

/* public code */

void apply_interface_hooks(void);

datum interface_get_tag_index(int32 interface_tag_index);

void __cdecl interface_draw_bitmap(
	struct bitmap_data* bitmap,
	const real_point2d* position,
	const real_rectangle2d* rectangle,
	real32 a4,
	real32 a5,
	real32 a6);
