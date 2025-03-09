#include "stdafx.h"

#include "screen_button_settings.h"
#include "cache/cache_files.h"
#include "interface/user_interface_memory.h"
#include "interface/user_interface_globals.h"
#include "interface/user_interface_bitmap_block.h"
#include "tag_files/global_string_ids.h"
#include "tag_files/tag_loader/tag_injection.h"

/* macro defines */

#define k_button_setting_list_name "button settings edit list"

/* enums */

enum e_button_list_items : uint16
{
	_item_standard,
	_item_south_paw,
	_item_boxer,
	_item_green_thumb,
	
	//custom addons
	_item_jumpy,

	k_total_no_of_button_list_items
};

enum e_button_screen_panes
{
	_button_screen_pane_standard,
	_button_screen_pane_south_paw,
	_button_screen_pane_boxer,
	_button_screen_pane_green_thumb,
	//_button_screen_pane_unused4,//copy_of_standard
	_button_screen_pane_jumpy,
	_button_screen_pane_unused5,//copy_of_south_paw
	_button_screen_pane_unused6,//copy_of_boxer
	_button_screen_pane_unused7,//copy_of_green_thumb	

	k_total_no_of_button_settings_pane,
};

enum e_jumpy_pane_text_blocks
{
	_jumpy_pane_text_help,
	_jumpy_pane_text_button_throw_grenade,
	_jumpy_pane_text_button_crouch,
	_jumpy_pane_text_button_score,
	_jumpy_pane_text_button_pause,
	_jumpy_pane_text_button_fire,
	_jumpy_pane_text_button_flashlight,//_jumpy_pane_text_button_reload
	_jumpy_pane_text_button_switch_weapons,
	_jumpy_pane_text_button_reload,//_jumpy_pane_text_button_melee
	_jumpy_pane_text_button_swap_grenades,//_jumpy_pane_text_button_jump
	_jumpy_pane_text_button_jump,//_jumpy_pane_text_button_flashlight
	_jumpy_pane_text_button_melee,//_jumpy_pane_text_button_swap_grenades
	_jumpy_pane_text_button_zoom,

	//addtions
	_jumpy_pane_text_button_swap_left,
	k_jumpy_pane_texts_count,

	k_orignal_jumpy_pane_texts_count =13,
	k_number_of_jumpy_pane_text_addons = k_jumpy_pane_texts_count- k_orignal_jumpy_pane_texts_count

};

enum e_jumpy_ingame_pane_text_blocks
{
	_jumpy_ig_pane_text_help,
	_jumpy_ig_pane_text_button_throw_grenade,
	_jumpy_ig_pane_text_button_crouch,
	_jumpy_ig_pane_text_button_score,
	_jumpy_ig_pane_text_button_pause,
	_jumpy_ig_pane_text_button_fire,
	_jumpy_ig_pane_text_button_flashlight,//_jumpy_pane_text_button_reload
	_jumpy_ig_pane_text_button_switch_weapons,
	_jumpy_ig_pane_text_button_reload,//_jumpy_pane_text_button_melee
	_jumpy_ig_pane_text_button_swap_grenades,//_jumpy_pane_text_button_jump
	_jumpy_ig_pane_text_button_jump,//_jumpy_pane_text_button_flashlight
	_jumpy_ig_pane_text_button_melee,//_jumpy_pane_text_button_swap_grenades
	_jumpy_ig_pane_text_button_zoom,

	//addtions
	_jumpy_ig_pane_text_header,
	_jumpy_ig_pane_text_button_swap_left,
	k_jumpy_ig_pane_texts_count,

	k_orignal_jumpy_ig_pane_texts_count = 13,
	k_number_of_jumpy_ingame_pane_text_addons = k_jumpy_ig_pane_texts_count - k_orignal_jumpy_ig_pane_texts_count

};

enum e_button_settings_bitmap_blocks
{
	_button_settings_bitmap_ul_03,
	_button_settings_bitmap_br_05,
	_button_settings_bitmap_button_config,

	k_number_of_button_settings_bitmap_blocks
};

