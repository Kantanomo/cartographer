#pragma once
#include "interface/user_interface.h"

class c_screen_brightness_level_menu
{
public:
	static void* __cdecl load(s_screen_parameters* parameters);
	static void* __cdecl load_mp(s_screen_parameters* parameters);
};