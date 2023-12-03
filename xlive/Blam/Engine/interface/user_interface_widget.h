#pragma once
#include "Blam/Engine/math/integer_math.h"

enum e_user_interface_widget_type
{
	_widget_type_none = 0,
	_widget_type_screen = 1,
	_widget_type_list_item = 2,
	_widget_type_tab_view = 5,
	_widget_type_text = 6,
	_widget_type_player = 10
};


struct c_user_interface_animation
{
	int32 field_0;
	int32 field_4;
	int16 number_of_key_frames;
	int16 direction;
	int16 field_C;
	int16 field_E;
	int32 field_10;
	int32 field_14;
	int32 field_18;
	int32 field_1C;
	int32 field_20;
	int32 field_24;
	int32 field_28;
	int32 field_2C;
	real32 current_alpha;
};




struct c_user_interface_widget
{
	void* v_table;
	e_user_interface_widget_type type;
	int16 flags;
	int16 wordA;
	int32 field_C;
	c_user_interface_widget* parent;
	int32 field_14;
	int32 field_18;
	int32 field_1C;
	int32 mouse_region;
	int32 field_24;
	real32 field_28;
	real32 field_2C;
	real32 field_30;
	c_user_interface_animation m_animation;
	int16 word68;
	int16 word6A;
	int8 byte6C;
	bool byte6D;
	int8 byte6E;
	bool can_handle_events;
};

struct c_user_interface_widget_vtable
{
	void* c_user_interface_widget__sub_799B8F;
	int(__thiscall* c_user_interface_widget__sub_79B108)(c_user_interface_widget*);
	void(__thiscall* c_user_interface_widget__recurse_next_child_pointlessly)(c_user_interface_widget*);
	int(__thiscall* c_user_interface_widget__update)(c_user_interface_widget*);
	void* nullsub_117;
	void(__thiscall* c_user_interface_widget__get_mouse_region_out)(c_user_interface_widget*, rectangle2d*);
	void(__thiscall* c_user_interface_widget__set_animation_with_children)(c_user_interface_widget*, c_user_interface_animation*);
	void* c_user_interface_widget__return_0;
	void* c_user_interface_widget__sub_79AF34;
	void* (__thiscall* c_user_interface_widget__sub_79B701)(c_user_interface_widget*, c_user_interface_widget*);
	void* c_user_interface_widget__sub_79B77F;
	void* c_user_interface_widget__sub_79B818;
	void* c_user_interface_widget__sub_79AFE4;
	int(__thiscall* c_user_interface_widget__get_parent_screen_channel_type)(c_user_interface_widget*);
	int(__thiscall* c_user_interface_widget__get_parent_screen_render_window)(c_user_interface_widget*);
	int(__thiscall* nullsub_314)(c_user_interface_widget*, int);
	int(__thiscall* nullsub_313)(c_user_interface_widget*, int);
	void* c_user_interface_widget__sub_79A75D;
	bool(__thiscall* c_user_interface_widget__return_0_2)(c_user_interface_widget*);
	void* _purecall;
	void* c_user_interface_widget__sub_79B6E0;
};

