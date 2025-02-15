#pragma once
#include "interface/multiplayer_variant_settings_interface_definition.h"
#include "interface/user_interface_widget.h"
#include "interface/user_interface_widget_list.h"
#include "interface/user_interface_widget_list_item.h"
#include "interface/user_interface_widget_window.h"

#define k_variant_parameter_setting_list_name "variant parameter setting list"
#define k_variant_parameter_setting_list_empty_name "EMPTY variant parameter setting list"
#define k_variant_parameter_setting_list_count 6

struct s_variant_parameter_setting_list_item
{
	int32 unk;
	s_text_value_pair_reference_new* text_value_pair_reference;
};
ASSERT_STRUCT_SIZE(s_variant_parameter_setting_list_item, 8);

static e_variant_setting_parameter_type g_previous_variant_setting_category;

class c_variant_parameter_setting_list : public c_list_widget
{
private:
	c_list_item_widget m_items[k_variant_parameter_setting_list_count];
	c_slot2<c_variant_parameter_setting_list, s_event_record*, int32> m_slot;
	e_variant_setting_parameter_type m_variant_setting_parameter_type;
	s_text_value_pair_definition* sily_definition;
	int8 field_3E8;

	void handle_item_pressed_event(s_event_record** event, datum* pitem_index);

public:
	c_variant_parameter_setting_list(uint16 user_flags);

	virtual ~c_variant_parameter_setting_list() = default;

	virtual int32 link_item_widgets() override;

	virtual c_list_item_widget* get_list_items() override;

	virtual int32 get_list_items_count() override;

	virtual void update_list_items(c_list_item_widget* item, int32 skin_index) override;
};
ASSERT_STRUCT_SIZE(c_variant_parameter_setting_list, 1004);


class c_screen_variant_parameter_setting : public c_screen_with_menu
{
private:
	c_variant_parameter_setting_list m_list;

public:
	c_screen_variant_parameter_setting(e_user_interface_channel_type ui_channel, e_user_interface_render_window  window_index, uint16 user_flags, e_user_interface_screen_id screen_id);

	virtual ~c_screen_variant_parameter_setting() = default;

	virtual void post_initialize() override;

	const void* load_proc() const override;

	static void* __cdecl load_quick_options(s_screen_parameters* parameters);
	static void* __cdecl load_settings(s_screen_parameters* parameters);
};
ASSERT_STRUCT_SIZE(c_screen_variant_parameter_setting, 3660);