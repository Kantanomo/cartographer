#pragma once

#define k_max_save_game_fs_slot 32768
#define save_game_max_name 128

int32 __cdecl pc_saved_game_create_directory_if_free(char* flat_path, wchar_t* full_display_name, int32 unk_1, int32 unk_2, wchar_t* wide_path, int32 wide_path_size);