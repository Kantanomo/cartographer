#include "stdafx.h"
#include "game_engine_headhunter.h"

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

