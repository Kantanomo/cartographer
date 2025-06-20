#pragma once
#include "network_message_handler.h"
#include "network_message_type_collection.h"
#include "network_messages_out_of_band.h"

#include "networking/delivery/network_link.h"

/* classes */

class c_network_message_gateway : public c_network_out_of_band_consumer
{
public:
	bool initialize_gateway(c_network_link* link, c_network_message_type_collection* message_types);
	void attach_handler(c_network_message_handler* handler);
private:
	bool m_initialized;
	int8 pad0[3];
	c_network_link* m_link;
	c_network_message_type_collection* m_message_types;
	c_network_message_handler* m_handler;
	bool m_field_14;
	int8 pad1[3];
	int8 gap0[1608];
};
ASSERT_STRUCT_SIZE(c_network_message_gateway, 1632);
