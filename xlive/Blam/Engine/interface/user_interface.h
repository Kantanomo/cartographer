#pragma once
#include "H2MOD/Modules/CustomMenu/c_screen_widget.h"

struct c_user_interface_channel_vtable
{
	void* loc_7CBE8C;
	void* sub_7CB924;
	void* nullsub_268;
	int(__thiscall* sub_7CB944)(void*);
	void* sub_7CC4E8;
	void* sub_7CC187;
	int(__thiscall* sub_7CBDDB)(void*, void*, void*);
	void* sub_7CBAAA;
	void* sub_7CB9A9;
	void* sub_7CBF1D;
	void* sub_7CC0E0;
};


struct c_user_interface_channel
{
	c_user_interface_channel_vtable* vtable;
	int32 field_4;
	c_screen_widget* m_active_screen;
	c_screen_widget* m_incoming_screen;
	s_screen_load_data m_incoming_screen_parameters;
	int32 field_30;
	int32 field_34;
	int32 field_38;
};


struct __cppobj c_user_interface_channel_with_history : c_user_interface_channel
{
	c_user_interface_widget* m_widget_stack;
	int32 field_40;
	real32 floasdft;
};
