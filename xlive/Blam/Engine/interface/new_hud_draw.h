#pragma once

struct s_draw_hud_widget_input_results
{
	real32 result_1;
	real32 result_2;
	real32 result_3;
	real32 result_4;
};


/* public code */

void hud_draw_on_map_load(void);

datum hud_bitmap_tag_index_get(void);

int16 hud_bitmap_data_index_get(void);

void hud_player_indicators_draw_enabled_set(int32 user_index, bool enabled);

void hud_player_indicators_draw_reset(void);

void __cdecl draw_hud_layer(void);

void new_hud_draw_apply_patches(void);

void new_hud_draw_deinitialize(void);

void __cdecl render_ingame_user_interface_hud_element(
	real32 left,
	real32 top,
	int16 x,
	int16 y,
	real32 scale,
	real32 rotation_rad,
	datum bitmap_tag_index,
	datum bitmap,
	real_rectangle2d* bounds,
	datum shader_tag_index);
