#pragma once

/* constants */

#define MAXIMUM_PLAYER_WINDOWS 4
#define MAXIMUM_RENDERED_WINDOWS 9

/* globals */

extern bool g_debug_render_horizontal_splitscreen;
extern bool g_debug_force_all_player_views_to_default;

/* prototypes */

void main_render_apply_patches(void);

void __cdecl main_render(void);

void __cdecl main_render_previous_backbuffer(int32 a1, int32 a2);
