#include "stdafx.h"
#include "screen_brightness_level.h"

void* __cdecl c_screen_brightness_level_menu::load(s_screen_parameters* parameters)
{
	return INVOKE(0x24925C, 0x0, c_screen_brightness_level_menu::load, parameters);
}
