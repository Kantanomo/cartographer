#pragma once
#include "networking/Session/network_session_manager.h"

/* constants */

enum
{
	k_multiplayer_team_count = 8,
};

/* prototypes */

bool __cdecl network_session_interface_initialize(c_network_session_manager* session_manager);

