#pragma once
#include "devices.h"

enum e_control_type : int16
{
	_control_toggle_switch = 0,
	_control_on_button = 1,
	_control_off_button = 2,
	_control_call_button = 3,
	NUMBER_OF_CONTROL_TYPES,
};

enum e_control_trigger : int16
{
	_control_trigger_player = 0,
	_control_trigger_destruction = 1,
	NUMBER_OF_CONTROL_TRIGGERS
};

struct _control_definition
{
	// Explaination("$$$ CONTROL $$$", "")

	e_control_type type;
	e_control_trigger trigger;
	real32 call_value;
	string_id action_string;

	tag_reference on_effect;
	tag_reference off_effect;
	tag_reference deny_effect;
};

struct control_definition
{
	_object_definition object;
	_device_definition device;
	_control_definition control;
};
ASSERT_STRUCT_SIZE(control_definition, 320);
