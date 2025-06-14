#pragma once
#include "networking/Session/network_session_manager.h"

/* structures */

struct s_network_time_globals
{
	bool network_time_locked;
	int8 pad[3];
	uint32 network_time;
	c_network_session_manager* session_manager;
};

/* prototypes */

bool network_session_time_get_id_and_time(int32 session_id, XNKID* id, uint32* time);

uint32 __cdecl network_time_get_exact(void);

void network_session_time_register_session_manager(c_network_session_manager* session_manager);
