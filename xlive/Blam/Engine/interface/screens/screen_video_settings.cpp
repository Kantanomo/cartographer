#include "stdafx.h"
#include "screen_video_settings.h"
#include "screen_display_mode.h"
#include "screen_resolution.h"
#include "screen_brightness_level.h"
#include "screen_gamma_setting.h"
#include "screen_anti_aliasing.h"
#include "screen_lod_setting.h"
#include "screen_safe_area_setting.h"
#include "screen_restore_video_defaults.h"
#include "screen_vsync_setting.h"

#include "interface/user_interface_memory.h"
#include "interface/user_interface_controller.h"
#include "rasterizer/rasterizer_settings.h"
#include "tag_files/global_string_ids.h"

#include "H2MOD/Modules/Shell/Config.h"

/* macro defines */

/* constants */

static const char* k_video_setting_list_name = "video settings list";

/* enums */

enum e_video_settings_list_items : uint16
{
	_item_display_mode,
	_item_resolution,
	_item_vsync,
	_item_brightness_level,
	_item_gamma_setting,
	_item_anti_aliasing,
	_item_lod_setting,
	_item_safe_area,
	_item_restore_defaults,

	k_total_no_of_video_settings_list_items
};


/* globals */

/* prototypes */

/* private code */

/* public code */

c_video_settings_list::c_video_settings_list(uint16 user_flags):
	c_list_widget(user_flags),
	m_slot(this, &c_video_settings_list::handle_item_pressed_event)
{
	//we dont need s_list_item_datum here as no of list items remain same
	m_list_data = ui_list_data_new(k_video_setting_list_name, k_total_no_of_video_settings_list_items, sizeof(datum));
	data_make_valid(m_list_data);

	for (int32 i = 0; i < m_list_data->datum_max_elements; ++i)
	{
		datum_new(m_list_data);
	}

	linker_type2.link(&m_slot);
}

c_video_settings_list::~c_video_settings_list()
{
	rasterizer_settings_write_to_registry();
}

c_list_item_widget* c_video_settings_list::get_list_items()
{
	return m_list_items;
}

int32 c_video_settings_list::get_list_items_count()
{
	return k_no_of_visible_items_for_video_settings;
}

void c_video_settings_list::update_list_items(c_list_item_widget* item, int32 skin_index)
{
	ASSERT(item);

	c_text_widget* primary_text = item->try_find_text_widget(_settings_list_skin_text_header);
	c_text_widget* secondary_text = item->try_find_text_widget(_settings_list_skin_text_value);

	string_id primary_string = _string_id_empty_string;
	string_id secondary_string = _string_id_empty_string;

	s_rasterizer_settings* rasterizer_settings = rasterizer_settings_get();
	e_rasterizer_window_mode display_mode = (e_rasterizer_window_mode)rasterizer_settings->display_mode;

	if (primary_text)
	{
		switch (DATUM_INDEX_TO_ABSOLUTE_INDEX(item->get_last_data_index()))
		{
		case _item_display_mode:
			primary_string = _string_id_display_mode;
			if (display_mode == _rasterizer_window_mode_funky_fullscreen)
			{
				secondary_string = _string_id_invalid;
				secondary_text->set_text(k_borderless_string[get_current_language()]);
			}
			else
			{
				secondary_string = rasterizer_settings_get_display_mode_string(display_mode);
			}
			break;
		case _item_resolution:
			primary_string = _string_id_resolution;
			secondary_string = _string_id_invalid;

			wchar_t resolution_text[32];
			rasterizer_settings_get_display_option_resolution_string(rasterizer_settings->display_option_index, resolution_text, NUMBEROF(resolution_text));
			secondary_text->set_text(resolution_text);
			break;
		case _item_vsync:
			primary_string = _string_id_invalid;
			primary_text->set_text(k_vsync_header_string[get_current_language()]);
			secondary_string = H2Config_use_vsync ? _string_id_on : _string_id_off;
			break;
		case _item_brightness_level:
			primary_string = _string_id_brightness_level;
			secondary_string = rasterizer_settings_get_brightness_level_string(rasterizer_settings->brightness);
			break;
		case _item_gamma_setting:
			primary_string = _string_id_gamma_setting;
			secondary_string = rasterizer_settings_get_gamma_setting_string(rasterizer_settings->gamma);
			break;
		case _item_anti_aliasing:
			primary_string = _string_id_anti_aliasing;
			secondary_string = rasterizer_settings_get_anti_aliasing_string(rasterizer_settings->anti_aliasing);
			break;
		case _item_lod_setting:
			primary_string = _string_id_level_of_detail;
			secondary_string = rasterizer_settings_get_lod_setting_string(rasterizer_settings->level_of_detail);
			break;
		case _item_safe_area:
			primary_string = _string_id_safe_area;
			secondary_string = rasterizer_settings_get_safe_area_string(rasterizer_settings->safe_area);
			break;
		case _item_restore_defaults:
			primary_string = _string_id_restore_video_defaults;
			secondary_string = _string_id_empty_string;
			break;
		default:
			DISPLAY_ASSERT("primary_string is undefined and will be used");
		}

		if (primary_string != _string_id_invalid)
		{
			primary_text->set_text_from_string_id(primary_string);
		}
	}
	else
	{
		if (!secondary_text)
			return;

		DISPLAY_ASSERT("secondary_string is undefined and will be used");
	}

	if (secondary_text)
	{
		if (secondary_string != _string_id_invalid)
		{
			secondary_text->set_text_from_string_id(secondary_string);
		}
	}
}

