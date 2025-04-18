#pragma once
#include "interface/user_interface.h"
#include "interface/user_interface_widget_list.h"
#include "interface/user_interface_widget_list_item.h"
#include "interface/user_interface_widget_window.h"
#include "main/game_preferences.h"


/* macro defines */

#define k_no_of_visible_items_for_vsync 2

/* constants */

/* enums */

/* classes */

class c_vsync_edit_list : public c_list_widget
{
protected:
	c_list_item_widget m_list_items[k_no_of_visible_items_for_vsync];
	c_slot2<c_vsync_edit_list, s_event_record*, datum> m_slot;

	void handle_item_pressed_event(s_event_record** pevent, datum* pitem_index);


public:
	c_vsync_edit_list(int16 user_flags);

	// c_vsync_edit_list virtual functions

	virtual ~c_vsync_edit_list() = default;
	virtual c_list_item_widget* get_list_items() override;
	virtual int32 get_list_items_count() override;
	virtual void update_list_items(c_list_item_widget* item, int32 skin_index) override;

};



class c_screen_vsync_menu : public c_screen_with_menu
{
protected:
	c_vsync_edit_list m_vsync_edit_list;

public:
	static void* load(s_screen_parameters* parameters);
	c_screen_vsync_menu(e_user_interface_channel_type channel_type, e_user_interface_render_window window_index, int16 user_flags);

	// c_screen_vsync_menu virtual functions

	virtual ~c_screen_vsync_menu() = default;
	virtual void initialize(s_screen_parameters* parameters) override;
	virtual const void* load_proc() const override;

};

extern const wchar_t* const k_vsync_header_string[k_language_count];