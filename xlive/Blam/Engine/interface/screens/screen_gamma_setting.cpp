#include "stdafx.h"
#include "screen_gamma_setting.h"

void* __cdecl c_screen_gamma_menu::load(s_screen_parameters* parameters)
{
	return INVOKE(0x2492E5, 0x0, c_screen_gamma_menu::load, parameters);
}
