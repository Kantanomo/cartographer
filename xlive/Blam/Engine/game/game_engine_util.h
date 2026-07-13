#pragma once

/* public code */

bool game_engine_in_round(void);

void __cdecl game_engine_check_for_round_winner(void);
void __cdecl game_engine_end_round_with_winner(int32 player_datum_or_team_index, bool go_to_next_round);
bool game_engine_has_teams(void);

// ### TODO: function name
bool __cdecl sub_4701B6(datum player_index);
