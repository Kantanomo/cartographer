#include "stdafx.h"
#include "network_message_gateway.h"


/* constants */


/* typedefs */


/* prototypes */


/* globals */


/* public code */

bool c_network_message_gateway::initialize_gateway(c_network_link* link, c_network_message_type_collection* message_types)
{
	ASSERT(link);
	ASSERT(message_types);

	m_link = link;
	m_link->attach_out_of_band(this);
	m_message_types = message_types;
	m_message_types->check_message_types();
	m_handler = NULL;
	m_field_14 = false;
	m_initialized = true;
	return true;
}

void c_network_message_gateway::attach_handler(c_network_message_handler* handler)
{
	m_handler = handler;
	return;
}

/* private code */