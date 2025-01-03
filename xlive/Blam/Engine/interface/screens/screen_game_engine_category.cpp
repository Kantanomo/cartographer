#include "stdafx.h"
#include "screen_game_engine_category.h"

#include "screen_custom_game_profile_select.h"
#include "screen_virtual_keyboard.h"
#include "interface/user_interface_controller.h"
#include "interface/user_interface_globals.h"
#include "interface/user_interface_memory.h"
#include "main/game_preferences.h"
#include "saved_games/saved_game_files.h"
#include "tag_files/global_string_ids.h"

/* private */

e_saved_game_file_type get_saved_game_file_type_from_item_index(uint32 index)
{
	switch ((e_screen_game_engine_items)index)
	{
		case _screen_game_engine_item_slayer:
			return _saved_game_file_type_game_variant_slayer;
		case _screen_game_engine_item_king:
			return _saved_game_file_type_game_variant_koth;
		case _screen_game_engine_item_oddball:
			return _saved_game_file_type_game_variant_oddball;
		case _screen_game_engine_item_juggernaut:
			return _saved_game_file_type_game_variant_juggernaut;
		case _screen_game_engine_item_ctf:
			return _saved_game_file_type_game_variant_ctf;
		case _screen_game_engine_item_assault:
			return _saved_game_file_type_game_variant_assault;
		case _screen_game_engine_item_territories:
			return _saved_game_file_type_game_variant_territories;
		case _screen_game_engine_item_zombies:
			// todo: new default_string_id_type
			return _saved_game_file_type_game_variant_slayer;
	}

	return _saved_game_file_type_profile;
}


int32 c_screen_game_engine_category_list::setup_children()
{
	return INVOKE_TYPE(0x249F46, 0, int32(__thiscall*)(c_screen_game_engine_category_list*), this);
}

c_list_item_widget* c_screen_game_engine_category_list::get_list_items()
{
	return this->m_list_items;
}

int32 c_screen_game_engine_category_list::get_list_items_count()
{
	return k_screen_game_engine_item_count;
}

__declspec(naked) void jmp_c_screen_game_engine_category_list_get_list_count() { __asm { jmp c_screen_game_engine_category_list::get_list_items_count } };

void c_screen_game_engine_category_list::update_list_items(c_list_item_widget* item, int32 skin_index)
{
	const static s_item_text_mapping items_map[k_screen_game_engine_item_count] =
	{
		{_screen_game_engine_item_slayer, _string_id_slayer},
		{_screen_game_engine_item_king, _string_id_koth},
		{_screen_game_engine_item_oddball, _string_id_oddball},
		{_screen_game_engine_item_juggernaut, _string_id_juggernaut},
		{_screen_game_engine_item_ctf, _string_id_ctf},
		{_screen_game_engine_item_assault, _string_id_assault},
		{_screen_game_engine_item_territories, _string_id_territories}
	};

	const static wchar_t* zombies_strings[k_language_count]
	{
		L"Zombies",
		L"ゾンビ",
		L"Zombies",
		L"Zombi",
		L"Zombi",
		L"Zombi",
		L"좀비",
		L"僵尸",
		L"Zumbis"
	};

	if (item == nullptr)
		return;

	c_text_widget* item_text = item->try_find_text_widget(0);
	if(item_text)
	{
		s_list_item_datum* item_datum = (s_list_item_datum*)datum_try_and_get(this->m_list_data, item->get_last_data_index());
		const e_language language = get_current_language();
		switch((e_screen_game_engine_items)item_datum->item_id)
		{
			case _screen_game_engine_item_zombies:
				// TODO: if we ever do a map recompile grab the text from the stringid again
				item_text->set_text(zombies_strings[language]);
				break;
			default:
				item_text->set_text_from_string_id(items_map[item_datum->item_id].item_text);
				break;
		}
	}
}

__declspec(naked) void jmp_c_screen_game_engine_category_list_update_list_items () { __asm { jmp c_screen_game_engine_category_list::update_list_items } };

