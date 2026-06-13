#pragma once

/* classes */

class c_panorama_user_history
{
	char gap0[7520];
};
ASSERT_STRUCT_SIZE(c_panorama_user_history, 0x1d60);

c_panorama_user_history* panorama_user_history_get(void);
