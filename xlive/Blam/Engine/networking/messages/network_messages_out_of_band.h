#pragma once
#include "network_message_type_collection.h"

#include "memory/bitstream.h"
#include "networking/transport/transport.h"

/* classes */

class c_network_out_of_band_consumer
{
public:
	virtual void recieve_out_of_band_packet(const transport_address* incoming_address, c_bitstream* packet) = 0;
};

/* prototypes */

void __cdecl network_message_types_register_out_of_band(c_network_message_type_collection* message_collection);
