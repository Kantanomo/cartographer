#include "stdafx.h"
#include "game_engine_util.h"

#include "game/game.h"
#include "game/game_engine.h"

/* public code */

bool game_engine_in_round(void)
{
	return current_game_engine() != NULL
		&& game_engine_globals_get()->field_6C == 1
		&& (game_is_predicted() || game_engine_globals_get()->field_C44 == 1);
}

void __cdecl game_engine_check_for_round_winner(void)
{
	INVOKE(0x70F49, 0x6FA4A, game_engine_check_for_round_winner);
	return;
}

void __cdecl game_engine_end_round_with_winner(
	int32 player_datum_or_team_index,
	bool go_to_next_round)
{
	INVOKE(0x70A6F, 0x6F570, game_engine_end_round_with_winner, player_datum_or_team_index, go_to_next_round);
	return;
}

bool game_engine_has_teams(void)
{
	bool result = false;

	if (current_game_engine())
	{
		result = current_game_variant()->game_engine_flags.test(_game_engine_teams_bit);
	}

	return result;
}

bool __cdecl sub_4701B6(datum player_index)
{
	return INVOKE(0x701B6, 0x0, sub_4701B6, player_index);
}
