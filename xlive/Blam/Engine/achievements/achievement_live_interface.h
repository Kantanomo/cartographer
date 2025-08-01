#pragma once

/* classes */

class c_achievement_live_interface
{
public:
	void start_upload(void);

private:
	int8 gap[188];
};

/* prototypes */

c_achievement_live_interface* achievement_live_interface_get(void);