enum e_button_settings_ingame_bitmap_blocks //qtr_screen
{
	_button_settings_ingame_bitmap_ul_07,
	_button_settings_ingame_bitmap_br_07,
	_button_settings_ingame_bitmap_dialog1,
	_button_settings_ingame_bitmap_dialog2,
	_button_settings_ingame_bitmap_settings_framing,
	_button_settings_ingame_bitmap_settings_framing2,
	_button_settings_ingame_bitmap_arrows_up,
	_button_settings_ingame_bitmap_arrows_down,
	_button_settings_ingame_bitmap_settings_framing3,
	_button_settings_ingame_bitmap_button_config,

	k_number_of_button_settings_ingame_bitmap_blocks
};

enum e_jumpy_ingame_pane_bitmap_blocks //qtr_screen
{
	_jumpy_ig_pane_bitmap_ul_07,
	_jumpy_ig_pane_bitmap_br_07,
	_jumpy_ig_pane_bitmap_dialog1,
	_jumpy_ig_pane_bitmap_dialog2,
	_jumpy_ig_pane_bitmap_settings_framing,
	_jumpy_ig_pane_bitmap_settings_framing2,
	_jumpy_ig_pane_bitmap_arrows_up,
	_jumpy_ig_pane_bitmap_arrows_down,

	//addtions
	_jumpy_ig_pane_bitmap_settings_framing3,
	_jumpy_ig_pane_bitmap_button_config,
	k_jumpy_ig_pane_bitmaps_count,

	k_orignal_jumpy_ig_pane_bitmaps_count = 8,
	k_number_of_jumpy_ingame_pane_bitmap_addons = k_jumpy_ig_pane_bitmaps_count - k_orignal_jumpy_ig_pane_bitmaps_count

};


enum e_button_settings_multilingual_unicode_string_list : uint32
{
	_string_id_l_header = 0x6000234,
	_string_id_l_default = _string_id_default,
	_string_id_l_south_paw = _string_id_south_paw,
	_string_id_l_jumpy = 0x5001A09,
	_string_id_l_boxer = _string_id_boxer,
	_string_id_l_green_thumb = _string_id_green_thumb,
	_string_id_l_button_fire = 0xB001A34,
	_string_id_l_button_throw_grenade = 0x14001A30,
	_string_id_l_button_reload = 0xD001A35,
	_string_id_l_button_switch_weapons = 0x15001A36,
	_string_id_l_button_melee = 0xC001A37,
	_string_id_l_button_jump = 0xB001A38,
	_string_id_l_button_swap_grenades = 0x14001A3A,
	_string_id_l_button_flashlight = 0x11001A39,
	_string_id_l_button_zoom = 0xB001A3B,
	_string_id_l_button_crouch = 0xD001A31,
	_string_id_l_button_score = 0xC001A32,
	_string_id_l_button_pause = 0xC001A33,
	_string_id_l_button_lean_left = 0x10001A41,
	_string_id_l_button_lean_right = 0x11001A42,
	_string_id_l_standard_help = 0xD002653,
	_string_id_l_southpaw_help = 0xD001A3C,
	_string_id_l_jumpy_help = 0xA001A43,
	_string_id_l_boxer_help = 0xA001A3D,
	_string_id_l_greenthumb_help = 0xF001A40,
	_string_id_l_default_ingame = 0xE002644,
	_string_id_l_south_paw_ingame = 0x10002661,
	_string_id_l_jumpy_ingame = 0xC001A45,
	_string_id_l_boxer_ingame = 0xC002665,
	_string_id_l_green_thumb_ingame = 0x12002667,
	_string_id_l_button_boxer_grenade = 0x14001A3F,
	_string_id_l_button_boxer_melee = 0x12001A3E,
};


