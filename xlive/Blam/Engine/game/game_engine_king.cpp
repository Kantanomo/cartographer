#include "stdafx.h"
#include "game_engine_king.h"

e_game_engine_type c_king_engine::get_type()
{
	return INVOKE_TYPE(0x11110A, 0x0, e_game_engine_type(__thiscall*)(c_game_engine*), this);
}

bool c_king_engine::setup()
{
	return INVOKE_TYPE(0x10FA0C, 0x0, bool(__thiscall*)(c_game_engine*), this);
}

bool c_king_engine::function_4()
{
	return INVOKE_TYPE(0x10FCDC, 0x0, bool(__thiscall*)(c_game_engine*), this);
}

void c_king_engine::send_game_start_event(datum player_index)
{
	INVOKE_TYPE(0x10DFA3, 0x0, void(__thiscall*)(c_game_engine*, datum), this, player_index);
}

void c_king_engine::function_13()
{
	INVOKE_TYPE(0x10DFA2, 0x0, void(__thiscall*)(c_game_engine*), this);
}

void c_king_engine::function_14(datum player_index)
{
	INVOKE_TYPE(0x10FACE, 0x0, void(__thiscall*)(c_game_engine*, datum), this, player_index);
}

void c_king_engine::render_game_engine_elements(uint32 user_index)
{
	INVOKE_TYPE(0x10FB47, 0x0, void(__thiscall*)(c_game_engine*, uint32), this, user_index);
}

void c_king_engine::function_16(datum player_index)
{
	INVOKE_TYPE(0x10FAE8, 0x0, void(__thiscall*)(c_game_engine*, datum), this, player_index);
}

void c_king_engine::function_19()
{
	INVOKE_TYPE(0x10FD21, 0x0, void(__thiscall*)(c_game_engine*), this);
}

void c_king_engine::get_multiplayer_score_string(wchar_t* out_string)
{
	INVOKE_TYPE(0x10E1FB, 0x0, void(__thiscall*)(c_game_engine*, wchar_t*), this, out_string);
}

void c_king_engine::function_31(datum player_index, datum player_index_2, bool a3, int32 a4)
{
	INVOKE_TYPE(0x10E291, 0, void(__thiscall*)(c_game_engine*, datum, datum, bool, int32), this, player_index, player_index_2, a3, a4);
}

void c_king_engine::function_33(datum player_index, void* unk)
{
	INVOKE_TYPE(0x10E77D, 0x0, void(__thiscall*)(c_game_engine*, datum, void*), this, player_index, unk);
}

bool c_king_engine::function_35(int32 unk_index)
{
	return INVOKE_TYPE(0x11124C, 0x0, bool(__thiscall*)(c_game_engine*, int32), this, unk_index);
}

bool c_king_engine::test_variant_engine_flag(datum player_index, e_game_engine_variant_flag_test_type type)
{
	return INVOKE_TYPE(0x10E210, 0x0, bool(__thiscall*)(c_game_engine*, datum, int), this, player_index, type);
}

uint32 c_king_engine::get_game_engine_entity_type()
{
	return INVOKE_TYPE(0x10E328, 0x0, uint32(__thiscall*)(c_game_engine*), this);
}

void c_king_engine::function_41()
{
	INVOKE_TYPE(0x10E32E, 0x0, void(__thiscall*)(c_game_engine*), this);
}

void c_king_engine::set_simulation_baseline_data(int32 unused, void* unk)
{
	INVOKE_TYPE(0x10E32F, 0x0, void(__thiscall*)(c_game_engine*, int32, void*), this, unused, unk);
}

void c_king_engine::build_simulation_update(void* unk, int32 unused, void* unk_2)
{
	INVOKE_TYPE(0x10E360, 0x0, void(__thiscall*)(c_game_engine*, void*, int32, void*), this, unk, unused, unk_2);
}

bool c_king_engine::apply_simulation_update(int16 flags, int32 unused, void* unk)
{
	return INVOKE_TYPE(0x10FEB1, 0x0, bool(__thiscall*)(c_game_engine*, int16, int32, void*), this, flags, unused, unk);
}
