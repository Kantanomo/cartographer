#include "stdafx.h"
#include "network_link.h"

/* constants */


/* typedefs */


/* prototypes */


/* globals */


/* public code */

bool c_network_link::initialize_link(void)
{
	this->m_field_248.initialize(2000);
	this->m_field_320.initialize(2000);
	this->m_field_3F8.initialize(2000);
	this->m_field_4D0.initialize(2000);
	this->create_endpoints();
	this->m_initialized = true;
	return true;
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