enum e_button_settings_ingame_multilingual_unicode_string_list : uint32
{
	_string_id_l_ig_header = 0x6000234,
	_string_id_l_ig_default = _string_id_default,
	_string_id_l_ig_south_paw = _string_id_south_paw,
	_string_id_l_ig_jumpy = 0x5001A09,
	_string_id_l_ig_boxer = _string_id_boxer,
	_string_id_l_ig_green_thumb = _string_id_green_thumb,
	_string_id_l_ig_button_fire = 0xB002658,
	_string_id_l_ig_button_throw_grenade = 0x14002654,
	_string_id_l_ig_button_reload = 0xD002659,
	_string_id_l_ig_button_switch_weapons = 0x1500265A,
	_string_id_l_ig_button_melee = 0xC00265B,
	_string_id_l_ig_button_jump = 0xB00265C,
	_string_id_l_ig_button_swap_grenades = 0x1400265E,
	_string_id_l_ig_button_flashlight = 0x1100265D,
	_string_id_l_ig_button_zoom = 0xB00265F,
	_string_id_l_ig_button_crouch = 0xD002655,
	_string_id_l_ig_button_score = 0xC002656,
	_string_id_l_ig_button_pause = 0xC002657,
	_string_id_l_ig_button_lean_left = 0x1000267B,
	_string_id_l_ig_button_lean_right = 0x1100267C,
	_string_id_l_ig_standard_help = 0xD002653,
	_string_id_l_ig_southpaw_help = 0xD002660,
	_string_id_l_ig_jumpy_help = 0xA00267D,
	_string_id_l_ig_boxer_help = 0xA002662,
	_string_id_l_ig_greenthumb_help = 0xF002666,
	_string_id_l_ig_default_ingame = 0xE002644,
	_string_id_l_ig_south_paw_ingame = 0x10002661,
	_string_id_l_ig_jumpy_ingame = 0xC00267E,
	_string_id_l_ig_boxer_ingame = 0xC002665,
	_string_id_l_ig_green_thumb_ingame = 0x12002667,
	_string_id_l_ig_button_boxer_grenade = 0x14002664,
	_string_id_l_ig_button_boxer_melee = 0x12002663,
};

/* constants */

const wchar_t* g_jumpy_text_button_swap_left_string[k_language_count]
{
	L"Swap Left Weapon",
	L"左武器の交換",
	L"Linke Waffe tauschen",
	L"Échanger l'arme gauche",
	L"Cambiar arma izquierda",
	L"Scambia arma sinistra",
	L"왼쪽 무기 교체",
	L"交换左武器",
	L"Trocar Arma Esquerda"
};

/* prototypes */

c_button_settings_edit_list::c_button_settings_edit_list(int16 user_flags) :
	c_list_widget(user_flags),
	m_slot(this, &c_button_settings_edit_list::handle_item_pressed_event),
	m_qtr_screen(false)
{
	//we dont need s_list_item_datum here as no of list items remain same
	m_list_data = ui_list_data_new(k_button_setting_list_name, k_total_no_of_button_list_items, sizeof(datum));
	data_make_valid(m_list_data);

	for (int32 i = 0; i < m_list_data->datum_max_elements; ++i)
	{
		datum_new(m_list_data);
	}

	linker_type2.link(&m_slot);
}

void c_button_settings_edit_list::set_using_qtr_screen(bool param)
{
	this->m_qtr_screen = param;
}

c_list_item_widget* c_button_settings_edit_list::get_list_items()
{
	//return INVOKE_TYPE(0x25D425, 0x0, c_list_item_widget*(__thiscall*)(c_button_settings_edit_list*), this);
	return this->m_list_items;
}

int32 c_button_settings_edit_list::get_list_items_count()
{
	return k_no_of_visible_items_for_button_settings;
}

void c_button_settings_edit_list::update_list_items(c_list_item_widget* item, int32 skin_index)
{
	//INVOKE_TYPE(0x25D021, 0x0, void(__thiscall*)(c_button_settings_edit_list*, c_list_item_widget*, int32), this, item, skin_index);

	ASSERT(item);
	c_text_widget* item_text = item->try_find_text_widget(_default_list_skin_text_main);

	if (item_text)
	{
		switch (DATUM_INDEX_TO_ABSOLUTE_INDEX(item->get_last_data_index()))
		{
		case _item_standard:
			item_text->set_text_from_string_id(_string_id_l_default);
			break;
		case _item_south_paw:
			item_text->set_text_from_string_id(_string_id_l_south_paw);
			break;
		case _item_boxer:
			item_text->set_text_from_string_id(_string_id_l_boxer);
			break;
		case _item_green_thumb:
			item_text->set_text_from_string_id(_string_id_l_green_thumb);
			break;
		case _item_jumpy:
			item_text->set_text_from_string_id(_string_id_l_jumpy);
			break;
		default:
			item_text->set_text_from_string_id(_string_id_invalid);
			break;
		}
	}

}


