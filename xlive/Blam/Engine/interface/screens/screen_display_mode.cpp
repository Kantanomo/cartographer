#include "stdafx.h"
#include "screen_display_mode.h"

void* __cdecl c_screen_display_mode_menu::load(s_screen_parameters* parameters)
{
	return INVOKE(0x2491D3, 0x0, c_screen_display_mode_menu::load, parameters);
}

void* __cdecl c_screen_display_mode_menu::load_mp(s_screen_parameters* parameters)
{
	return INVOKE(0x258C02, 0x0, c_screen_display_mode_menu::load_mp, parameters);
}
