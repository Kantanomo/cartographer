#include "stdafx.h"
#include "game_variant.h"

#include "game/game.h"
#include "game/game_engine.h"
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

s_game_variant* __cdecl get_default_game_variant_by_name(wchar_t const* name)
{
	ASSERT(Memory::g_memory_is_dedicated_server);

	return INVOKE(0, 0x678E, get_default_game_variant_by_name, name);
}

bool game_variant_is_valid(s_game_variant* variant)
{
	s_game_variant tmp_variant;
	csmemcpy(&tmp_variant, variant, sizeof(s_game_variant));

	return game_engine_variant_cleanup(&tmp_variant);
}

void game_variant_cartographer_settings_default_new(s_game_variant* variant)
{
	variant->cartographer_settings.version = _cartographer_variant_settings_version_one;
	variant->cartographer_settings.flags.set_unsafe(0);
	variant->cartographer_settings.gravity = _game_gravity_modifier_none;
	variant->cartographer_settings.game_speed = _game_speed_modifier_none;
	variant->cartographer_settings.spawn_protection = _player_spawn_protection_timer_one_second;
}

bool game_variant_cartographer_settings_validate(s_game_variant* variant)
{
	if (variant->cartographer_settings.version == _cartographer_variant_settings_version_none)
	{
		game_variant_cartographer_settings_default_new(variant);
	}

	// in the future this functionality needs to be expanded for upgrading between versions and verification.
	return true;
}