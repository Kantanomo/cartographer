#include "stdafx.h"
#include "game_engine_default.h"

e_game_engine_type c_game_engine_default::get_type()
{
	return _game_engine_type_none;
}

bool c_game_engine_default::setup()
{
	return false;
}

bool c_game_engine_default::function_4()
{
	return true;
}

bool c_game_engine_default::verify_netpoints(uint32 netpoint_index)
{
	return false;
}

int32 c_game_engine_default::get_sudden_death_timer(int32 time_remaining_in_ticks, bool unk, bool unk_2)
{
	return time_remaining_in_ticks;
}

e_game_team c_game_engine_default::get_primary_team_index()
{
	return e_game_team::_game_team_none;
}

bool c_game_engine_default::is_team_enemy(e_game_team a, e_game_team b)
{
	return a != b;
}

void c_game_engine_default::get_multiplayer_score_string(wchar_t* out_string)
{
	INVOKE_TYPE(0x111251, 0, void(__thiscall*)(c_game_engine_default*, wchar_t*), this, out_string);
}

int32 c_game_engine_default::function_32(datum player_index, datum player_index_2, bool a3, int32 a4)
{
	return -1;
}

bool c_game_engine_default::function_34(datum player_index, void* unk)
{
	return true;
}

bool c_game_engine_default::test_variant_engine_flag(datum player_index, e_game_engine_variant_flag_test_type type)
{
	return INVOKE_TYPE(0x1112D1, 0, bool(__thiscall*)(c_game_engine_default*, datum, e_game_engine_variant_flag_test_type), this, player_index, type);
}

void c_game_engine_default::get_player_state_index(datum player_index, bool* always_returned_true)
{
	*always_returned_true = true;
}

bool c_game_engine_default::should_garbage_collect(datum object_index)
{
	return false;
}

uint32 c_game_engine_default::get_game_engine_entity_type()
{
	return 0;
}

void c_game_engine_default::function_42(void* unk)
{
	INVOKE_TYPE(0x1112A7, 0, void(__thiscall*)(c_game_engine_default*, void*), this, unk);
}

void c_game_engine_default::function_43(int8 flags, void* unk, void* unk_2)
{
	INVOKE_TYPE(0x111750, 0, void(__thiscall*)(c_game_engine_default*, int8, void*, void*), this, flags, unk, unk_2);
}

void c_game_engine_default::function_44(int8 flags, void* unk)
{
	INVOKE_TYPE(0x11196A, 0, void(__thiscall*)(c_game_engine_default*, int8, void*), this, flags, unk);
}

void c_game_engine_default::set_player_simulation_baseline_data(int32 unused, int32 unused_2, void* unk)
{
	INVOKE_TYPE(0x11127D, 0, void(__thiscall*)(c_game_engine_default*, int32, int32, void*), this, unused, unused_2, unk);
}

void c_game_engine_default::build_player_simulation_update(int16 abs_player_index, void* unk, int32 unused, void* unk_2)
{
	INVOKE_TYPE(0x11131C, 0, void(__thiscall*)(c_game_engine_default*, int16, void*, int32, void*), this, abs_player_index, unk, unused, unk_2);
}

bool c_game_engine_default::apply_player_simulation_update(int16 abs_player_index, int8 flags, int32 unused, void* unk)
{
	return INVOKE_TYPE(0x111681, 0, bool(__thiscall*)(c_game_engine_default*, int16, int8, int32, void*), this, abs_player_index, flags, unused, unk);
}

uint32 c_game_engine_default::get_territory_name(wchar_t* a1, int32 a2, int32 a3, wchar_t* a4, wchar_t* a5)
{
	return 0;
}
