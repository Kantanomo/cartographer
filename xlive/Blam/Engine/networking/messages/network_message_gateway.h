#pragma once
#include "network_messages_out_of_band.h"

#include "networking/delivery/network_link.h"

/* classes */

class c_network_message_gateway : public c_network_out_of_band_consumer
{
public:
	bool initialize_gateway(class c_network_link* link, class c_network_message_type_collection* message_types);
	void attach_handler(class c_network_message_handler* handler);
private:
	bool m_initialized;
	class c_network_link* m_link;
	class c_network_message_type_collection* m_message_types;
	class c_network_message_handler* m_handler;
	bool m_outgoing_packet_pending;
	int8 m_outgoing_packet_storage[k_network_link_maximum_game_data_size];
	transport_address m_outgoing_packet_address;
	c_bitstream m_outgoing_packet;
};
ASSERT_STRUCT_SIZE(c_network_message_gateway, 1632);
