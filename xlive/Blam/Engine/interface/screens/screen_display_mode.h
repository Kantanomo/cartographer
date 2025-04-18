#pragma once
#include "interface/user_interface.h"

class c_screen_display_mode_menu
{
public:
	static void* __cdecl load(s_screen_parameters* parameters);
	static void* __cdecl load_mp(s_screen_parameters* parameters);
};