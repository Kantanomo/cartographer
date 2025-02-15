#include "stdafx.h"
#include "screen_variant_parameter_setting.h"

#include "interface/user_interface_memory.h"
#include "text/text_group.h"


/* ---------------------------------- */
/* -c_variant_parameter_setting_list- */
/* ---------------------------------- */

c_variant_parameter_setting_list::c_variant_parameter_setting_list(uint16 user_flags) :
	c_list_widget(user_flags),
	m_slot(this, &c_variant_parameter_setting_list::handle_item_pressed_event),
	m_variant_setting_parameter_type(_variant_setting_parameter_type_invalid),
	sily_definition(nullptr),
	field_3E8(0)
{
}

int32 c_variant_parameter_setting_list::link_item_widgets()
{
	if (this->m_variant_setting_parameter_type == _variant_setting_parameter_type_invalid)
		this->m_variant_setting_parameter_type = g_previous_variant_setting_category;
	else
		g_previous_variant_setting_category = this->m_variant_setting_parameter_type;

	this->sily_definition = multiplayer_variant_settings_interface_get_text_value_pair_for_parameter(this->m_variant_setting_parameter_type);

	if(this->sily_definition && this->sily_definition->text_value_pairs.count)
	{
		this->m_list_data = ui_list_data_new(k_variant_parameter_setting_list_name, this->sily_definition->text_value_pairs.count, sizeof(s_variant_parameter_setting_list_item));
		data_make_valid(this->m_list_data);

		if(this->m_list_data->datum_max_elements)
		{
			for(int32 i = 0; i < this->m_list_data->datum_max_elements; ++i)
			{
				s_variant_parameter_setting_list_item* list_item = (s_variant_parameter_setting_list_item*)datum_get(this->m_list_data, datum_new(this->m_list_data));
				list_item->text_value_pair_reference = this->sily_definition->text_value_pairs[i];
			}
		}

		this->linker_type2.link(&this->m_slot);
		c_list_widget::link_item_widgets();
	}
	else
	{
		this->m_list_data = ui_list_data_new(k_variant_parameter_setting_list_empty_name, 1, sizeof(s_variant_parameter_setting_list_item));
		data_make_valid(this->m_list_data);

		s_variant_parameter_setting_list_item* list_item = (s_variant_parameter_setting_list_item*)datum_get(this->m_list_data, datum_new(this->m_list_data));
		list_item->text_value_pair_reference = nullptr;

		this->linker_type2.link(&this->m_slot);
		c_list_widget::link_item_widgets();
	}
}

c_list_item_widget* c_variant_parameter_setting_list::get_list_items()
{
	return this->m_items;
}

int32 c_variant_parameter_setting_list::get_list_items_count()
{
	return k_variant_parameter_setting_list_count;
}

void c_variant_parameter_setting_list::update_list_items(c_list_item_widget* item, int32 skin_index)
{
	if(item->get_last_data_index() != NONE)
	{
		s_variant_parameter_setting_list_item* list_item = (s_variant_parameter_setting_list_item*)datum_get(this->m_list_data, item->get_last_data_index());
		c_text_widget* text_widget = item->try_find_text_widget(0);

		if(this->sily_definition && this->sily_definition->string_list.index != NONE && text_widget && list_item->text_value_pair_reference)
		{
			wchar_t temp_string[512]{};

			text_group_get_unicode_string(this->sily_definition->string_list.index, list_item->text_value_pair_reference->label_string, temp_string);

			text_widget->set_text(temp_string);
		}
	}
}


/* ------------------------------------ */
/* -c_screen_variant_parameter_setting- */
/* ------------------------------------ */

c_screen_variant_parameter_setting::c_screen_variant_parameter_setting(e_user_interface_channel_type ui_channel, e_user_interface_render_window window_index, uint16 user_flags, e_user_interface_screen_id screen_id) :
	c_screen_with_menu(screen_id, ui_channel, window_index, user_flags, &this->m_list),
	m_list(user_flags)
{
}

void c_screen_variant_parameter_setting::post_initialize()
{
	c_screen_with_menu::post_initialize();
}

const void* c_screen_variant_parameter_setting::load_proc() const
{
}

void* c_screen_variant_parameter_setting::load_quick_options(s_screen_parameters* parameters)
{
}

void* c_screen_variant_parameter_setting::load_settings(s_screen_parameters* parameters)
{
}