void c_screen_game_engine_category_list::handle_item_pressed_event(s_event_record** pevent, datum* pitem_index)
{
	s_list_item_datum* item = (s_list_item_datum*)datum_get(this->m_list_data, *pitem_index);

	if (item)
	{
		if(this->data[2])
		{
			s_saved_game_player_profile* edit_profile = user_interface_globals_get_edit_player_profile();
			if (edit_profile)
			{
				uint8 unk_type = item->item_id;
				switch (item->item_id)
				{
				case 0:
					unk_type = 0;
					break;
				case 1:
					unk_type = 3;
					break;
				case 2:
					unk_type = 1;
					break;
				case 3:
					unk_type = 2;
					break;
				case 4:
					unk_type = 4;
					break;
				case 5:
					unk_type = 5;
					break;
				case 6:
					unk_type = 6;
					break;
				}
				edit_profile->variant.data[0] = unk_type;
				user_interface_globals_save_edit_profile_to_disk();
			}
			if (!this->data[3])
			{
				user_interface_back_out_from_channel(this->get_parent_channel(), this->get_parent_render_window());
				return;
			}
		}

		if (this->data[0])
		{
			e_saved_game_file_type saved_game_file_type = get_saved_game_file_type_from_item_index(item->item_id);

			wchar_t saved_game_file_name[128]{};

			s_saved_game_files_globals* saved_game_globals = saved_game_files_globals_get();

			saved_game_globals->saved_file_creation_result = _saved_gave_disk_result_success;

			if (!saved_games_generate_file_name(saved_game_file_type, saved_game_file_name))
			{
				DISPLAY_ASSERT("Failed to create a name for a new variant!");
				item_pressed_event_error();
			}
			else
			{
				uint32 new_variant_enumerated_index = saved_games_create_new_game_variant((*pevent)->controller, saved_game_file_type, saved_game_file_name);
				if (new_variant_enumerated_index == UINT_MAX)
				{
					DISPLAY_ASSERT("Failed to write new variant to disk");
					item_pressed_event_error();
				}
				else
				{
					s_game_variant new_variant{};

					if (!saved_games_load_game_variant(new_variant_enumerated_index, &new_variant))
					{
						DISPLAY_ASSERT("Failed to load new variant from disk");
						item_pressed_event_error();
					}
					else
					{
						user_interface_set_variant(new_variant_enumerated_index, &new_variant);
						s_game_variant* interface_variant = user_interface_get_variant();
						ui_load_virtual_keyboard(interface_variant->variant_name, NUMBEROF(interface_variant->variant_name), _vkbd_context_variant_name_entry);
					}
				}
			}
		}
		else
		{
			s_screen_parameters params;
			params.m_flags = 0;
			params.m_window_index = _window_4;
			params.m_context = NULL;
			params.user_flags = FLAG((*pevent)->controller);
			params.m_channel_type = _user_interface_channel_type_gameshell_screen;
			params.m_screen_state.field_0 = 0xFFFFFFFF;
			params.m_screen_state.m_last_focused_item_order = 0xFFFFFFFF;
			params.m_screen_state.m_last_focused_item_index = 0xFFFFFFFF;

			switch ((e_screen_game_engine_items)item->item_id)
			{
			case _screen_game_engine_item_slayer:
				if (this->type == _screen_game_engine_category_settings)
					params.m_load_function = c_screen_custom_game_profile_select::load_slayer_settings;
				else if (!this->data[1])
					params.m_load_function = c_screen_custom_game_profile_select::load_slayer_settings_unused;
				else
					params.m_load_function = c_screen_custom_game_profile_select::load_slayer_lobby;
				break;
			case _screen_game_engine_item_king:
				if (this->type == _screen_game_engine_category_settings)
					params.m_load_function = c_screen_custom_game_profile_select::load_king_settings;
				else if (!this->data[1])
					params.m_load_function = c_screen_custom_game_profile_select::load_king_settings_unused;
				else
					params.m_load_function = c_screen_custom_game_profile_select::load_king_lobby;
				break;
			case _screen_game_engine_item_oddball:
				if (this->type == _screen_game_engine_category_settings)
					params.m_load_function = c_screen_custom_game_profile_select::load_oddball_settings;
				else if (!this->data[1])
					params.m_load_function = c_screen_custom_game_profile_select::load_oddball_settings_unused;
				else
					params.m_load_function = c_screen_custom_game_profile_select::load_oddball_lobby;
				break;
			case _screen_game_engine_item_juggernaut:
				if (this->type == _screen_game_engine_category_settings)
					params.m_load_function = c_screen_custom_game_profile_select::load_juggernaut_settings;
				else if (!this->data[1])
					params.m_load_function = c_screen_custom_game_profile_select::load_juggernaut_settings_unused;
				else
					params.m_load_function = c_screen_custom_game_profile_select::load_juggernaut_lobby;
				break;
			case _screen_game_engine_item_ctf:
				if (this->type == _screen_game_engine_category_settings)
					params.m_load_function = c_screen_custom_game_profile_select::load_ctf_settings;
				else if (!this->data[1])
					params.m_load_function = c_screen_custom_game_profile_select::load_ctf_settings_unused;
				else
					params.m_load_function = c_screen_custom_game_profile_select::load_ctf_lobby;
				break;
			case _screen_game_engine_item_assault:
				if (this->type == _screen_game_engine_category_settings)
					params.m_load_function = c_screen_custom_game_profile_select::load_assault_settings;
				else if (!this->data[1])
					params.m_load_function = c_screen_custom_game_profile_select::load_assault_settings_unused;
				else
					params.m_load_function = c_screen_custom_game_profile_select::load_assault_lobby;
				break;
			case _screen_game_engine_item_territories:
				if (this->type == _screen_game_engine_category_settings)
					params.m_load_function = c_screen_custom_game_profile_select::load_territories_settings;
				else if (!this->data[1])
					params.m_load_function = c_screen_custom_game_profile_select::load_territories_settings_unused;
				else
					params.m_load_function = c_screen_custom_game_profile_select::load_territories_lobby;
				break;
			case _screen_game_engine_item_zombies:
				// todo: new load functions for zombies
				if (this->type == _screen_game_engine_category_settings)
					params.m_load_function = c_screen_custom_game_profile_select::load_slayer_settings;
				else if (!this->data[1])
					params.m_load_function = c_screen_custom_game_profile_select::load_slayer_settings_unused;
				else
					params.m_load_function = c_screen_custom_game_profile_select::load_slayer_lobby;
				break;
			}

			if (this->data[3])
				++(*(DWORD*)params.m_context);

			if (this->type != _screen_game_engine_category_settings && !this->data[1])
				params.m_channel_type = _user_interface_channel_type_online_menu;

			params.m_load_function(&params);
		}
	}


	//INVOKE_TYPE(0x249862, 0, void(__thiscall*)(c_screen_game_engine_category_list*, s_event_record**, datum*), this, pevent, pitem_index);
}

