#pragma once
#include "channel_with_history.h"

/* structures */

class c_dialog_channel : public c_channel_with_history
{

public:

	// c_dialog_channel virtual functions

	virtual ~c_dialog_channel();

};
ASSERT_STRUCT_SIZE(c_dialog_channel, 0x44);
