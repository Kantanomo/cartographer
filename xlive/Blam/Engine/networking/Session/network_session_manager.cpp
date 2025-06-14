#include "stdafx.h"
#include "network_session_manager.h"


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
	return this->m_sessions[session_index];
}

c_network_session* c_network_session_manager::get_session(const XNKID* target_session_id) const
{
	XNKID session_id;

	c_network_session* session = NULL;
	for (int32 i = 0; i < k_network_maximum_sessions; i++)
	{
		session = m_sessions[i];
		if (session && session->get_secure_key(&session_id, NULL, NULL, NULL))
		{
			if (csmemcmp(session_id.ab, target_session_id->ab, sizeof(session_id)) == 0)
				break;
		}
	}

	return session;
}


/* private code */