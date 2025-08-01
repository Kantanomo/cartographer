#pragma once
#include "channel_with_history.h"

/* classes */

class c_gameshell_channel : public c_channel_with_history
{

public:

	// c_gameshell_channel virtual functions

	virtual ~c_gameshell_channel();

};
ASSERT_STRUCT_SIZE(c_gameshell_channel, 0x44);
