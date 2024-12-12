#include "stdafx.h"
#include "screen_game_engine_category.h"

#include "interface/user_interface_memory.h"
#include "tag_files/global_string_ids.h"


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
	static s_item_text_mapping items_map[k_screen_game_engine_item_count] =
	{
		{_screen_game_engine_item_slayer, _string_id_slayer},
		{_screen_game_engine_item_king, _string_id_koth},
		{_screen_game_engine_item_oddball, _string_id_oddball},
		{_screen_game_engine_item_juggernaut, _string_id_juggernaut},
		{_screen_game_engine_item_ctf, _string_id_ctf},
		{_screen_game_engine_item_assault, _string_id_assault},
		{_screen_game_engine_item_territories, _string_id_territories},
		//{_screen_game_engine_item_zombies, _string_id_zombies}
	};

	//// todo: re-implement update_list into this function to handle non string-id strings

	//this->update_list_items_from_mapping(item, skin_index, 0, items_map, k_screen_game_engine_item_count);

	if (item == nullptr)
		return;

	c_text_widget* item_text = item->try_find_text_widget(0);
	if(item_text)
	{
		s_list_item_datum* item_datum = (s_list_item_datum*)datum_try_and_get(this->m_list_data, item->get_last_data_index());
		switch((e_screen_game_engine_items)item_datum->item_id)
		{
			case _screen_game_engine_item_zombies:
				item_text->set_text(L"Zombies");
				break;
			default:
				item_text->set_text_from_string_id(items_map[item_datum->item_id].item_text);
				break;
		}
	}
}

__declspec(naked) void jmp_c_screen_game_engine_category_list_update_list_items () { __asm { jmp c_screen_game_engine_category_list::update_list_items } };

void c_screen_game_engine_category_list::handle_item_pressed_event(s_event_record* pevent, datum* pitem_index)
{
	INVOKE_TYPE(0x249862, 0, void(__thiscall*)(c_screen_game_engine_category_list*, s_event_record*, datum*), this, pevent, pitem_index);
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

void c_screen_game_engine_category::apply_patches()
{
	WriteValue(Memory::GetAddress(0x25B6ED + 1), c_screen_game_engine_category::load);
	WriteValue(Memory::GetAddress(0x25B919 + 1), c_screen_game_engine_category::load);
}

void* c_screen_game_engine_category::load(s_screen_parameters* parameters)
{
	c_screen_game_engine_category* screen;

	void* pool = ui_pool_allocate_space(sizeof(c_screen_game_engine_category), 0);

	if(pool)
	{
		screen = new (pool) c_screen_game_engine_category(
			parameters->m_channel_type,
			parameters->m_window_index,
			parameters->user_flags,
			0,
			0,
			0
		);

		screen->m_allocated = true;

		screen->data[3] = 1;

		if (parameters->m_context)
			screen->data[4] = 1;


		

		user_interface_register_screen_to_channel(screen, parameters);
	}
	else
	{
		screen = NULL;
	}

	return screen;
}

c_screen_game_engine_category::c_screen_game_engine_category(e_user_interface_channel_type ui_channel, e_user_interface_render_window window_index,
                                                             uint16 user_flags, int8 unk_1, int8 unk_2, int8 unk_3) :
	c_screen_with_menu(_screen_game_engine_category_listing, ui_channel, window_index, user_flags, &m_game_engine_list),
	m_game_engine_list(user_flags)
{
	this->data[0] = unk_1;
	this->data[1] = unk_2;
	this->data[2] = unk_3;
	this->data[3] = 0;
	this->data[4] = 0;
}

void* c_screen_game_engine_category::load_proc()
{
	return &c_screen_game_engine_category::load;
}
