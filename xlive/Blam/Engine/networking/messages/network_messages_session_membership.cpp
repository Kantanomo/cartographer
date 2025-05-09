#include "stdafx.h"
#include "network_messages_session_membership.h"

/* public code */

void __cdecl network_message_types_register_session_membership(c_network_message_type_collection* message_collection)
{
	INVOKE(0x1F0A55, 0x1D140E, network_message_types_register_session_membership, message_collection);
	return;
}