void c_button_settings_edit_list::handle_item_pressed_event(s_event_record** pevent, datum* pitem_index)
{
	//INVOKE_TYPE(0x25D0AC, 0x0, void(__thiscall*)(c_button_settings_edit_list*, s_event_record**, datum*), this, pevent, pitem_index);

	s_gamepad_input_preferences preferences;
	e_button_preset_types selected_preset;

	switch (DATUM_INDEX_TO_ABSOLUTE_INDEX(*pitem_index))
	{
	case _item_south_paw:
		selected_preset = _button_preset_south_paw;
		break;
	case _item_boxer:
		selected_preset = _button_preset_boxer;
		break;
	case _item_green_thumb:
		selected_preset = _button_preset_green_thumb;
		break;
	case _item_jumpy:
		selected_preset = _button_preset_jumpy;
		break;
	default:
		selected_preset = _button_preset_default;
	}

	s_saved_game_player_profile* player_profile = user_interface_globals_get_edit_player_profile();

	player_profile->input_preferences.controller_button_layout = selected_preset;
	input_abstraction_set_preferences_from_controller_settings(&preferences, &player_profile->input_preferences);
	input_abstraction_get_default_preferences(&preferences, player_profile->input_preferences.controller_thumbstick_layout, selected_preset, player_profile->input_preferences.keyboard_preset_type, 0);
	input_abstraction_set_controller_settings_from_preferences(&preferences, &player_profile->input_preferences);

	// apparently ingame setting changes are directly commited to disk unlike settings screen which uses (edit_player_profile)
	if (this->m_qtr_screen)
		user_interface_globals_save_profile_changes_to_disk();

	user_interface_back_out_from_channel(this->get_parent_channel(), this->get_parent_render_window());
}


//
// c_screen_button_settings_menu class starts here
// 


c_screen_button_settings_menu::c_screen_button_settings_menu(e_user_interface_channel_type channel_type, e_user_interface_render_window window_index, int16 user_flags, e_user_interface_screen_id screen_id) :
	c_screen_with_menu(screen_id, channel_type, window_index, user_flags, &m_button_settings_list),
	m_button_settings_list(user_flags),
	field_D3C(NONE)
{
}

void c_screen_button_settings_menu::update()
{
	//INVOKE_TYPE(0x25d180, 0, void(__thiscall*)(c_screen_button_settings_menu*), this);

	if (this->get_any_responding_controller() != k_no_controller)
	{
		//const int32 unk_setting = return_0x0();
		const int32 unk_setting = 0;
		if (unk_setting != this->field_D3C)
		{
			if (unk_setting == 2)
				this->field_9FB = 5; // wgit pane offset pointer ?
			else
				this->field_9FB = 0;

			datum old_data_idx = this->m_button_settings_list.get_old_data_index();
			c_screen_widget::switch_panes(&old_data_idx);
			this->field_D3C = unk_setting;
		}
		if (this->m_using_qtr_arrows)
		{
			c_bitmap_widget* bitmap_down_arrow = this->try_find_bitmap_widget(_button_settings_ingame_bitmap_arrows_down);
			c_bitmap_widget* bitmap_up_arrow = this->try_find_bitmap_widget(_button_settings_ingame_bitmap_arrows_up);
			this->set_list_arrows_widget(bitmap_up_arrow, bitmap_down_arrow);

		}
	}
	c_user_interface_widget::update();
}

