#pragma once
#include "networking/logic/life_cycle_manager.h"

#include "game/players.h"
#include "game/player_constants.h"

/* classes */

class c_game_life_cycle_handler_joining : c_game_life_cycle_handler
{
public:
	bool field_10;
	int8 field_11;
	bool field_12;
	int32 field_14;
	XNKID joining_xnkid;
	XNKEY joining_xnkey;
	XNADDR joining_xnaddr;
	int32 field_54;
	int32 joining_user_count;
	s_player_identifier player_identifiers[k_number_of_users];
	wchar_t player_names[k_number_of_users][XUSER_NAME_SIZE];
	int8 gapFC[128];
	int32 field_17C;
	bool field_180;
	int8 field_181;
	int8 gap_182[2];
	int32 field_184;
	int32 field_188;
	int32 field_18C;
	bool field_190;
	int8 field_191;
	bool field_192;
	int8 gap_193[5];
	int32 field_198;
	int8 gap_19C[3804];
	int32 field_1078;
	int8 gap_107C[4];
	int32 join_attempt_result_code;
	static void __cdecl check_joining_capability();
};
ASSERT_STRUCT_SIZE(c_game_life_cycle_handler_joining, 0x1084);
