#pragma once

/* enums */

enum e_render_debug_type
{
	_render_debug_type_circle = 0,
	_render_debug_type_point,
	_render_debug_type_line,
	_render_debug_type_sphere,
	_render_debug_type_cylinder,
	_render_debug_type_pill,
	_render_debug_type_box,
	_render_debug_type_box_outline,
	_render_debug_type_triangle,
	_render_debug_type_string,
	_render_debug_type_string_at_point,
	_render_debug_type_box2d_outline,

	k_render_debug_type_count
};

/* prototypes */

void render_debug(void);

void render_debug_string(const char* string);
