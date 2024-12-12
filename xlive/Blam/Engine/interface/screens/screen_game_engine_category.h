#pragma once
#include "interface/user_interface_widget.h"
#include "interface/user_interface_widget_list.h"
#include "interface/user_interface_widget_list_item.h"
#include "interface/user_interface_widget_window.h"

#define k_game_engine_category_list_name "game engine variant category list"

enum e_screen_game_engine_items : uint16
{
	_screen_game_engine_item_slayer = 0,
	_screen_game_engine_item_king = 1,
	_screen_game_engine_item_oddball = 2,
	_screen_game_engine_item_juggernaut = 3,
	_screen_game_engine_item_ctf = 4,
	_screen_game_engine_item_assault = 5,
	_screen_game_engine_item_territories = 6,
	_screen_game_engine_item_zombies = 7,
	k_screen_game_engine_item_count
};

class c_screen_game_engine_category_list : public c_list_widget
{
public:
	c_list_item_widget m_list_items[k_screen_game_engine_item_count];
	c_slot2<c_screen_game_engine_category_list, s_event_record*, datum> m_slot;
	int8 data[8];
	void handle_item_pressed_event(s_event_record* pevent, datum* pitem_index);

public:
	static void apply_patches();
	c_screen_game_engine_category_list(int16 user_flags);

	virtual ~c_screen_game_engine_category_list() = default;
	int32 setup_children() override;
	virtual c_list_item_widget* get_list_items() override;
	virtual int32 get_list_items_count() override;
	virtual void update_list_items(c_list_item_widget* item, int32 skin_index) override;
};

// size of c_screen_game_engine_category_list if list count 0 + total size of all list item widgets based on item count
ASSERT_STRUCT_SIZE(c_screen_game_engine_category_list, 208 + (sizeof(c_list_item_widget) * k_screen_game_engine_item_count));

class c_screen_game_engine_category : public c_screen_with_menu
{
protected:
	c_screen_game_engine_category_list m_game_engine_list;
	int8 data[5];
public:
	static void apply_patches();
	static void* load(s_screen_parameters* parameters);
	c_screen_game_engine_category(e_user_interface_channel_type ui_channel, e_user_interface_render_window window_index, uint16 user_flags, int8 unk_1, int8 unk_2, int8 unk_3);
	virtual ~c_screen_game_engine_category() = default;
	virtual void* load_proc() override;
};

// size of c_screen_game_engine_category if list count 0 + total size of all list item widgets based on item count
ASSERT_STRUCT_SIZE(c_screen_game_engine_category, 2872 + (sizeof(c_list_item_widget) * k_screen_game_engine_item_count));