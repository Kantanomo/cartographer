#include "stdafx.h"
#include "screen_custom_game_profile_select.h"

#include "screen_game_engine_category.h"
#include "interface/user_interface_memory.h"
#include "saved_games/saved_game_files.h"

#define create_game_engine_load_functions(name, save_type) \
void* c_screen_custom_game_profile_select::load_##name##_settings(s_screen_parameters* parameters) { return load_settings(parameters, (save_type));  } \
void* c_screen_custom_game_profile_select::load_##name##_unused(s_screen_parameters* parameters){ return load_unused(parameters, (save_type)); } \
void* c_screen_custom_game_profile_select::load_##name##_lobby(s_screen_parameters* parameters) { return load_lobby(parameters, (save_type)); }

c_custom_game_profile_list::c_custom_game_profile_list(int16 user_flags) :
	c_list_widget(user_flags),
	enumerated_file_index_storage{},
	m_slot(this, &c_custom_game_profile_list::handle_item_pressed_event)
{
	m_list_data = ui_list_data_new(k_custom_game_profile_list_name, k_maximum_enumerated_total_save_game_files + 1,
	                               sizeof(s_custom_game_profile_list_item));

	data_make_valid(m_list_data);

	this->save_game_type = _saved_game_file_type_game_variant_slayer;
	this->unk_1 = NONE;
	this->enumerated_file_index = NONE;
	this->data_2[0] = 0;
	this->data_2[1] = 0;
	this->unk_bool = false;
	this->data_3 = 0;

	linker_type2.link(&this->m_slot);
}

void c_custom_game_profile_list::handle_item_pressed_event(s_event_record** pevent, datum* pitem_index)
{
	INVOKE_TYPE(0x251B7E, 0, void(__thiscall*)(c_custom_game_profile_list*, s_event_record**, datum*), this, pevent, pitem_index);
}

int32 c_custom_game_profile_list::setup_children()
{
	return INVOKE_TYPE(0x251AE9, 0, int32(__thiscall*)(c_custom_game_profile_list*), this);
}

void c_custom_game_profile_list::update()
{
	INVOKE_TYPE(0x251B35, 0, void(__thiscall*)(c_custom_game_profile_list*), this);
}

c_list_item_widget* c_custom_game_profile_list::get_list_items()
{
	return this->m_list_items;
}

int32 c_custom_game_profile_list::get_list_items_count()
{
	return k_game_profile_list_item_count;
}

void c_custom_game_profile_list::update_list_items(c_list_item_widget* item, int32 skin_index)
{
	INVOKE_TYPE(0x2513B6, 0, void(__thiscall*)(c_custom_game_profile_list*, c_list_item_widget*, int32), this, item, skin_index);
}

void* c_screen_custom_game_profile_select::load_settings(s_screen_parameters* parameters, e_saved_game_file_type type)
{
	c_screen_custom_game_profile_select* screen;

	void* pool = ui_pool_allocate_space(21648, 0);

	if (pool)
	{
		screen = new (pool) c_screen_custom_game_profile_select(
			parameters->m_channel_type,
			parameters->m_window_index,
			parameters->user_flags,
			_screen_custom_game_menu
		);

		screen->m_allocated = true;

		screen->save_game_type = type;

		screen->unk_data[0] = 1;
		screen->unk_data[1] = 0;

		screen->m_profile_list.unk_bool = parameters->m_context != nullptr;

		user_interface_register_screen_to_channel((c_screen_with_menu*)screen, parameters);
	}
	else
	{
		screen = nullptr;
	}

	return nullptr;
}

void* c_screen_custom_game_profile_select::load_lobby(s_screen_parameters* parameters, e_saved_game_file_type type)
{
	c_screen_custom_game_profile_select* screen;

	void* pool = ui_pool_allocate_space(21648, 0);

	if (pool)
	{
		screen = new (pool) c_screen_custom_game_profile_select(
			parameters->m_channel_type,
			parameters->m_window_index,
			parameters->user_flags,
			_screen_multiplayer_variant_listlobby
		);

		screen->m_allocated = true;

		screen->save_game_type = type;

		screen->unk_data[0] = 0;
		screen->unk_data[1] = 0;

		screen->m_profile_list.unk_bool = parameters->m_context != nullptr;

		user_interface_register_screen_to_channel((c_screen_with_menu*)screen, parameters);
	}
	else
	{
		screen = nullptr;
	}

	return nullptr;
}

