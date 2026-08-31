#pragma once

/* structures */

struct s_cartographer_variant_settings_render_globals
{
	bool render;
	real32 progress;
};

/* globals */

extern s_cartographer_variant_settings_render_globals g_cartographer_variant_settings_render_globals;

/* prototypes */

void cartographer_variant_settings_interface_update(real32 dt);