bool c_screen_button_settings_menu::handle_event(s_event_record* event)
{
	return INVOKE_TYPE(0x25d2d0, 0, bool(__thiscall*)(c_screen_button_settings_menu*, s_event_record*), this, event);
}

void c_screen_button_settings_menu::post_initialize()
{
	//INVOKE_TYPE(0x25d229, 0, void(__thiscall*)(c_screen_button_settings_menu*), this);

	//This menu class is designed to handle both button_settings.wgit and button_settings_ingame.wgit together
	//m_using_qtr_arrows == true only when button_settings_ingame.wgit is being used

	if (this->m_using_qtr_arrows)
	{
		c_bitmap_widget* bitmap_down_arrow = this->try_find_bitmap_widget(_button_settings_ingame_bitmap_arrows_down);
		c_bitmap_widget* bitmap_up_arrow = this->try_find_bitmap_widget(_button_settings_ingame_bitmap_arrows_up);
		this->set_list_arrows_widget(bitmap_up_arrow, bitmap_down_arrow);

	}

	//player_profile != nullptr when button_settings.wgit is being used inside player profile settings menu
	s_saved_game_player_profile* player_profile = user_interface_globals_get_edit_player_profile();
	if (player_profile)
	{
		switch (player_profile->input_preferences.controller_button_layout)
		{
		case _button_preset_south_paw:
			this->m_button_settings_list.set_focused_item_index(_item_south_paw);
			break;
		case _button_preset_boxer:
			this->m_button_settings_list.set_focused_item_index(_item_boxer);
			break;
		case _button_preset_green_thumb:
			this->m_button_settings_list.set_focused_item_index(_item_green_thumb);
			break;
		case _button_preset_jumpy:
			this->m_button_settings_list.set_focused_item_index(_item_jumpy);
			break;
		default:
			this->m_button_settings_list.set_focused_item_index(_item_standard);

		}
	}
	c_screen_widget::post_initialize();
}

void c_screen_button_settings_menu::post_initialize_button_keys()
{
	// this function is executed once upon every pane creation
	// thus can be used as on_pane_switch hook

	if (this->m_pane_index == _button_screen_pane_jumpy
		|| DATUM_INDEX_TO_ABSOLUTE_INDEX(this->m_button_settings_list.get_old_data_index()) == _item_jumpy)
	{
		c_text_widget* swap_left_text = this->try_find_screen_text(
			m_using_qtr_arrows ? _jumpy_ig_pane_text_button_swap_left : _jumpy_pane_text_button_swap_left);

		if (swap_left_text)
		{
			swap_left_text->set_text(g_jumpy_text_button_swap_left_string[get_current_language()]);
		}

	}

	// call orignal function now
	c_screen_widget::post_initialize_button_keys();
}

const void* c_screen_button_settings_menu::load_proc() const
{
	//return INVOKE_TYPE(0x256037, 0, void*(__thiscall*)(const c_screen_button_settings_menu*), this);
	return &c_screen_button_settings_menu::load;
}


void* c_screen_button_settings_menu::load(s_screen_parameters* parameters)
{
	//return INVOKE(0x255FA4, 0x0, c_screen_button_settings_menu::load, parameters);
	c_screen_button_settings_menu* screen;

	void* pool = ui_pool_allocate_space(sizeof(c_screen_button_settings_menu), 0);
	if (pool)
	{
		screen = new (pool) c_screen_button_settings_menu(
			parameters->m_channel_type,
			parameters->m_window_index,
			parameters->user_flags,
			_screen_pp_button_settings);

		screen->m_allocated = true;
		screen->m_using_qtr_arrows = false;
		user_interface_register_screen_to_channel(screen, parameters);
	}
	else
	{
		screen = NULL;
	}

	return screen;
}

