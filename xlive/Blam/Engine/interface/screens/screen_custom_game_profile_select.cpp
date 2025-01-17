#include "stdafx.h"
#include "screen_custom_game_profile_select.h"

#include "screen_game_engine_category.h"
#include "interface/user_interface_memory.h"
#include "saved_games/saved_game_files.h"

void* c_screen_custom_game_profile_select::load_slayer_settings(s_screen_parameters* parameters)
{
	return INVOKE(0x25213A, 0, load_slayer_settings, parameters);
}

void* c_screen_custom_game_profile_select::load_slayer_settings_unused(s_screen_parameters* parameters)
{
	return INVOKE(0x2521DD, 0, load_slayer_settings_unused, parameters);
}

void* c_screen_custom_game_profile_select::load_slayer_lobby(s_screen_parameters* parameters)
{
	return INVOKE(0x252291, 0, load_slayer_lobby, parameters);
}

void* c_screen_custom_game_profile_select::load_king_settings(s_screen_parameters* parameters)
{
	return INVOKE(0x252334, 0, load_king_settings, parameters);
}

void* c_screen_custom_game_profile_select::load_king_settings_unused(s_screen_parameters* parameters)
{
	return INVOKE(0x2523D8, 0, load_king_settings_unused, parameters);
}

void* c_screen_custom_game_profile_select::load_king_lobby(s_screen_parameters* parameters)
{
	return INVOKE(0x25248C, 0, load_king_lobby, parameters);
}

void* c_screen_custom_game_profile_select::load_oddball_settings(s_screen_parameters* parameters)
{
	return INVOKE(0x252530, 0, load_oddball_settings, parameters);
}

void* c_screen_custom_game_profile_select::load_oddball_settings_unused(s_screen_parameters* parameters)
{
	return INVOKE(0x2525D4, 0, load_oddball_settings_unused, parameters);
}

void* c_screen_custom_game_profile_select::load_oddball_lobby(s_screen_parameters* parameters)
{
	return INVOKE(0x252688, 0, load_oddball_lobby, parameters);
}

void* c_screen_custom_game_profile_select::load_juggernaut_settings(s_screen_parameters* parameters)
{
	return INVOKE(0x25272C, 0, load_juggernaut_settings, parameters);
}

void* c_screen_custom_game_profile_select::load_juggernaut_settings_unused(s_screen_parameters* parameters)
{
	return INVOKE(0x2527D0, 0, load_juggernaut_settings_unused, parameters);
}

void* c_screen_custom_game_profile_select::load_juggernaut_lobby(s_screen_parameters* parameters)
{
	return INVOKE(0x252884, 0, load_juggernaut_lobby, parameters);
}

void* c_screen_custom_game_profile_select::load_ctf_settings(s_screen_parameters* parameters)
{
	return INVOKE(0x252928, 0, load_ctf_settings, parameters);
}

void* c_screen_custom_game_profile_select::load_ctf_settings_unused(s_screen_parameters* parameters)
{
	return INVOKE(0x2529CC, 0, load_ctf_settings_unused, parameters);
}

void* c_screen_custom_game_profile_select::load_ctf_lobby(s_screen_parameters* parameters)
{
	return INVOKE(0x252A80, 0, load_ctf_lobby, parameters);
}

void* c_screen_custom_game_profile_select::load_assault_settings(s_screen_parameters* parameters)
{
	return INVOKE(0x252B24, 0, load_assault_settings, parameters);
}

void* c_screen_custom_game_profile_select::load_assault_settings_unused(s_screen_parameters* parameters)
{
	return INVOKE(0x252BCB, 0, load_assault_settings_unused, parameters);
}

void* c_screen_custom_game_profile_select::load_assault_lobby(s_screen_parameters* parameters)
{
	return INVOKE(0x252C7C, 0, load_assault_lobby, parameters);
}

void* c_screen_custom_game_profile_select::load_territories_settings(s_screen_parameters* parameters)
{
	return INVOKE(0x252D20, 0, load_territories_settings, parameters);
}

void* c_screen_custom_game_profile_select::load_territories_settings_unused(s_screen_parameters* parameters)
{
	return INVOKE(0x252DC4, 0, load_territories_settings_unused, parameters);
}

void* c_screen_custom_game_profile_select::load_territories_lobby(s_screen_parameters* parameters)
{
	return INVOKE(0x252E78, 0, load_territories_lobby, parameters);
}

void* c_screen_custom_game_profile_select::load_headhunter_settings(s_screen_parameters* parameters)
{
	int8* screen;

	void* pool = ui_pool_allocate_space(21648, 0);

	if(pool)
	{
		 screen = INVOKE_TYPE(0x252007, 0, int8 *(__thiscall*)(void*, e_user_interface_screen_id, e_user_interface_channel_type, e_user_interface_render_window, int16),
			pool,
			_screen_custom_game_menu,
			parameters->m_channel_type,
			parameters->m_window_index,
			parameters->m_flags);


		 //screen->set_allocated(true);
		 screen[108] = 1;
		 *((int32*)screen + 5410) = _saved_game_file_type_game_variant_headhunter;
		 screen[21644] = 1;
		 screen[21645] = 0;

		 user_interface_register_screen_to_channel((c_screen_with_menu*)screen, parameters);
	}
	else
	{
		screen = NULL;
	}

	return nullptr;
}

void* c_screen_custom_game_profile_select::load_headhunter_lobby(s_screen_parameters* parameters)
{
	int8* screen;

	void* pool = ui_pool_allocate_space(21648, 0);

	if (pool)
	{
		screen = INVOKE_TYPE(0x252007, 0, int8 * (__thiscall*)(void*, e_user_interface_screen_id, e_user_interface_channel_type, e_user_interface_render_window, int16),
			pool,
			_screen_multiplayer_variant_listlobby,
			parameters->m_channel_type,
			parameters->m_window_index,
			parameters->m_flags);


		//screen->set_allocated(true);
		screen[108] = 1;
		*((int32*)screen + 5410) = _saved_game_file_type_game_variant_headhunter;
		screen[21644] = 0;
		screen[21645] = 0;
		screen[21614] = parameters->m_context != nullptr;

		user_interface_register_screen_to_channel((c_screen_with_menu*)screen, parameters);
	}
	else
	{
		screen = NULL;
	}

	return nullptr;
}
