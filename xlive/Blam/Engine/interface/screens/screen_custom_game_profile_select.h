#pragma once
#include "interface/user_interface_widget_window.h"

class c_screen_custom_game_profile_select : public c_screen_with_menu
{
public:
	static void* load_slayer_settings(s_screen_parameters* parameters);
	static void* load_slayer_settings_unused(s_screen_parameters* parameters);
	static void* load_slayer_lobby(s_screen_parameters* parameters);

	static void* load_king_settings(s_screen_parameters* parameters);
	static void* load_king_settings_unused(s_screen_parameters* parameters);
	static void* load_king_lobby(s_screen_parameters* parameters);

	static void* load_oddball_settings(s_screen_parameters* parameters);
	static void* load_oddball_settings_unused(s_screen_parameters* parameters);
	static void* load_oddball_lobby(s_screen_parameters* parameters);

	static void* load_juggernaut_settings(s_screen_parameters* parameters);
	static void* load_juggernaut_settings_unused(s_screen_parameters* parameters);
	static void* load_juggernaut_lobby(s_screen_parameters* parameters);

	static void* load_ctf_settings(s_screen_parameters* parameters);
	static void* load_ctf_settings_unused(s_screen_parameters* parameters);
	static void* load_ctf_lobby(s_screen_parameters* parameters);

	static void* load_assault_settings(s_screen_parameters* parameters);
	static void* load_assault_settings_unused(s_screen_parameters* parameters);
	static void* load_assault_lobby(s_screen_parameters* parameters);

	static void* load_territories_settings(s_screen_parameters* parameters);
	static void* load_territories_settings_unused(s_screen_parameters* parameters);
	static void* load_territories_lobby(s_screen_parameters* parameters);

	static void* load_headhunter_settings(s_screen_parameters* parameters);
	static void* load_headhunter_settings_unused(s_screen_parameters* parameters);
	static void* load_headhunter_lobby(s_screen_parameters* parameters);
};

//ASSERT_STRUCT_SIZE(c_screen_custom_game_profile_select, 21648);