void* c_screen_button_settings_menu::load_qtr(s_screen_parameters* parameters)
{
	//return INVOKE(0x259DC0, 0x0, c_screen_button_settings_menu::load_qtr, parameters);
	c_screen_button_settings_menu* screen;

	void* pool = ui_pool_allocate_space(sizeof(c_screen_button_settings_menu), 0);
	if (pool)
	{
		screen = new (pool) c_screen_button_settings_menu(
			parameters->m_channel_type,
			parameters->m_window_index,
			parameters->user_flags,
			_screen_pp_buttons_qtr); //qtr screen is button_settings_ingame.wgit

		screen->m_allocated = true;
		screen->m_button_settings_list.set_using_qtr_screen(true);
		screen->m_using_qtr_arrows = true;
		user_interface_register_screen_to_channel(screen, parameters);
	}
	else
	{
		screen = NULL;
	}

	return screen;
}

void c_screen_button_settings_menu::apply_patches_on_ui_map_load()
{
	const char* main_widget_tag_path = "ui\\screens\\game_shell\\settings_screen\\player_profile\\button_settings";

	datum main_widget_datum_index = tag_loaded(_tag_group_user_interface_screen_widget_definition, main_widget_tag_path);

	if (main_widget_datum_index == NONE)
	{
		LOG_ERROR_FUNC("bad datum found ");
		return;
	}

	s_user_interface_screen_widget_definition* main_widget_tag = (s_user_interface_screen_widget_definition*)tag_get_fast(main_widget_datum_index);

	main_widget_tag->panes[_button_screen_pane_standard]->list_block[0]->num_visible_items = k_no_of_visible_items_for_button_settings;
	//_button_screen_pane_south_paw to _button_screen_pane_unused7 all use same s_list_reference tag_block
	main_widget_tag->panes[_button_screen_pane_south_paw]->list_block[0]->num_visible_items = k_no_of_visible_items_for_button_settings;

	s_window_pane_reference* standard_pane = main_widget_tag->panes[_button_screen_pane_standard];
	s_window_pane_reference* jumpy_pane = main_widget_tag->panes[_button_screen_pane_jumpy];

	//fixup bitmap blocks
	if (standard_pane->bitmap_blocks.count)
	{
		csmemcpy(jumpy_pane->bitmap_blocks[0], standard_pane->bitmap_blocks[0], sizeof(s_bitmap_block_reference) * standard_pane->bitmap_blocks.count);
	}

	if (standard_pane->text_blocks.count)
	{
		//addon text blocks
		tag_injection_extend_block(&jumpy_pane->text_blocks, sizeof(s_text_block_reference), k_number_of_jumpy_pane_text_addons);
		csmemcpy(jumpy_pane->text_blocks[0], standard_pane->text_blocks[0], sizeof(s_text_block_reference) * standard_pane->text_blocks.count);
	}

	//start adjusting texts for jumpy
	jumpy_pane->text_blocks[_jumpy_pane_text_help]->string = _string_id_l_jumpy_help;
	jumpy_pane->text_blocks[_jumpy_pane_text_button_reload]->string = _string_id_l_button_reload;
	jumpy_pane->text_blocks[_jumpy_pane_text_button_melee]->string = _string_id_l_button_melee;
	jumpy_pane->text_blocks[_jumpy_pane_text_button_jump]->string = _string_id_l_button_jump;
	jumpy_pane->text_blocks[_jumpy_pane_text_button_flashlight]->string = _string_id_l_button_flashlight;
	jumpy_pane->text_blocks[_jumpy_pane_text_button_swap_grenades]->string = _string_id_l_button_swap_grenades;

	//reposition
	jumpy_pane->text_blocks[_jumpy_pane_text_button_flashlight]->text_bounds = { -60 ,-600,-100,-266 };
	jumpy_pane->text_blocks[_jumpy_pane_text_button_swap_grenades]->text_bounds = { -280,-550,-420,-216 };

	//additional texts to be added
	csmemcpy(jumpy_pane->text_blocks[_jumpy_pane_text_button_swap_left], jumpy_pane->text_blocks[_jumpy_pane_text_button_swap_grenades], sizeof(s_text_block_reference));
	jumpy_pane->text_blocks[_jumpy_pane_text_button_swap_left]->string = _string_id_invalid;
	jumpy_pane->text_blocks[_jumpy_pane_text_button_swap_left]->text_bounds = { -175,296,-215,640 };

}

