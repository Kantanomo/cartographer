#pragma once

/* forward declarations */

class c_network_message_gateway;
class c_network_observer;
class c_network_session_manager;
class c_network_session;

/* prototypes */

bool __cdecl network_life_cycle_initialize(c_network_message_gateway* message_gateway, c_network_observer* observer, c_network_session_manager* session_manager, c_network_session* squad_session_one, c_network_session* squad_session_two);
