#pragma once
#include "math/color_math.h"

/* prototypes */

void __cdecl draw_string_get_color(real_argb_color* color);
void __cdecl draw_string_set_color(real_argb_color const* color);
void __cdecl draw_string_set_shadow_color(real_argb_color const* color);
void __cdecl draw_string_set_player_color(real_argb_color const* color);
void __cdecl draw_string_set_font(int32 font_index);
void __cdecl draw_string_set_format(int32 style, int32 justification, uint32 flags, bool wrap_horizontally);
void __cdecl draw_string_set_draw_mode(int32 font, int32 style, int32 justification, uint32 flags, real_argb_color const* color, real_argb_color const* shadow_color, bool wrap_horizontally);
bool __cdecl draw_string_set_string(const wchar_t* string);
void __cdecl draw_string_set_position(int16 x, int16 y);
void __cdecl draw_string_compute_bounds(const rectangle2d* bounds, const wchar_t* string, rectangle2d* text_bounds, rectangle2d* cursor_bounds, real32 scale);
void __cdecl draw_string_reset(void);
