#pragma once
#include "interface/user_interface_widget.h"
#include "interface/user_interface_widget_list.h"
#include "interface/user_interface_widget_list_item.h"
#include "interface/user_interface_widget_window.h"

enum e_screen_game_profile_items : uint16
{

	k_screen_game_profile_item_count = 12
};


class c_custom_game_profile_list : public c_list_widget
{
public:
	c_list_item_widget m_list_items[k_screen_game_profile_item_count];
	c_custom_game_profile_list(int16 user_flags);

	virtual int32 setup_children() override;
	virtual c_list_item_widget* get_list_items() override;
	virtual int32 get_list_items_count() override;
	virtual void update_list_items(c_list_item_widget* item, int32 skin_index) override;
};

class c_screen_custom_game_profile_select : public c_screen_with_menu
{
protected:
	c_custom_game_profile_list m_profile_list;

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