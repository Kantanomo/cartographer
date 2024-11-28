#include "stdafx.h"
#include "game_engine_slayer.h"

e_game_engine_type c_slayer_engine::get_type()
{
	return INVOKE_TYPE(0x110BCB, 0x0, e_game_engine_type(__thiscall*)(c_game_engine*), this);
}

bool c_slayer_engine::setup()
{
	return INVOKE_TYPE(0x110BD1, 0x0, bool(__thiscall*)(c_game_engine*), this);
}

void c_slayer_engine::player_join(datum player_index)
{
	INVOKE_TYPE(0x110C05, 0x0, void(__thiscall*)(c_game_engine*, datum), this, player_index);
}

void c_slayer_engine::send_game_start_event(datum player_index)
{
	INVOKE_TYPE(0x110C2B, 0x0, void(__thiscall*)(c_game_engine*, datum), this, player_index);
}

void c_slayer_engine::swap_player_indices(uint32 old_index, uint32 new_index)
{
	INVOKE_TYPE(0x110E0A, 0x0, void(__thiscall*)(c_game_engine*, uint32, uint32), this, old_index, new_index);
}

void c_slayer_engine::function_13()
{
	INVOKE_TYPE(0x110C5F, 0x0, void(__thiscall*)(c_game_engine*), this);
}

void c_slayer_engine::render_game_engine_elements(uint32 user_index)
{
	INVOKE_TYPE(0x111129, 0x0, void(__thiscall*)(c_game_engine*, uint32), this, user_index);
}

void c_slayer_engine::function_16(datum player_index)
{
	INVOKE_TYPE(0x1111F8, 0x0, void(__thiscall*)(c_game_engine*, datum), this, player_index);
}

void c_slayer_engine::function_31(datum player_index, datum player_index_2, bool a3, int32 a4)
{
	INVOKE_TYPE(0x110C60, 0x0, void(__thiscall*)(c_game_engine*, datum, datum, bool, int32), this, player_index, player_index_2, a3, a4);
}

bool c_slayer_engine::function_35(int32 unk_index)
{
	return INVOKE_TYPE(0x110D67, 0x0, bool(__thiscall*)(c_game_engine*, int32), this, unk_index);
}

uint32 c_slayer_engine::get_game_engine_entity_type()
{
	return INVOKE_TYPE(0x110D77, 0x0, uint32(__thiscall*)(c_game_engine*), this);
}

void c_slayer_engine::function_41()
{
	INVOKE_TYPE(0x110E09, 0x0, void(__thiscall*)(c_game_engine*), this);
}

void c_slayer_engine::set_simulation_baseline_data(int32 unused, void* unk)
{
	INVOKE_TYPE(0x110D7A, 0x0, void(__thiscall*)(c_game_engine*, int32, void*), this, unused, unk);
}

void c_slayer_engine::build_simulation_update(void* unk, int32 unused, void* unk_2)
{
	INVOKE_TYPE(0x110DA8, 0x0, void(__thiscall*)(c_game_engine*, void*, int32, void*), this, unk, unused, unk_2);
}

bool c_slayer_engine::apply_simulation_update(int16 flags, int32 unused, void* unk)
{
	return INVOKE_TYPE(0x110DE4, 0x0, bool(__thiscall*)(c_game_engine*, int16, int32, void*), this, flags, unused, unk);
}
