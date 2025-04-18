#include "stdafx.h"
#include "screen_resolution.h"

void* __cdecl c_screen_resolution_menu::load(s_screen_parameters* parameters)
{
	return INVOKE(0x249592, 0x0, c_screen_resolution_menu::load, parameters);
}

void* __cdecl c_screen_resolution_menu::load_mp(s_screen_parameters* parameters)
{
	return INVOKE(0x258B78, 0x0, c_screen_resolution_menu::load_mp, parameters);
}