void c_screen_game_engine_category_list::item_pressed_event_error() const
{
	s_saved_game_files_globals* saved_game_globals = saved_game_files_globals_get();
	INVOKE_TYPE(0x209B0F, 0, void(__cdecl*)(int16, e_saved_game_disk_result), this->m_controllers_mask, saved_game_globals->saved_file_creation_result);
}

void c_screen_game_engine_category_list::apply_patches()
{
	//WritePointer(Memory::GetAddress(0x3D7FF0), jmp_c_screen_game_engine_category_list_get_list_count);
	//WritePointer(Memory::GetAddress(0x3D7FF4), jmp_c_screen_game_engine_category_list_update_list_items);
}

c_screen_game_engine_category_list::c_screen_game_engine_category_list(int16 user_flags) :
	c_list_widget(user_flags),
	m_slot(this, &c_screen_game_engine_category_list::handle_item_pressed_event),
	data{}
{
	m_list_data = ui_list_data_new(k_game_engine_category_list_name, k_screen_game_engine_item_count, sizeof(s_list_item_datum));

	data_make_valid(m_list_data);

	for (int16 i = 0; i < k_screen_game_engine_item_count; i++)
	{
		((s_list_item_datum*)datum_get(this->m_list_data, datum_new(this->m_list_data)))->item_id = i;
	}

	linker_type2.link(&this->m_slot);
}

void* c_screen_game_engine_category::load_settings(s_screen_parameters* parameters)
{
	c_screen_game_engine_category* screen;

	void* pool = ui_pool_allocate_space(sizeof(c_screen_game_engine_category), 0);

	if(pool)
	{
		screen = new (pool) c_screen_game_engine_category(
			parameters->m_channel_type,
			parameters->m_window_index,
			parameters->user_flags,
			_screen_game_engine_category_listing,
			_screen_game_engine_category_settings,
			0,
			0
		);

		screen->m_allocated = true;

		user_interface_register_screen_to_channel(screen, parameters);
	}
	else
	{
		screen = NULL;
	}

	return screen;
}

void* c_screen_game_engine_category::load_lobby(s_screen_parameters* parameters)
{
	c_screen_game_engine_category* screen;

	void* pool = ui_pool_allocate_space(sizeof(c_screen_game_engine_category), 0);

	if (pool)
	{
		screen = new (pool) c_screen_game_engine_category(
			parameters->m_channel_type,
			parameters->m_window_index,
			parameters->user_flags,
			_screen_multiplayer_variant_typelobby,
			_screen_game_engine_category_lobby,
			0,
			0
		);

		screen->m_allocated = true;

		screen->data[2] = 0;
		screen->data[3] = 0;

		user_interface_register_screen_to_channel(screen, parameters);
	}
	else
	{
		screen = NULL;
	}

	return screen;
}

