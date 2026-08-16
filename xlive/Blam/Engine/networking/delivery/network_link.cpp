#include "stdafx.h"
#include "network_link.h"

/* constants */


/* typedefs */


/* prototypes */


/* globals */


/* public code */

bool c_network_link::initialize_link(void)
{
	m_packets_transmitted.initialize(2000);
	m_packets_received.initialize(2000);
	m_upstream_bandwidth.initialize(2000);
	m_downstream_bandwidth.initialize(2000);
	create_endpoints();
	m_initialized = true;
	return m_initialized;
}

void c_network_link::create_endpoints(void)
{
	INVOKE_TYPE(0x1BAB6B, 0x1C998B, void(__thiscall*)(c_network_link*), this);
	return;
}

void c_network_link::attach_out_of_band(c_network_out_of_band_consumer* consumer)
{
	ASSERT(m_initialized);
	m_consumer = consumer;
	return;
}


/* private code */
