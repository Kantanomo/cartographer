#include "stdafx.h"
#include "game_engine_oddball.h"

e_game_engine_type c_oddball_engine::get_type()
{
	return INVOKE_TYPE(0x10FF2A, 0x0, e_game_engine_type(__thiscall*)(c_game_engine*), this);
}

bool c_oddball_engine::setup()
{
	return INVOKE_TYPE(0x1102FC, 0x0, bool(__thiscall*)(c_game_engine*), this);
}

void c_oddball_engine::send_game_start_event(datum player_index)
{
	INVOKE_TYPE(0x10FF31, 0x0, void(__thiscall*)(c_game_engine*, datum), this, player_index);
}

void c_oddball_engine::function_13()
{
	INVOKE_TYPE(0x10FF30, 0x0, void(__thiscall*)(c_game_engine*), this);
}

void c_oddball_engine::render_game_engine_hud_elements(uint32 user_index)
{
	INVOKE_TYPE(0x110579, 0x0, void(__thiscall*)(c_game_engine*, uint32), this, user_index);
}

void c_oddball_engine::function_16(datum player_index)
{
	INVOKE_TYPE(0x110781, 0x0, void(__thiscall*)(c_game_engine*, datum), this, player_index);
}

void c_oddball_engine::function_19()
{
	INVOKE_TYPE(0x110B6C, 0x0, void(__thiscall*)(c_game_engine*), this);
}

real32 c_oddball_engine::get_player_speed_modifier(datum player_index)
{
	return INVOKE_TYPE(0x1103EB, 0x0, real32(__thiscall*)(c_game_engine*, datum), this, player_index);
}

void c_oddball_engine::update_object_color_change(datum object_index)
{
	INVOKE_TYPE(0x10FF65, 0x0, void(__thiscall*)(c_game_engine*, datum), this, object_index);
}

void c_oddball_engine::handle_object_taken_event(datum weapon_index, datum biped_index)
{
	INVOKE_TYPE(0x110460, 0x0, void(__thiscall*)(c_game_engine*, datum, datum), this, weapon_index, biped_index);
}

void c_oddball_engine::handle_object_dropped_event(datum weapon_index, datum biped_index)
{
	INVOKE_TYPE(0x1104F3, 0, void(__thiscall*)(c_game_engine*, datum, datum), this, weapon_index, biped_index);
}

void c_oddball_engine::get_multiplayer_score_string(wchar_t* out_string)
{
	INVOKE_TYPE(0x10FFF0, 0x0, void(__thiscall*)(c_game_engine*, wchar_t*), this, out_string);
}

void c_oddball_engine::function_31(datum player_index, datum player_index_2, bool a3, int32 a4)
{
	INVOKE_TYPE(0x1107E1, 0, void(__thiscall*)(c_game_engine*, datum, datum, bool, int32), this, player_index, player_index_2, a3, a4);
}

void c_oddball_engine::function_33(datum player_index, void* unk)
{
	INVOKE_TYPE(0x110005, 0x0, void(__thiscall*)(c_game_engine*, datum, void*), this, player_index, unk);
}

bool c_oddball_engine::function_35(int32 unk_index)
{
	return INVOKE_TYPE(0x10FFEB, 0x0, bool(__thiscall*)(c_game_engine*, int32), this, unk_index);
}

bool c_oddball_engine::test_variant_engine_flag(datum player_index, e_game_engine_variant_flag_test_type type)
{
	return INVOKE_TYPE(0x110856, 0x0, bool(__thiscall*)(c_game_engine*, datum, int), this, player_index, type);
}

void c_oddball_engine::get_player_state_index(datum player_index, bool* always_returned_true)
{
	INVOKE_TYPE(0x1108DC, 0x0, void(__thiscall*)(c_game_engine*, datum, bool*), this, player_index, always_returned_true);
}

uint32 c_oddball_engine::get_game_engine_entity_type()
{
	return INVOKE_TYPE(0x110076, 0x0, uint32(__thiscall*)(c_game_engine*), this);
}

void c_oddball_engine::function_41()
{
	INVOKE_TYPE(0x11010B, 0x0, void(__thiscall*)(c_game_engine*), this);
}

void c_oddball_engine::set_simulation_baseline_data(int32 unused, void* unk)
{
	INVOKE_TYPE(0x11007C, 0x0, void(__thiscall*)(c_game_engine*, int32, void*), this, unused, unk);
}

void c_oddball_engine::build_simulation_update(void* unk, int32 unused, void* unk_2)
{
	INVOKE_TYPE(0x1100AA, 0x0, void(__thiscall*)(c_game_engine*, void*, int32, void*), this, unk, unused, unk_2);
}

bool c_oddball_engine::apply_simulation_update(int16 flags, int32 unused, void* unk)
{
	return INVOKE_TYPE(0x1100E6, 0x0, bool(__thiscall*)(c_game_engine*, int16, int32, void*), this, flags, unused, unk);
}