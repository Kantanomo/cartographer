#include "stdafx.h"
#include "game_variant.h"

#include "game/game.h"

s_game_variant* get_game_variant(void)
{
	return &game_options_get()->game_variant;
}

void __cdecl game_variant_create_default_new(s_game_variant* variant, e_game_variant_description_index game_variant_type)
{
    INVOKE(0x5B33D, 0x3CF9D, game_variant_create_default_new, variant, game_variant_type);
    return;
}
bool __cdecl game_variant_validate(s_game_variant* variant)
{
    return INVOKE(0x5B720, 0x3D380, game_variant_validate, variant);
}