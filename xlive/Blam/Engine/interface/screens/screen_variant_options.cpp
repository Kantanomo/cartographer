#include "stdafx.h"
#include "screen_variant_options.h"

#include "screen_variant_parameter_setting.h"
#include "cache/cache_files.h"
#include "interface/user_interface_bitmap_block.h"
#include "interface/user_interface_memory.h"
#include "networking/logic/life_cycle_manager.h"
#include "networking/session/network_session.h"
#include "text/text_group.h"

/* ------------------------ */
/* -c_variant_options_list- */
/* ------------------------ */





int32 c_variant_options_list::link_item_widgets()
{
	if (this->m_variant_setting_category_type == _variant_setting_category_type_none)
		this->m_variant_setting_category_type = g_previous_variant_setting_category;
	else
		g_previous_variant_setting_category = this->m_variant_setting_category_type;

	if (this->m_variant_setting_category_type != _variant_setting_category_type_game_head_hunter)
	{
		s_variant_setting_edit_reference* variant_reference = multiplayer_variant_settings_interface_get_category_reference(this->m_variant_setting_category_type);

		if (variant_reference && variant_reference->options.count > 0)
		{
			this->m_list_data = ui_list_data_new(k_variant_options_list_name, variant_reference->options.count, sizeof(s_variant_options_list_item));
			data_make_valid(this->m_list_data);
			if (this->m_list_data->datum_max_elements > 0)
			{
				for (int32 i = 0; i < this->m_list_data->datum_max_elements; ++i)
				{
					tag_reference* sily_reference = variant_reference->options[i];
					s_variant_options_list_item* list_item = (s_variant_options_list_item*)datum_get(this->m_list_data, datum_new(this->m_list_data));

					if (sily_reference->index != NONE)
						list_item->sily_definition = (s_text_value_pair_definition*)tag_get_fast(sily_reference->index);
					else
						list_item->sily_definition = nullptr;
				}
			}
			this->linker_type2.link(&this->m_slot);
			c_list_widget::link_item_widgets();
		}
		else
		{
			this->m_list_data = ui_list_data_new(k_variant_options_list_empty_name, 1, sizeof(s_variant_options_list_item));
			data_make_valid(this->m_list_data);

			s_variant_options_list_item* list_item = (s_variant_options_list_item*)datum_get(this->m_list_data, datum_new(this->m_list_data));

			list_item->sily_definition = nullptr;

			this->linker_type2.link(&this->m_slot);
			c_list_widget::link_item_widgets();
		}
	}
	else
	{
		this->m_list_data = ui_list_data_new(k_variant_options_list_name, k_multiplayer_variant_headhunter_parameter_count, sizeof(s_variant_options_list_item));
		data_make_valid(this->m_list_data);
		if(this->m_list_data->datum_max_elements > 0)
		{
			for(int32 i = 0; i < this->m_list_data->datum_max_elements; ++i)
			{
				s_variant_options_list_item* list_item = (s_variant_options_list_item*)datum_get(this->m_list_data, datum_new(this->m_list_data));
			}
		}
		this->linker_type2.link(&this->m_slot);
		c_list_widget::link_item_widgets();
	}

	return 1;
}

c_list_item_widget* c_variant_options_list::get_list_items()
{
	return m_items;
}

int32 c_variant_options_list::get_list_items_count()
{
	return k_variant_options_list_item_count;
}

void c_variant_options_list::update_list_items(c_list_item_widget* item, int32 skin_index)
{
	if(item->get_last_data_index() != NONE)
	{
		s_variant_options_list_item* list_item = (s_variant_options_list_item*)datum_get(this->m_list_data, item->get_last_data_index());

		c_text_widget* title_text = item->try_find_text_widget(0);
		c_text_widget* value_text = item->try_find_text_widget(1);

		if (this->m_variant_setting_category_type != _variant_setting_category_type_game_head_hunter)
		{
			if (list_item && list_item->sily_definition && list_item->sily_definition->string_list.index != NONE)
			{
				wchar_t temp_string[512];
				if (title_text)
				{
					temp_string[0] = '\0';
					text_group_get_unicode_string(list_item->sily_definition->string_list.index, list_item->sily_definition->title_text, temp_string);
					title_text->set_text(temp_string);
				}
				if (value_text)
				{
					s_game_variant* variant;

					if (!this->m_is_quick_options)
						variant = user_interface_get_variant();
					else
					{
						c_network_session* network_session;
						if (network_life_cycle_in_squad_session(&network_session))
							variant = &network_session->m_session_parameters.game_variant;
						else
							variant = nullptr;
					}
					if (variant)
					{
						int32 variant_setting_value = multiplayer_variant_settings_interface_get_variant_parameter_value(variant, list_item->sily_definition->parameter);
						s_text_value_pair_reference_new* variant_setting_label = multiplayer_variant_settings_interface_get_variant_parameter_label(list_item->sily_definition, variant_setting_value);
						temp_string[0] = '\0';
						if (variant_setting_label)
						{
							text_group_get_unicode_string(list_item->sily_definition->string_list.index, variant_setting_label->label_string, temp_string);
							value_text->set_text(temp_string);
						}
					}
				}
			}
		}
		else
		{
			s_game_variant* variant;

			if (!this->m_is_quick_options)
				variant = user_interface_get_variant();
			else
			{
				c_network_session* network_session;
				if (network_life_cycle_in_squad_session(&network_session))
					variant = &network_session->m_session_parameters.game_variant;
				else
					variant = nullptr;
			}
			if (variant)
			{
				if (title_text)
				{
					title_text->set_text(multiplayer_variant_settings_interface_get_variant_parameter_title_direct(variant, DATUM_INDEX_TO_ABSOLUTE_INDEX(item->get_last_data_index())));
				}
				if (value_text)
				{
					wchar_t temp_string[512]{};

					multiplayer_variant_settings_interface_get_variant_parameter_label_direct(variant, g_multiplayer_variant_interface_headhunter_parameter_types[DATUM_INDEX_TO_ABSOLUTE_INDEX(item->get_last_data_index())], temp_string);

					value_text->set_text(temp_string);
				}
			}
		}
	}
}

