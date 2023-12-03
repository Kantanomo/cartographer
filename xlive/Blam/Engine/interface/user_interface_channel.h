#pragma once
#define k_user_interface_maximum_windows_per_channel 4

enum e_ui_channel
{
	_ui_channel_hardware_error = 0,
	_ui_channel_game_error = 1,
	_ui_channel_virtual_keyboard = 2,
	_ui_channel_gameshell_dialog = 3,
	_ui_channel_gameshell_unk = 4,
	_ui_channel_gameshell_screen = 5,
	_ui_channel_gameshell_background = 6,
	k_number_of_user_interface_channels = 7
};
