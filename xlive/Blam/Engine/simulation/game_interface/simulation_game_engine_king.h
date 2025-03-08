#pragma once
#include "simulation_game_engine.h"

enum e_simulation_king_engine_state_data_flags
{
	_king_engine_state_data_flag_hill_id_exists = 5,
	_king_engine_state_data_flag_players_in_hill_exists = 6,


	k_king_engine_state_data_flags_count = 2,
	k_king_engine_state_data_flags_total_count = k_king_engine_state_data_flags_count + k_game_engine_state_data_flag_count,
	k_king_engine_state_data_initial_update_mask = MASK(k_king_engine_state_data_flags_total_count),
	k_king_engine_state_data_flags_mask = FLAG_RANGE(_king_engine_state_data_flag_hill_id_exists, _king_engine_state_data_flag_players_in_hill_exists)
};

struct s_king_engine_state_data : s_game_engine_state_data
{
	uint16 hill_id;
	uint16 players_in_hill;
};
