#pragma once

/* prototypes */

bool __cdecl network_life_cycle_initialize(
	class c_network_message_gateway* message_gateway,
	class c_network_observer* observer,
	class c_network_session_manager* session_manager,
	class c_network_session* squad_session_one,
	class c_network_session* squad_session_two);
