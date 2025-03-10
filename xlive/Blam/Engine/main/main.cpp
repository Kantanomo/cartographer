#include "stdafx.h"
#include "main.h"

#include "simulation/simulation.h"

#include "H2MOD/Modules/EventHandler/EventHandler.hpp"

/* typedefs */
typedef void(__cdecl* t_main_game_reset_map)();

/* prototypes */
static void __cdecl main_game_reset_map_blue_screen_detection();

/* globals */

/* public code */

void main_apply_patches()
{
	PatchCall(Memory::GetAddress(0x397F6, 0x4130F), main_game_reset_map_blue_screen_detection);
}

bool __cdecl cinematic_sound_sync_complete(void)
{
	return INVOKE(0x39480, 0x40FA1, cinematic_sound_sync_complete);
}

void __cdecl main_loop(void)
{
	INVOKE(0x39E2C, 0xC668, main_loop);
	return;
}

void main_reset_map(void)
{
	*Memory::GetAddress<bool*>(0x48224E, 0x4A70C6) = true;
	return;
}

void main_quit()
{
	*Memory::GetAddress<bool*>(0x48220b, 0x4a7083) = true;
	return;
}

void __cdecl main_loop_pregame(int32 a1, int32 a2)
{
	INVOKE(0x3948C, 0x0, main_loop_pregame, a1, a2);
	return;
}

void __cdecl main_reset_map_immediate()
{
	INVOKE(0x9763, 0x1FA4E, main_reset_map_immediate);
	return;
}

/* private code */

static void __cdecl main_game_reset_map_blue_screen_detection()
{
	s_simulation_globals* sim_globals = simulation_get_globals();
	if (sim_globals->simulation_reset_in_progress)
	{
		EventHandler::BlueScreenEventExecute(EventExecutionType::execute_before);
	}
	main_reset_map_immediate();
	if (sim_globals->simulation_reset_in_progress)
	{
		EventHandler::BlueScreenEventExecute(EventExecutionType::execute_after);
	}
}
