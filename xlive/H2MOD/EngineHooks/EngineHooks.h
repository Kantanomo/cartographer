#pragma once
#include "Blam/Cache/DataTypes/BlamPrimitiveType.h"

namespace EngineHooks
{
	typedef int(__thiscall* game_life_cycle_update)(BYTE* this_);
	typedef void(__cdecl main_game_reset_map)();

	typedef bool(__cdecl* verify_game_version_on_join)(BYTE executable_type, unsigned short executable_version, unsigned short compatible_version);
	typedef bool(__cdecl* verify_executable_type)(BYTE executable_type);
	typedef void(__cdecl* get_game_version)(BYTE* executable_type, DWORD* executable_version, DWORD* compatible_version);

	typedef void(__thiscall* update_player_score_t)(void* thisptr, unsigned short a2, int a3, int a4, int a5, char a6);

	void call_update_player_score(datum playerDatumIndex);
	void ApplyHooks();
}
