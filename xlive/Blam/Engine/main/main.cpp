#include "stdafx.h"
#include "main.h"

#include "shell/shell.h"
#include "simulation/simulation.h"

#include "H2MOD/Modules/EventHandler/EventHandler.hpp"
#include "H2MOD/Modules/MapManager/MapManager.h"
#include "XLive/xnet/IpManagement/XnIp.h"

/* typedefs */
typedef void(__cdecl* t_main_game_reset_map)();

/* prototypes */
static void __cdecl main_game_reset_map_blue_screen_detection();

/* globals */

/* public code */

void main_apply_patches(void)
{
	PatchCall(Memory::GetAddress(0x397F6, 0x4130F), main_game_reset_map_blue_screen_detection);
	PatchCall(Memory::GetAddress(0x39E64, 0xC684), main_loop_body);	// main_loop
	return;
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

void main_quit(void)
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

void __cdecl main_loop_body(void)
{
	EventHandler::GameLoopEventExecute(EventExecutionType::execute_before);
	if (!shell_is_dedicated_server())
	{
		mapManager->MapDownloadUpdateTick();
		gXnIpMgr.GetLocalUserXn()->m_pckStats.PckDataSampleUpdate();	// update local user network stats
	}
	
	INVOKE(0x399CC, 0xBFDE, main_loop_body);

	EventHandler::GameLoopEventExecute(EventExecutionType::execute_after);
	return;
}