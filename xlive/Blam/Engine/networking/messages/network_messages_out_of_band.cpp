#include "stdafx.h"
#include "network_messages_out_of_band.h"

/* public code */

void __cdecl network_message_types_register_out_of_band(c_network_message_type_collection* message_collection)
{
	INVOKE(0x1F1DEA, 0x1D27A3, network_message_types_register_out_of_band, message_collection);
	return;
}
