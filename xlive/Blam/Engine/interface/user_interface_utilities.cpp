#include "stdafx.h"
#include "user_interface_utilities.h"

#include "hud.h"
#include "user_interface_controller.h"
#include "user_interface_shared_globals.h"
#include "user_interface_widget_window.h"

#include "render/render.h"
#include "rasterizer/rasterizer_globals.h"

/* typedefs */

typedef void(__cdecl* user_interface_get_cursor_position_scale_t)(point2d* out_position);

/* globals */

user_interface_get_cursor_position_scale_t p_user_interface_get_cursor_position;

/* public code */

void user_interface_utilities_apply_patches(void)
{
	DETOUR_ATTACH(p_user_interface_get_cursor_position, Memory::GetAddress<user_interface_get_cursor_position_scale_t>(0x21D620), user_interface_get_cursor_position_scaled);
	return;
}

datum __cdecl user_interface_get_widget_tag_index_from_screen_id(e_user_interface_screen_id screen_id)
{
	return INVOKE(0x20C701, 0x0, user_interface_get_widget_tag_index_from_screen_id, screen_id);
}

bool __cdecl user_interface_exit_to_dash()
{
	return INVOKE(0x212F7F, 0x0, user_interface_exit_to_dash);
}

bool __cdecl user_interface_save_map_and_exit()
{
	return INVOKE(0x212F68, 0x0, user_interface_save_map_and_exit);
}

void user_interface_transition_to_offline()
{
	for (e_controller_index controller = first_controller();
		controller != k_no_controller;
		controller = next_controller(controller))
	{
		if (user_interface_controller_is_player_profile_valid(controller))
		{
			user_interface_controller_xbox_live_account_set_signed_in(controller, false);
			user_interface_controller_update_player_name(controller);
			user_interface_controller_update_network_properties(controller);
		}
	}
}

void user_interface_get_cursor_position(point2d* out_position)
{
	INVOKE(0x2EC6D, 0, user_interface_get_cursor_position, out_position);
}

void __cdecl user_interface_get_cursor_position_scaled(point2d* out_position)
{
	if (out_position)
	{
		point2d cur_pos;
		// Get the current cursor position
		user_interface_get_cursor_position(&cur_pos);

		const window_bound* user_bounds = get_user_window_bounds(0);

		// Don't adjust the cursor scale by viewport bounds if we're in campaign, as the pause menu takes up the full screen
		point2d bounds;
		if (user_interface_in_screen(_user_interface_channel_type_gameshell_screen, _window_4, _screen_single_player_pause_game) )
		{
			const s_rasterizer_globals* rasterizer_globals = rasterizer_globals_get();
			bounds = { (int16)rasterizer_globals->resolution_x, (int16)rasterizer_globals->resolution_y };
		}
		else
		{
			bounds = { user_bounds->rasterizer_camera.viewport_bounds.right, user_bounds->rasterizer_camera.window_bounds.bottom };
		}

		cur_pos.x = (int16)(cur_pos.x - (bounds.x / 2.f));
		cur_pos.y = (int16)((bounds.y / 2.f) - cur_pos.y);

		cur_pos.x = (int16)(cur_pos.x / *get_ui_scale());
		cur_pos.y = (int16)(cur_pos.y / *get_ui_scale());

		// Convert to integer coordinates
		*out_position = cur_pos;
	}
	return;
}

void __cdecl user_interface_utilities_play_sound(e_user_interface_global_sound sound_type)
{
	INVOKE(0x21DD04, 0x0, user_interface_utilities_play_sound, sound_type);
	return;
}
