#include "stdafx.h"
#include "network_session_manager.h"


/* constants */


/* typedefs */


/* prototypes */


/* globals */


/* public code */

c_network_session* c_network_session_manager::get_network_session_by_id(const XNKID* target_session_id) const
{
	XNKID session_id;

	c_network_session* session = NULL;
	for (int32 i = 0; i < k_network_maximum_sessions; i++)
	{
		session = m_sessions[i];
		if (session && session->get_transport_keys(&session_id, NULL, NULL, NULL))
		{
			if (csmemcmp(session_id.ab, target_session_id->ab, sizeof(session_id.ab)) == 0)
				break;
		}
	}

	return session;
}


/* private code */