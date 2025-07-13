#pragma once
#include "user_interface_channel.h"

/* structures */

class c_hardware_error_dialog_channel : public c_user_interface_channel
{
private:
	int32 field3C;
	int32 field40;

public:

	// c_hardware_error_dialog_channel virtual functions

	virtual ~c_hardware_error_dialog_channel();
	virtual void update_channel() override;

};
ASSERT_STRUCT_SIZE(c_hardware_error_dialog_channel, 0x44);
