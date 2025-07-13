#pragma once
#include "user_interface.h"

/* forward declarations */

class c_screen_widget;
class c_user_interface_widget;


/* enums */

enum e_focussed_widget_type : int32
{
	_focussed_widget_type_selected = 0x0,
	_focussed_widget_type_hovered = 0x1, //doesnt exist in h2x
	k_focussed_widget_types
};


/* structures */

class c_user_interface_channel
{
private:
	/*c_user_interface_channel::_vftable* vtable;*/
	int32 m_window_index;
	c_screen_widget* m_active_screen;
	c_screen_widget* m_incoming_screen;
	s_screen_parameters m_incoming_screen_parameters;
	c_screen_widget* m_outgoing_screen;
	c_user_interface_widget* m_focussed_widgets[k_focussed_widget_types];

public:

	bool active_or_incoming_screen_exists();
	void transition_out();

	// c_user_interface_channel virtual functions

	virtual ~c_user_interface_channel();
	virtual void dispose_screens_wrapper();
	virtual void sub_637627();
	virtual void dispose_screens();
	virtual void update_channel();
	virtual c_screen_widget* sub_638025(rectangle2d* bounds);
	virtual void register_incoming_screen(c_screen_widget* new_screen, s_screen_parameters* parameters);
	virtual void  retreat_one_step();
	virtual int32  destroy_target_screen(c_screen_widget* a2);
	virtual void construct_parameters_from_active_screen();
	virtual void  load_screen_from_incoming_parameters();
};
ASSERT_STRUCT_SIZE(c_user_interface_channel, 0x3C);