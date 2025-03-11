#include "stdafx.h"
#include "game_engine_headhunter.h"

#include "simulation/game_interface/simulation_game_engine_headhunter.h"
#include "simulation/game_interface/simulation_game_entities.h"

e_game_engine_type c_headhunter_engine::get_type()
{
	return _game_engine_type_headhunter;
}

bool c_headhunter_engine::function_34(datum player_index, void* unk)
{
	return false;
}

uint32 c_headhunter_engine::get_game_engine_entity_type()
{
	return _simulation_entity_type_headhunter_engine_globals;
}

void c_headhunter_engine::set_simulation_baseline_data(int32 unused, void* state_data)
{
	s_headhunter_engine_state_data* game_state_data = (s_headhunter_engine_state_data*)state_data;

	game_state_data->initial_teams = 0;
	game_state_data->valid_teams = 0;
	game_state_data->ever_active_teams = 0;
	game_state_data->current_round = 0;

	memset(game_state_data->team_designator, NONE, sizeof(game_state_data->team_designator));

	game_state_data->bin_id = 1;

	c_headhunter_engine::function_42(state_data);
}

void c_headhunter_engine::build_simulation_update(uint32* update_mask, int32 unused, void* state_data)
{
	s_headhunter_engine_state_data* game_state_data = (s_headhunter_engine_state_data*)state_data;

	uint32 out_written_flags = 0;
	uint32 base_game_engine_state_flags = *update_mask & k_game_engine_state_data_flags_mask;

	if(base_game_engine_state_flags)
		c_headhunter_engine::function_43(base_game_engine_state_flags, &out_written_flags, state_data);

	//if (TEST_BIT(*update_mask, _headhunter_engine_state_flag_bin_id_exists) /*&& headhunter_globals->bin_id != state_data->bin_id*/)
	//{
		game_state_data->bin_id = 2;
		out_written_flags |= FLAG(_headhunter_engine_state_flag_bin_id_exists);
	//}

	*update_mask = out_written_flags;
}

bool c_headhunter_engine::apply_simulation_update(uint32 update_mask, int32 unused, void* state_data)
{
	bool result = true;

	s_headhunter_engine_state_data* game_state_data = (s_headhunter_engine_state_data*)state_data;

	uint32 base_game_engine_state_flags = update_mask & k_game_engine_state_data_flags_mask;

	if (base_game_engine_state_flags)
		result = c_headhunter_engine::function_44(update_mask, state_data);

	if(TEST_BIT(update_mask, _headhunter_engine_state_flag_bin_id_exists))
	{
		LOG_INFO_GAME("[{}] recieved simulation update bin_id: {}", __FUNCTION__, game_state_data->bin_id);
		//headhunter_globals->bin_id = game_state_data->bin_id;
	}

	return result;
}

