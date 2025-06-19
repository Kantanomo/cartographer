#include "stdafx.h"
#include "network_time.h"

/* prototypes */

static s_network_time_globals* network_time_globals_get(void);

/* public code */

bool network_session_time_get_id_and_time(int32 session_id, XNKID* id, uint32* time)
{
	s_network_time_globals* network_time_globals = network_time_globals_get();

	bool result = false;

	if (network_time_globals->session_manager)
	{
		c_network_session* session = network_time_globals->session_manager->get_session(session_id);
		if (session && session->m_time_exists)
		{
			session->get_secure_key(id, NULL, NULL, NULL);
			*time = session->time_get();
			result = true;
		}
	}

	return result;
}

uint32 __cdecl network_time_get_exact(void)
{
	return INVOKE(0x1B3C4E, 0x1AF217, network_time_get_exact);
}

void network_session_time_register_session_manager(c_network_session_manager* session_manager)
{
	s_network_time_globals* network_time_globals = network_time_globals_get();

	ASSERT(network_time_globals->session_manager == NULL);
	network_time_globals->session_manager = session_manager;
	return;
}

/* private code */

static s_network_time_globals* network_time_globals_get(void)
{
	return Memory::GetAddress<s_network_time_globals*>(0x51ABC8, 0x544A20);
}