void c_screen_button_settings_menu::apply_patches_on_mp_map_load()
{

	const char* ingame_widget_tag_path = "ui\\screens\\game_shell\\settings_screen\\player_profile\\button_settings_ingame";
	datum main_widget_datum_index = tag_loaded(_tag_group_user_interface_screen_widget_definition, ingame_widget_tag_path);

	if (main_widget_datum_index == NONE)
	{
		LOG_ERROR_FUNC("bad datum found ");
		return;
	}

	s_user_interface_screen_widget_definition* main_widget_tag = (s_user_interface_screen_widget_definition*)tag_get_fast(main_widget_datum_index);

	s_window_pane_reference* standard_pane = main_widget_tag->panes[_button_screen_pane_standard];
	s_window_pane_reference* jumpy_pane = main_widget_tag->panes[_button_screen_pane_jumpy];

	if (standard_pane->bitmap_blocks.count)
	{
		//add & fixup bitmap blocks
		tag_injection_extend_block(&jumpy_pane->bitmap_blocks, sizeof(s_bitmap_block_reference), k_number_of_jumpy_ingame_pane_bitmap_addons);
		csmemcpy(jumpy_pane->bitmap_blocks[0], standard_pane->bitmap_blocks[0], sizeof(s_bitmap_block_reference) * standard_pane->bitmap_blocks.count);
	}

	if (standard_pane->text_blocks.count)
	{
		//addon text blocks
		tag_injection_extend_block(&jumpy_pane->text_blocks, sizeof(s_text_block_reference), k_number_of_jumpy_ingame_pane_text_addons);
		csmemcpy(jumpy_pane->text_blocks[0], standard_pane->text_blocks[0], sizeof(s_text_block_reference) * standard_pane->text_blocks.count);
	}

	//start adjusting texts for jumpy
	jumpy_pane->text_blocks[_jumpy_ig_pane_text_help]->string = _string_id_l_ig_jumpy_help;
	jumpy_pane->text_blocks[_jumpy_ig_pane_text_button_reload]->string = _string_id_l_ig_button_reload;
	jumpy_pane->text_blocks[_jumpy_ig_pane_text_button_melee]->string = _string_id_l_ig_button_melee;
	jumpy_pane->text_blocks[_jumpy_ig_pane_text_button_jump]->string = _string_id_l_ig_button_jump;
	jumpy_pane->text_blocks[_jumpy_ig_pane_text_button_flashlight]->string = _string_id_l_ig_button_flashlight;
	jumpy_pane->text_blocks[_jumpy_ig_pane_text_button_swap_grenades]->string = _string_id_l_ig_button_swap_grenades;
	jumpy_pane->text_blocks[_jumpy_ig_pane_text_header]->string = _string_id_l_ig_jumpy_ingame;

	//reposition
	jumpy_pane->text_blocks[_jumpy_ig_pane_text_button_flashlight]->text_bounds = { 40 ,-540,0,-170 };
	jumpy_pane->text_blocks[_jumpy_ig_pane_text_button_swap_grenades]->text_bounds = { -180,-520,-320,-216 };

	//additional texts to be added
	csmemcpy(jumpy_pane->text_blocks[_jumpy_ig_pane_text_button_swap_left], jumpy_pane->text_blocks[_jumpy_ig_pane_text_button_swap_grenades], sizeof(s_text_block_reference));
	jumpy_pane->text_blocks[_jumpy_ig_pane_text_button_swap_left]->string = _string_id_invalid;
	jumpy_pane->text_blocks[_jumpy_ig_pane_text_button_swap_left]->text_bounds = { -75,326,-115,640 };

}

void c_screen_button_settings_menu::apply_instance_patches()
{
	//Replace orignal call with custom one inside c_controller_settings_edit_list::handle_item_pressed_event
	WriteValue(Memory::GetAddress(0x256214) + 4, c_screen_button_settings_menu::load);
	//Replace orignal call with custom one inside c_multiplayer_controller_settings_list::handle_item_pressed_event
	WriteValue(Memory::GetAddress(0x259F85) + 4, c_screen_button_settings_menu::load_qtr);
}
