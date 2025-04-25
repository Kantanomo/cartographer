#include "stdafx.h"
#include "screen_multiplayer_video_settings_menu.h"
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
#include "interface/user_interface_globals.h"
#include "main/game_preferences.h"
#include "rasterizer/rasterizer_settings.h"
#include "tag_files/global_string_ids.h"


/* enums */

enum e_mp_video_settings_list_items : uint16
{
	_item_display_mode,
	_item_resolution,
	_item_vsync,
	_item_brightness_level,
	_item_gamma_setting,
	_item_anti_aliasing,
	_item_safe_area,
	_item_restore_defaults,

	k_total_no_of_mp_video_settings_list_items
};

/* constants */

static const char k_mp_video_setting_list_name[] = "mp video settings game list";
static const s_custom_item_text_mapping k_screen_multiplayer_video_settings_menu_items_map[] =
{
	{ STRING_ID_TO_CUSTOM_ITEM_MAPPING(_string_id_display_mode)					, _item_display_mode 		},
	{ STRING_ID_TO_CUSTOM_ITEM_MAPPING(_string_id_resolution)					, _item_resolution 			},
	{ k_vsync_header_string	, _item_vsync, true 															},
	{ STRING_ID_TO_CUSTOM_ITEM_MAPPING(_string_id_brightness_level)				, _item_brightness_level	},
	{ STRING_ID_TO_CUSTOM_ITEM_MAPPING(_string_id_gamma_setting)				, _item_gamma_setting 		},
	{ STRING_ID_TO_CUSTOM_ITEM_MAPPING(_string_id_anti_aliasing)				, _item_anti_aliasing 		},
	{ STRING_ID_TO_CUSTOM_ITEM_MAPPING(_string_id_safe_area)					, _item_safe_area 			},
	{ STRING_ID_TO_CUSTOM_ITEM_MAPPING(_string_id_restore_video_defaults)		, _item_restore_defaults 	},

};

/* globals */

/* prototypes */

/* public code */

c_multiplayer_video_settings_list::c_multiplayer_video_settings_list(uint16 user_flags):
	c_list_widget(user_flags),
	m_slot(this, &c_multiplayer_video_settings_list::handle_item_pressed_event)
{

	m_list_data = ui_list_data_new(k_mp_video_setting_list_name, k_total_no_of_mp_video_settings_list_items, sizeof(s_list_item_datum));
	
	ASSERT(m_list_data);
	data_make_valid(m_list_data);

	// yes this sucks
#define LIST_ITEM_DATUM_GET_NEW() \
		(static_cast<s_list_item_datum*>(datum_get(m_list_data, datum_new(m_list_data))))

	LIST_ITEM_DATUM_GET_NEW()->item_id = _item_display_mode;
	LIST_ITEM_DATUM_GET_NEW()->item_id = _item_resolution;
	LIST_ITEM_DATUM_GET_NEW()->item_id = _item_vsync;
	LIST_ITEM_DATUM_GET_NEW()->item_id = _item_brightness_level;
	LIST_ITEM_DATUM_GET_NEW()->item_id = _item_gamma_setting;
	LIST_ITEM_DATUM_GET_NEW()->item_id = _item_anti_aliasing;
	LIST_ITEM_DATUM_GET_NEW()->item_id = _item_safe_area;
	LIST_ITEM_DATUM_GET_NEW()->item_id = _item_restore_defaults;

#undef LIST_ITEM_DATUM_GET_NEW

	linker_type2.link(&m_slot);
}

c_multiplayer_video_settings_list::~c_multiplayer_video_settings_list()
{
	rasterizer_settings_write_to_registry();
}

c_list_item_widget* c_multiplayer_video_settings_list::get_list_items()
{
	return m_list_items;
}

int32 c_multiplayer_video_settings_list::get_list_items_count()
{
	return k_no_of_visible_items_for_mp_video_settings;
}

void c_multiplayer_video_settings_list::update_list_items(c_list_item_widget* item, int32 skin_index)
{
	ASSERT(item);
	this->update_list_items_from_mapping(item, skin_index, _default_list_skin_text_main, k_screen_multiplayer_video_settings_menu_items_map, k_total_no_of_mp_video_settings_list_items);
	return;
}

