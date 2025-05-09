#include "stdafx.h"
#include "network_session_interface.h"

/* public code */

bool __cdecl network_session_interface_initialize(c_network_session_manager* session_manager)
{
	return INVOKE(0x1B07B2, 0x1A6411, network_session_interface_initialize, session_manager);
}
