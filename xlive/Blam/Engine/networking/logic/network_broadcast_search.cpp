#include "stdafx.h"
#include "network_broadcast_search.h"

/* public code */

bool __cdecl network_broadcast_search_initialize(c_network_link* link, c_network_message_gateway* message_gateway)
{
	return INVOKE(0x1E997C, 0x1CB93C, network_broadcast_search_initialize, link, message_gateway);
}
