#pragma once
#include "user_interface_channel.h"

/* structures */

class c_gameshell_background_channel : public c_user_interface_channel
{
private:
	float gap_3C[10];
	int32 field_64;
	int32 field_68;

public:

	// c_gameshell_background_channel virtual functions
	virtual ~c_gameshell_background_channel();
	virtual void sub_637627() override;
	virtual void dispose_screens() override;
	virtual void update_channel() override;

	// c_gameshell_background_channel addons

	virtual void dispose_screens_wrapper2();
	virtual void sub_637627_wrapper();

};
ASSERT_STRUCT_SIZE(c_gameshell_background_channel, 0x6C);
