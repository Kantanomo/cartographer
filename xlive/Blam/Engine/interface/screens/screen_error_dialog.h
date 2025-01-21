#pragma once
#include "saved_games/saved_game_files.h"

struct s_screen_parameters;

class c_screen_error_dialog_ok
{
public:
	static void* load_for_active_users(s_screen_parameters* parameters);
	static void load_for_disk_result(int16 controllers_mask, e_saved_game_disk_result disk_result);
	static void apply_patches();
};


class c_screen_error_dialog_ok_cancel
{
public:
	static void apply_patches();
};