#include "stdafx.h"
#include "network_life_cycle.h"

#include "life_cycle_manager.h"

#include "network_session_interface.h"

/* public code */

bool __cdecl network_life_cycle_initialize(
	c_network_message_gateway* message_gateway,
	c_network_observer* observer,
	c_network_session_manager* session_manager, 
	c_network_session* squad_session_one,
	c_network_session* squad_session_two)
{
	ASSERT(squad_session_one && squad_session_two);

	return INVOKE(0x1AD494, 0x1A6411, network_life_cycle_initialize, message_gateway, observer, session_manager, squad_session_one, squad_session_two);
}

void __cdecl network_life_cycle_end(void)
{
	INVOKE(0x1AD6D2, 0x0, network_life_cycle_end);

	return;
}

void __cdecl network_life_cycle_leave_squad(
	bool immediate)
{
	INVOKE(0x1AD6AF, 0x0, network_life_cycle_leave_squad, immediate);

	return;
}

e_life_cycle_state network_life_cycle_get_state(void)
{
	c_game_life_cycle_manager* life_cycle_manager = c_game_life_cycle_manager::get();
	e_life_cycle_state state = _life_cycle_state_none;

	if (game_life_cycle_initialized())
	{
		state = life_cycle_manager->get_current_state();
	}

	return state;
}

bool network_life_cycle_in_session(
	c_network_session** session)
{
	bool in_session = false;

	if (game_life_cycle_initialized())
	{
		e_life_cycle_state state = c_game_life_cycle_manager::get()->get_current_state();

		if (state>_life_cycle_state_none && state<=_life_cycle_state_joining)
		{
			in_session = network_life_cycle_in_squad_session(session);
		}
	}

	return in_session;
}

bool user_interface_networking_squad_is_online(void)
{
	return network_session_is_online(network_squad_session_get_session_class());
}

bool __cdecl network_life_cycle_squad_local_peer_is_leader(void)
{
    return INVOKE(0x1AD940, 0x0, network_life_cycle_squad_local_peer_is_leader);
}