void c_variant_options_list::set_quick_options(bool state)
{
	this->m_is_quick_options = state;
}

void c_variant_options_list::set_variant_setting_category_type(e_variant_setting_category_type category)
{
	this->m_variant_setting_category_type = category;
}


void c_variant_options_list::handle_item_pressed_event(s_event_record** event, datum* pitem_index)
{
	//INVOKE_TYPE(0x2585EC, 0, void(__thiscall*)(c_variant_options_list*, s_event_record**, datum*), this, event, pitem_index);

	s_variant_options_list_item* item = (s_variant_options_list_item*)datum_get(this->m_list_data, *pitem_index);
	if(item)
	{
		if(item->sily_definition)
		{
			c_screen_variant_parameter_setting::new_instance(
				item->sily_definition->parameter,
				_user_interface_channel_type_online_menu,
				_window_4,
				this->m_controllers_mask,
				this->m_is_quick_options
			);
		}
	}
}

c_variant_options_list::c_variant_options_list(uint16 user_flags):
	c_list_widget(user_flags),
	m_variant_setting_category_type(_variant_setting_category_type_none),
	m_slot(this, &c_variant_options_list::handle_item_pressed_event),
	m_variant_setting_reference(nullptr),
	m_is_quick_options(false)
{
}


/* -------------------------- */
/* -c_screen_variant_options- */
/* -------------------------- */

void c_screen_variant_options::update()
{
	c_network_session* network_session;
	if(network_life_cycle_in_squad_session(&network_session))
	{
		s_game_variant* variant = &network_session->m_session_parameters.game_variant;
		c_bitmap_widget* bitmap_widget = this->try_find_bitmap_widget(1);
		if (bitmap_widget)
			user_interface_set_bitmap_from_variant(variant, bitmap_widget);
	}
	c_user_interface_widget::update();
}

const void* c_screen_variant_options::load_proc() const
{
	if (this->m_list.m_is_quick_options)
		return &c_screen_variant_options::load_quick_options;
	else
		return &c_screen_variant_options::load_editor;
}

void c_screen_variant_options::set_variant_setting_category_type(e_variant_setting_category_type category)
{
	this->m_list.set_variant_setting_category_type(category);
}

void* c_screen_variant_options::load_editor(s_screen_parameters* parameters)
{
	c_screen_variant_options* screen;

	void* pool = ui_pool_allocate_space(sizeof(c_screen_variant_options), 1);

	if (pool)
	{
		screen = new (pool) c_screen_variant_options(
			parameters->m_channel_type,
			parameters->m_window_index,
			parameters->user_flags,
			_screen_variant_editing_format
		);

		screen->m_allocated = true;
		screen->m_list.set_quick_options(false);

		user_interface_register_screen_to_channel(screen, parameters);
	}
	else
	{
		return nullptr;
	}

	return screen;
}

void* c_screen_variant_options::load_quick_options(s_screen_parameters* parameters)
{
	c_screen_variant_options* screen;

	void* pool = ui_pool_allocate_space(sizeof(c_screen_variant_options), 1);

	if(pool)
	{
		screen = new (pool) c_screen_variant_options(
			parameters->m_channel_type,
			parameters->m_window_index,
			parameters->user_flags,
			_screen_variant_quick_options_format
		);

		screen->m_allocated = true;
		screen->m_list.set_quick_options(true);

		user_interface_register_screen_to_channel(screen, parameters);
	}
	else
	{
		return nullptr;
	}

	return screen;
}

void c_screen_variant_options::new_instance(e_variant_setting_category_type category,
	e_user_interface_channel_type channel, e_user_interface_render_window window, int16 user_flags)
{
	proc_ui_screen_load_cb_t load_func = c_screen_variant_options::load_editor;
	for (e_variant_setting_category_type g_variant_setting_category_type_quick_option : g_variant_setting_category_type_quick_options)
	{
		if (category == g_variant_setting_category_type_quick_option)
			load_func = c_screen_variant_options::load_quick_options;
	}

	s_screen_parameters params;
	params.m_flags = 0;
	params.m_window_index = window;
	params.m_context = NULL;
	params.user_flags = user_flags;
	params.m_channel_type = channel;
	params.m_screen_state.field_0 = 0xFFFFFFFF;
	params.m_screen_state.m_last_focused_item_order = 0xFFFFFFFF;
	params.m_screen_state.m_last_focused_item_index = 0xFFFFFFFF;
	params.m_load_function = load_func;

	((c_screen_variant_options*)params.ui_screen_load_proc_exec())->set_variant_setting_category_type(category);
}

c_screen_variant_options::c_screen_variant_options(e_user_interface_channel_type ui_channel, e_user_interface_render_window window_index, uint16 user_flags, e_user_interface_screen_id screen_id) :
	c_screen_with_menu(screen_id, ui_channel, window_index, user_flags, &m_list),
	m_list(user_flags)
{
}
