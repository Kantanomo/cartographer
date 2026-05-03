#pragma once
#include "networking/network_game_definitions.h"

/* prototypes */

bool __cdecl network_life_cycle_initialize(
	class c_network_message_gateway* message_gateway,
	class c_network_observer* observer,
	class c_network_session_manager* session_manager,
	class c_network_session* squad_session_one,
	class c_network_session* squad_session_two);
void __cdecl network_life_cycle_end(void);
void __cdecl network_life_cycle_leave_squad(bool immediate);
e_life_cycle_state network_life_cycle_get_state(void);
bool network_life_cycle_in_session(c_network_session** session);
bool user_interface_networking_squad_is_online(void);
bool __cdecl network_life_cycle_squad_local_peer_is_leader(void);