void c_video_settings_list::handle_item_pressed_event(s_event_record** pevent, datum* pitem_index)
{
	//INVOKE_TYPE(0x24961B, 0x0, void(__thiscall*)(c_video_settings_list*, s_event_record**, datum*), this, pevent, pitem_index);

	s_screen_parameters params;
	params.m_flags = 0;
	params.m_window_index = _window_4;
	params.m_context = 0;
	params.m_user_flags = FLAG((*pevent)->controller);
	params.m_channel_type = _user_interface_channel_type_dialog;
	params.m_screen_state.field_0 = NONE;
	params.m_screen_state.m_last_focused_item_order = NONE;
	params.m_screen_state.m_last_focused_item_index = NONE;
	params.m_load_function = nullptr; // stop warning of using uinitialized var

	switch (DATUM_INDEX_TO_ABSOLUTE_INDEX(*pitem_index))
	{
	case _item_display_mode:
		params.m_load_function = &c_screen_display_mode_menu::load;
		break;
	case _item_resolution:
		params.m_load_function = &c_screen_resolution_menu::load;
		break;
	case _item_vsync:
		params.m_load_function = &c_screen_vsync_menu::load;
		break;
	case _item_brightness_level:
		params.m_load_function = &c_screen_brightness_level_menu::load;
		break;
	case _item_gamma_setting:
		params.m_load_function = &c_screen_gamma_menu::load;
		break;
	case _item_anti_aliasing:
		params.m_load_function = &c_screen_anti_aliasing_menu::load;
		break;
	case _item_lod_setting:
		params.m_load_function = &c_screen_lod_menu::load;
		break;
	case _item_safe_area:
		params.m_load_function = &c_screen_safe_area_menu::load;
		break;
	case _item_restore_defaults:
		params.m_load_function = &c_screen_restore_video_defaults_setting_menu::load;
		break;
	default:
		DISPLAY_ASSERT("unreachable");
	}

	if (params.m_load_function != nullptr)
		params.m_load_function(&params);
}


//
// c_screen_video_settings class starts here
// 


c_screen_video_settings::c_screen_video_settings(e_user_interface_channel_type channel_type, e_user_interface_render_window window_index, uint16 user_flags) :
	c_screen_with_menu(_screen_video_settings, channel_type, window_index, user_flags, &m_video_settings_list),
	m_video_settings_list(user_flags)
{
}

const void* c_screen_video_settings::load_proc() const
{
	return &c_screen_video_settings::load;
}

void* c_screen_video_settings::load(s_screen_parameters* parameters)
{
	//return INVOKE(0x21EDC7, 0x0, c_screen_video_settings::load, parameters);

	c_screen_video_settings* screen;

	void* pool = ui_pool_allocate_space(sizeof(c_screen_video_settings), 0);
	if (pool)
	{
		screen = new (pool) c_screen_video_settings(
			parameters->m_channel_type,
			parameters->m_window_index,
			parameters->m_user_flags);

		screen->m_allocated = true;
		user_interface_register_screen_to_channel(screen, parameters);
	}
	else
	{
		screen = nullptr;
	}

	return screen;
}