void c_multiplayer_video_settings_list::handle_item_pressed_event(s_event_record** pevent, datum* pitem_index)
{
	//INVOKE_TYPE(0x258F3E, 0x0, void(__thiscall*)(c_multiplayer_video_settings_list*, s_event_record**, datum*), this, pevent, pitem_index);

	s_screen_parameters params;
	params.m_flags = 0;
	params.m_window_index = this->get_parent_render_window();
	params.m_context = 0;
	params.m_user_flags = FLAG((*pevent)->controller);
	params.m_channel_type = this->get_parent_channel();
	params.m_screen_state.field_0 = NONE;
	params.m_screen_state.m_last_focused_item_order = NONE;
	params.m_screen_state.m_last_focused_item_index = NONE;
	params.m_load_function = nullptr; // stop warning of using uinitialized var

	if (*pitem_index != NONE)
	{
		s_list_item_datum* item = (s_list_item_datum*)datum_try_and_get(m_list_data, *pitem_index);
		e_mp_video_settings_list_items item_type = (e_mp_video_settings_list_items)item->item_id;

		switch (item_type)
		{
		case _item_display_mode:
			params.m_load_function = &c_screen_display_mode_menu::load_mp;
			break;
		case _item_resolution:
			params.m_load_function = &c_screen_resolution_menu::load_mp;
			break;
		case _item_vsync:
			params.m_load_function = &c_screen_vsync_menu::load;
			break;
		case _item_brightness_level:
			params.m_load_function = &c_screen_brightness_level_menu::load_mp;
			break;
		case _item_gamma_setting:
			params.m_load_function = &c_screen_gamma_menu::load_mp;
			break;
		case _item_anti_aliasing:
			params.m_load_function = &c_screen_anti_aliasing_menu::load_mp;
			break;
		case _item_safe_area:
			params.m_load_function = &c_screen_safe_area_menu::load_mp;
			break;
		case _item_restore_defaults:
			params.m_load_function = &c_screen_restore_video_defaults_setting_menu::load_mp;
			break;
		default:
			DISPLAY_ASSERT("unreachable");
		}
	}

	if (params.m_load_function != nullptr)
	{
		if (user_interface_globals_get_edit_player_profile_index() != NONE)
			user_interface_globals_finish_saving_profile_changes();

		s_saved_game_player_profile profile;
		uint32 profile_index;

		user_interface_controller_get_profile_data(this->get_any_responding_controller(), &profile, &profile_index);
		user_interface_globals_set_edit_player_profile(this->get_any_responding_controller(), profile_index, &profile);

		params.m_load_function(&params);
	}
};


//
// c_screen_multiplayer_video_settings class starts here
// 


c_screen_multiplayer_video_settings::c_screen_multiplayer_video_settings(e_user_interface_channel_type channel_type, e_user_interface_render_window window_index, uint16 user_flags) :
	c_screen_with_menu(_screen_video_settings_mp, channel_type, window_index, user_flags, &m_mp_video_settings_list),
	m_mp_video_settings_list(user_flags)
{
}

const void* c_screen_multiplayer_video_settings::load_proc() const
{
	return &c_screen_multiplayer_video_settings::load;
}

void* c_screen_multiplayer_video_settings::load(s_screen_parameters* parameters)
{
	//return INVOKE(0x24DCD9, 0x0, c_screen_multiplayer_video_settings::load, parameters);

	c_screen_multiplayer_video_settings* screen;

	void* pool = ui_pool_allocate_space(sizeof(c_screen_multiplayer_video_settings), 0);
	if (pool)
	{
		screen = new (pool) c_screen_multiplayer_video_settings(
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

void c_screen_multiplayer_video_settings::apply_instance_patches()
{
	//Replace orignal call with custom one inside c_pause_settings_list::handle_item_pressed_event
	WriteValue(Memory::GetAddress(0x24E248) + 4, c_screen_multiplayer_video_settings::load);
}
