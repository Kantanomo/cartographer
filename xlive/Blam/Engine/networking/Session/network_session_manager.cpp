#include "stdafx.h"
#include "network_session_manager.h"

#include "network_session.h"
#include "networking/transport/transport_security.h"

/* constants */


/* typedefs */


/* prototypes */


/* globals */


/* public code */

bool c_network_session_manager::initialize_session_manager(void)
{
	csmemset(m_sessions, 0, sizeof(m_sessions));
	return true;
}

c_network_session* c_network_session_manager::get_session(uint32 session_index) const
{
	ASSERT(VALID_INDEX(session_index, NUMBEROF(m_sessions)));
	return m_sessions[session_index];
}

c_network_session* c_network_session_manager::get_session(const s_transport_secure_identifier* target_id) const
{
	c_network_session* session = NULL;
	for (int32 i = 0; i < k_network_maximum_sessions; i++)
	{
		c_network_session* current_session = m_sessions[i];
		if (current_session)
		{
			s_transport_secure_identifier id;
			if (current_session->get_transport_session_id(&id) && csmemcmp(&id, target_id, sizeof(id)))
			{
				session = current_session;
				break;
			}
		}
	}

	return session;
}


/* private code */