void* c_screen_custom_game_profile_select::load_unused(s_screen_parameters* parameters, e_saved_game_file_type type)
{
	c_screen_custom_game_profile_select* screen;

	void* pool = ui_pool_allocate_space(21648, 0);

	if (pool)
	{
		screen = new (pool) c_screen_custom_game_profile_select(
			parameters->m_channel_type,
			parameters->m_window_index,
			parameters->user_flags,
			_screen_custom_game_menu
		);

		screen->m_allocated = true;

		screen->save_game_type = type;

		screen->unk_data[0] = 0;
		screen->unk_data[1] = 1;

		screen->m_profile_list.unk_bool = parameters->m_context != nullptr;

		user_interface_register_screen_to_channel((c_screen_with_menu*)screen, parameters);
	}
	else
	{
		screen = nullptr;
	}

	return nullptr;
}

create_game_engine_load_functions(slayer, _saved_game_file_type_game_variant_slayer);
create_game_engine_load_functions(king, _saved_game_file_type_game_variant_koth);
create_game_engine_load_functions(oddball, _saved_game_file_type_game_variant_oddball);
create_game_engine_load_functions(juggernaut, _saved_game_file_type_game_variant_juggernaut);
create_game_engine_load_functions(ctf, _saved_game_file_type_game_variant_ctf);
create_game_engine_load_functions(assault, _saved_game_file_type_game_variant_assault);
create_game_engine_load_functions(territories, _saved_game_file_type_game_variant_territories);
create_game_engine_load_functions(headhunter, _saved_game_file_type_game_variant_headhunter);

void c_screen_custom_game_profile_select::update()
{
	INVOKE_TYPE(0x25188C, 0, void(__thiscall*)(c_screen_custom_game_profile_select*), this);
}

void c_screen_custom_game_profile_select::sub_60E884()
{
	this->m_profile_list.save_game_type = this->save_game_type;
	this->m_profile_list.data_2[0] = this->unk_data[0];
	this->m_profile_list.data_2[1] = this->unk_data[1];
}

const void* c_screen_custom_game_profile_select::load_proc() const
{
	switch (this->save_game_type)
	{
	case _saved_game_file_type_game_variant_slayer :
		{
			if(this->unk_data[0])
				return &this->load_slayer_settings;
			if(this->unk_data[1])
				return &this->load_slayer_unused;

			return &this->load_slayer_lobby;
		}
	case _saved_game_file_type_game_variant_koth :
		{
			if (this->unk_data[0])
				return &this->load_king_settings;
			if (this->unk_data[1])
				return &this->load_king_unused;

			return &this->load_king_lobby;
		}
	case _saved_game_file_type_game_variant_oddball :
		{
			if (this->unk_data[0])
				return &this->load_oddball_settings;
			if (this->unk_data[1])
				return &this->load_oddball_unused;

			return &this->load_oddball_lobby;
		}
	case _saved_game_file_type_game_variant_juggernaut :
		{
			if (this->unk_data[0])
				return &this->load_juggernaut_settings;
			if (this->unk_data[1])
				return &this->load_juggernaut_unused;

			return &this->load_juggernaut_lobby;
		}
	case _saved_game_file_type_game_variant_ctf :
		{
			if (this->unk_data[0])
				return &this->load_ctf_settings;
			if (this->unk_data[1])
				return &this->load_ctf_unused;

			return &this->load_ctf_lobby;
		}
	case _saved_game_file_type_game_variant_assault :
		{
			if (this->unk_data[0])
				return &this->load_assault_settings;
			if (this->unk_data[1])
				return &this->load_assault_unused;

			return &this->load_assault_lobby;
		}
	case _saved_game_file_type_game_variant_territories :
		{
			if (this->unk_data[0])
				return &this->load_territories_settings;
			if (this->unk_data[1])
				return &this->load_territories_unused;

			return &this->load_territories_lobby;
		}
	case _saved_game_file_type_game_variant_headhunter :
		{
			if (this->unk_data[0])
				return &this->load_headhunter_settings;
			if (this->unk_data[1])
				return &this->load_headhunter_unused;

			return &this->load_headhunter_lobby;
		}
	default:
		return &this->load_slayer_settings;
	}
}

c_screen_custom_game_profile_select::c_screen_custom_game_profile_select(e_user_interface_channel_type ui_channel, e_user_interface_render_window window_index, uint16 user_flags, e_user_interface_screen_id screen_id) :
	c_screen_with_menu(screen_id, ui_channel, window_index, user_flags, &m_profile_list),
	m_profile_list(user_flags),
	unk_data{}
{
	this->save_game_type = _saved_game_file_type_game_variant_slayer;
}

#undef create_game_engine_load_functions