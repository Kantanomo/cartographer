#pragma once
#include "networking/network_statistics.h"
#include "networking/messages/network_messages_out_of_band.h"

/* constants */

enum
{
	k_network_link_maximum_game_data_size = 1536,
	k_network_link_maximum_voice_data_size = 512,
	k_network_link_maximum_encoded_packet_size = 4096
};

/* classes */

class c_network_link
{
	bool m_initialized;
	int8 pad[3];
	int32 m_field_4;
	int32 m_field_8;
	int8 gap0[16];
	c_network_out_of_band_consumer* m_consumer;
	int8 gap1[552];
	c_network_time_statistics m_field_248;
	c_network_time_statistics m_field_320;
	c_network_time_statistics m_field_3F8;
	c_network_time_statistics m_field_4D0;

public:
	bool initialize_link(void);
	void create_endpoints(void);
	void attach_out_of_band(c_network_out_of_band_consumer* consumer);
};
ASSERT_STRUCT_SIZE(c_network_link, 1448);
