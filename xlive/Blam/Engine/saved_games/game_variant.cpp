#include "stdafx.h"
#include "game_variant.h"

#include "game/game.h"
#include "game/game_options.h"

/* public code */

s_game_variant* get_game_variant(void)
{
	return &game_options_get()->game_variant;
}

void __cdecl game_variant_build_default(s_game_variant* variant, e_game_variant_description_index game_variant_type)
{
	INVOKE(0x5B33D, 0x3CF9D, game_variant_build_default, variant, game_variant_type);
	return;
}

s_game_variant* __cdecl get_default_game_variant_by_name(wchar_t* name)
{
	// Server only function?
	if (Memory::g_memory_is_dedicated_server)
	{
		return INVOKE(0, 0x678E, get_default_game_variant_by_name, name);
	}
	else
		return nullptr;
}