#pragma once
#include "input_windows.h"

/* classes */

class xinput_device : public input_device
{
protected:
	uint32 dwUserIndex;
	uint32 error_level;
	XINPUT_STATE xinput_state;
public:

	uint32 get_port() const;
	// xinput_device virtual functions

	virtual void XInputOpen(void) override { return; };
	virtual void XInputClose(void) override { return; };
	virtual void XUpdateState(void) override;
	virtual uint32 XGetState(XINPUT_STATE* state) override;
	virtual void XSetState(XINPUT_VIBRATION* state) override;
	virtual void XUpdateImmediate(void) override;
};
ASSERT_STRUCT_SIZE(xinput_device, 0x1C);

/* globals */

extern uint16 g_controller_radial_deadzones[k_number_of_controllers];

/* prototypes */

int16 input_xinput_adjust_thumb_axis_deadzone(int16 thumb_axis, int16 thumb_deadzone);

void input_xinput_adjust_thumb_radial_deadzones(uint32 gamepad_index, s_gamepad_input_button_state* gamepad_state);

bool input_xinput_update_gamepad(uint32 gamepad_index, uint32 duration_ms, struct s_gamepad_input_button_state* gamepad_state);

void xinput_apply_patches(void);
