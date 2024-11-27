#include "stdafx.h"
#include "game_engine_juggernaut.h"

e_game_engine_type c_juggernaut_engine::get_type()
{
	return INVOKE_TYPE(0xD322B, 0x0, e_game_engine_type(__thiscall*)(c_game_engine*), this);
}

bool c_juggernaut_engine::setup()
{
	return INVOKE_TYPE(0xD3231, 0x0, bool(__thiscall*)(c_game_engine*), this);
}

void c_juggernaut_engine::player_join(datum player_index)
{
	INVOKE_TYPE(0xD3872, 0x0, void(__thiscall*)(c_game_engine*, datum), this, player_index);
}

void c_juggernaut_engine::send_game_start_event(datum player_index)
{
	INVOKE_TYPE(0xD3249, 0x0, void(__thiscall*)(c_game_engine*, datum), this, player_index);
}

void c_juggernaut_engine::player_leave(datum player_index)
{
	INVOKE_TYPE(0xD38A2, 0x0, void(__thiscall*)(c_game_engine*, datum), this, player_index);
}

void c_juggernaut_engine::swap_player_indices(uint32 old_index, uint32 new_index)
{
	INVOKE_TYPE(0xD327E, 0x0, void(__thiscall*)(c_game_engine*, uint32, uint32), this, old_index, new_index);
}

void c_juggernaut_engine::function_13()
{
	INVOKE_TYPE(0xD327D, 0x0, void(__thiscall*)(c_game_engine*), this);
}

void c_juggernaut_engine::function_14(datum player_index)
{
	INVOKE_TYPE(0xD32DE, 0x0, void(__thiscall*)(c_game_engine*, datum), this, player_index);
}

void c_juggernaut_engine::render_game_engine_elements(uint32 user_index)
{
	INVOKE_TYPE(0xD358E, 0x0, void(__thiscall*)(c_game_engine*, uint32), this, user_index);
}

void c_juggernaut_engine::function_16(datum player_index)
{
	INVOKE_TYPE(0xD33F9, 0x0, void(__thiscall*)(c_game_engine*, datum), this, player_index);
}

void c_juggernaut_engine::function_19()
{
	INVOKE_TYPE(0xD3A71, 0x0, void(__thiscall*)(c_game_engine*), this);
}

real32 c_juggernaut_engine::get_player_speed_modifier(datum player_index)
{
	return INVOKE_TYPE(0xD3463, 0x0, real32(__thiscall*)(c_game_engine*, datum), this, player_index);
}

bool c_juggernaut_engine::is_team_enemy(e_game_team a, e_game_team b)
{
	return INVOKE_TYPE(0xD3281, 0x0, bool(__thiscall*)(c_game_engine*, e_game_team, e_game_team), this, a, b);
}

void c_juggernaut_engine::function_31(datum player_index, datum player_index_2, bool a3, int32 a4)
{
	INVOKE_TYPE(0xD38D2, 0x0, void(__thiscall*)(c_game_engine*, datum, datum, bool, int32), this, player_index, player_index_2, a3, a4);
}

bool c_juggernaut_engine::function_35(int32 unk_index)
{
	return INVOKE_TYPE(0xD32CE, 0x0, bool(__thiscall*)(c_game_engine*, int32), this, unk_index);
}

bool c_juggernaut_engine::test_variant_engine_flag(datum player_index, e_game_engine_variant_flag_test_type type)
{
	return INVOKE_TYPE(0xD34E0, 0x0, bool(__thiscall*)(c_game_engine*, datum, e_game_engine_variant_flag_test_type), this, player_index, type);
}

void c_juggernaut_engine::get_player_state_index(datum player_index, bool* always_returned_true)
{
	INVOKE_TYPE(0xD3564, 0x0, void(__thiscall*)(c_game_engine*, char, bool*), this, player_index, always_returned_true);
}

uint32 c_juggernaut_engine::get_game_engine_entity_type()
{
	return INVOKE_TYPE(0xD32E1, 0x0, uint32(__thiscall*)(c_game_engine*), this);
}

void c_juggernaut_engine::function_41()
{
	INVOKE_TYPE(0xD32E7, 0x0, void(__thiscall*)(c_game_engine*), this);
}

void c_juggernaut_engine::set_simulation_baseline_data(int32 unused, void* unk)
{
	INVOKE_TYPE(0xD32E8, 0x0, void(__thiscall*)(c_game_engine*, int32, void*), this, unused, unk);
}

void c_juggernaut_engine::build_simulation_update(void* unk, int32 unused, void* unk_2)
{
	INVOKE_TYPE(0xD331A, 0, void(__thiscall*)(c_game_engine*, void*, int32, void*), this, unk, unused, unk_2);
}

bool c_juggernaut_engine::apply_simulation_update(int16 flags, int32 unused, void* unk)
{
	return INVOKE_TYPE(0xD3676, 0, bool(__thiscall*)(c_game_engine*, int16, int32, void*), this, flags, unused, unk);
}