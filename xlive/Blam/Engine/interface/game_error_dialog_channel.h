#pragma once
#include "user_interface_channel.h"

/* classes */

class c_game_error_dialog_channel : public c_user_interface_channel
{

public:

	// c_game_error_dialog_channel virtual functions

	virtual ~c_game_error_dialog_channel();

};
ASSERT_STRUCT_SIZE(c_game_error_dialog_channel, 0x3C);
