#pragma once

/* constants */

enum
{
	k_multiplayer_team_count = 8,
};

/* prototypes */

bool __cdecl network_session_interface_initialize(class c_network_session_manager* session_manager);

