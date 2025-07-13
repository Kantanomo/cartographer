#pragma once


/* structures */

class c_user_interface_main_menu_music
{
private:
	int32 old_state;
	int32 new_state;
	int32 music_playing;
	int32 state_change_start_time_ms;
	uint32 m_looping_sound_index;
	int32 music_end_time_stamp;

public:
	void update();
	void update_game_shell_music_state();
};
ASSERT_STRUCT_SIZE(c_user_interface_main_menu_music, 0x18);