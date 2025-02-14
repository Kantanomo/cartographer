#include "stdafx.h"
#include "user_interface_shared_globals.h"

s_user_interface_shared_globals* user_interface_shared_globals_get()
{
	return INVOKE(0x20BB89, 0, user_interface_shared_globals_get);
}
