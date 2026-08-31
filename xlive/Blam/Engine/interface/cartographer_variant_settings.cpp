#include "stdafx.h"
#include "cartographer_variant_settings.h"

#include "game/game.h"
#include "game/game_time.h"
#include "input/input_windows.h"
#include "networking/network_event.h"

/* constants */

constexpr real32 k_variant_ui_fade_in_seconds = 0.5f;
constexpr real32 k_variant_ui_fade_out_seconds = 0.25f;

/* globals */

s_cartographer_variant_settings_render_globals g_cartographer_variant_settings_render_globals;

/* public code */

void cartographer_variant_settings_interface_update(real32 dt)
{
	if (game_is_multiplayer())
	{
		if (input_key_msec_down(_key_f3) > 0)
		{
			g_cartographer_variant_settings_render_globals.progress += dt / k_variant_ui_fade_in_seconds;
		}
		else
		{
			g_cartographer_variant_settings_render_globals.progress += -(dt / k_variant_ui_fade_out_seconds);
		}

		g_cartographer_variant_settings_render_globals.progress = PIN(g_cartographer_variant_settings_render_globals.progress, 0.f, 1.f);

		g_cartographer_variant_settings_render_globals.render = g_cartographer_variant_settings_render_globals.progress > 0.0f;
	}
	else
	{
		g_cartographer_variant_settings_render_globals.progress = 0.0f;
		g_cartographer_variant_settings_render_globals.render = false;
	}
}