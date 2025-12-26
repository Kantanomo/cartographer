#include "stdafx.h"
#include "game_results.h"

/* prototypes */

s_game_results_globals* game_results_globals_get(void);

/* public code */

void game_results_stop_recording(void)
{
	game_results_globals_get()->recording = false;
	return;
}

void game_results_set_recording_pause(bool pause)
{
	game_results_globals_get()->recording_paused = pause;
	return;
}

bool game_results_get_game_recording(void)
{
	return game_results_globals_get()->recording;
}

bool game_results_get_game_updating(void)
{
	return game_results_globals_get()->updating;
}

void game_results_start_updating(void)
{
	game_results_globals_get()->updating = true;
	return;
}

void game_results_stop_updating(void)
{
	game_results_globals_get()->updating = false;
	return;
}

void __cdecl game_results_update(void)
{
	INVOKE(0x692CC, 0x68CE4, game_results_update);
	return;
}

c_game_results* game_results_get()
{
	return Memory::GetAddress<c_game_results*>(0x4B1C90, 0x4DC3C0);
}

/* private code */

s_game_results_globals* game_results_globals_get(void)
{
	return Memory::GetAddress<s_game_results_globals*>(0x4B1C80, 0x4DC3B0);
}