void* c_screen_game_engine_category::load_1(s_screen_parameters* parameters)
{
	c_screen_game_engine_category* screen;

	void* pool = ui_pool_allocate_space(sizeof(c_screen_game_engine_category), 0);

	if (pool)
	{
		screen = new (pool) c_screen_game_engine_category(
			parameters->m_channel_type,
			parameters->m_window_index,
			parameters->user_flags,
			_screen_multiplayer_variant_typelobby,
			_screen_game_engine_category_lobby,
			1,
			0
		);

		screen->m_allocated = true;

		user_interface_register_screen_to_channel(screen, parameters);
	}
	else
	{
		screen = NULL;
	}

	return screen;
}

void* c_screen_game_engine_category::load_2(s_screen_parameters* parameters)
{
	c_screen_game_engine_category* screen;

	void* pool = ui_pool_allocate_space(sizeof(c_screen_game_engine_category), 0);

	if (pool)
	{
		screen = new (pool) c_screen_game_engine_category(
			parameters->m_channel_type,
			parameters->m_window_index,
			parameters->user_flags,
			_screen_multiplayer_variant_typelobby,
			_screen_game_engine_category_lobby,
			0,
			0
		);

		screen->m_allocated = true;

		user_interface_register_screen_to_channel(screen, parameters);
	}
	else
	{
		screen = NULL;
	}

	return screen;
}

void* c_screen_game_engine_category::load_3(s_screen_parameters* parameters)
{
	c_screen_game_engine_category* screen;

	void* pool = ui_pool_allocate_space(sizeof(c_screen_game_engine_category), 0);

	if (pool)
	{
		screen = new (pool) c_screen_game_engine_category(
			parameters->m_channel_type,
			parameters->m_window_index,
			parameters->user_flags,
			_screen_multiplayer_variant_typelobby,
			_screen_game_engine_category_lobby,
			0,
			1
		);

		screen->m_allocated = true;

		screen->data[2] = 0;
		screen->data[3] = 0;

		user_interface_register_screen_to_channel(screen, parameters);
	}
	else
	{
		screen = NULL;
	}

	return screen;
}

void* c_screen_game_engine_category::load_4(s_screen_parameters* parameters)
{
	c_screen_game_engine_category* screen;

	void* pool = ui_pool_allocate_space(sizeof(c_screen_game_engine_category), 0);

	if (pool)
	{
		screen = new (pool) c_screen_game_engine_category(
			parameters->m_channel_type,
			parameters->m_window_index,
			parameters->user_flags,
			_screen_multiplayer_variant_typelobby,
			_screen_game_engine_category_lobby,
			0,
			0
		);

		screen->m_allocated = true;

		screen->data[2] = 1;

		if (parameters->m_context)
			screen->data[3] = 1;

		screen->m_game_engine_list.data[2] = 1;
		screen->m_game_engine_list.data[3] = screen->data[3];
			
		user_interface_register_screen_to_channel(screen, parameters);
	}
	else
	{
		screen = NULL;
	}

	return screen;
}

c_screen_game_engine_category::c_screen_game_engine_category(e_user_interface_channel_type ui_channel, e_user_interface_render_window window_index,
                                                             uint16 user_flags, e_user_interface_screen_id screen_id, e_screen_game_engine_category_type type, int8 unk_2, int8 unk_3) :
	c_screen_with_menu(screen_id, ui_channel, window_index, user_flags, &m_game_engine_list),
	m_game_engine_list(user_flags),
	data{}
{
	this->m_type = type;
	this->data[0] = unk_2;
	this->data[1] = unk_3;
	this->data[2] = 0;
	this->data[3] = 0;
}

void c_screen_game_engine_category::sub_60E884()
{
	this->m_game_engine_list.type = this->m_type;
	this->m_game_engine_list.data[0] = this->data[0];
	this->m_game_engine_list.data[1] = this->data[1];
}

const void* c_screen_game_engine_category::load_proc() const
{
	if (this->data[0])
		return &c_screen_game_engine_category::load_1;
	if(this->m_type == _screen_game_engine_category_settings)
		return &c_screen_game_engine_category::load_settings;
	if (!this->data[1])
		return &c_screen_game_engine_category::load_lobby;

	return &c_screen_game_engine_category::load_3;
}

__declspec(naked) void jmp_c_screen_game_engine_category_load_proc() { __asm { jmp c_screen_game_engine_category::load_proc } };

void c_screen_game_engine_category::apply_patches()
{
	WriteValue(Memory::GetAddress(0x25B6ED + 1), c_screen_game_engine_category::load_4);
	WriteValue(Memory::GetAddress(0x25B919 + 1), c_screen_game_engine_category::load_4);
	WriteValue(Memory::GetAddress(0x24F9F1 + 1), c_screen_game_engine_category::load_lobby);
	WritePointer(Memory::GetAddress(0x3D034C), jmp_c_screen_game_engine_category_load_proc);
}
