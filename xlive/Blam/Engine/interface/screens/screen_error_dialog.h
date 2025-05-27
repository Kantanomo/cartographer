#pragma once

struct s_screen_parameters;

class c_screen_error_dialog_ok
{
public:
	static void* __cdecl load_for_active_users(s_screen_parameters* parameters);
	static void apply_patches();
};


class c_screen_error_dialog_ok_cancel
{
public:
	static void apply_patches();
};