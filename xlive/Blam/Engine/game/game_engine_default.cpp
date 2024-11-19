#include "stdafx.h"
#include "game_engine_default.h"

e_game_engine_type c_game_engine::get_type()
{
	return _game_engine_type_none;
}

bool c_game_engine::function_4()
{
	return true;
}

void __stdcall c_game_engine::function_14(datum player_index)
{
	INVOKE_TYPE(0xD7691, 0, void(__stdcall*)(datum), player_index);
}


