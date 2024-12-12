#include "stdafx.h"
#include "game_engine_ctf.h"

e_game_engine_type c_ctf_engine::get_type()
{
	if (get_game_variant()->variant_game_engine_index == _game_engine_type_assault)
		return _game_engine_type_assault;

	return _game_engine_type_ctf;
}

bool c_ctf_engine::setup()
{
	return INVOKE_TYPE(0xD6B11, 0, bool(__thiscall*)(c_game_engine*), this);
}

bool c_ctf_engine::verify_netpoint(uint32 netpoint_index)
{
	return INVOKE_TYPE(0xD4A63, 0, bool(__thiscall*)(c_game_engine*, uint32), this, netpoint_index);
}

void c_ctf_engine::send_game_start_event(datum player_index)
{
	INVOKE_TYPE(0xD426D, 0, void(__thiscall*)(c_game_engine*, datum), this, player_index);
}

void c_ctf_engine::function_13()
{
	INVOKE_TYPE(0xD6C42, 0, void(__thiscall*)(c_game_engine*), this);
}

void c_ctf_engine::function_14(datum player_index)
{
	INVOKE_TYPE(0xD7691, 0, void(__thiscall*)(c_game_engine*, datum), this, player_index);
}

void c_ctf_engine::render_game_engine_hud_elements(uint32 user_index)
{
	INVOKE_TYPE(0xD77A9, 0, void(__thiscall*)(c_game_engine*, uint32), this, user_index);
}

void c_ctf_engine::function_16(datum player_index)
{
	INVOKE_TYPE(0xD6C91, 0, void(__thiscall*)(c_game_engine*, datum), this, player_index);
}

bool c_ctf_engine::player_can_interact_with_weapon(datum player_index, datum weapon_index)
{
	return INVOKE_TYPE(0xD4915, 0, bool(__thiscall*)(c_game_engine*, datum, datum), this, player_index, weapon_index);
}

void c_ctf_engine::handle_player_objective_touch_interaction(datum player_index, datum object_index)
{
	INVOKE_TYPE(0xD7509, 0, void(__thiscall*)(c_game_engine*, datum, datum), this, player_index, object_index);
}

void c_ctf_engine::function_19()
{
	INVOKE_TYPE(0xD7EA1, 0, void(__thiscall*)(c_game_engine*), this);
}

real32 c_ctf_engine::get_player_speed_modifier(datum player_index)
{
	return INVOKE_TYPE(0xD43B1, 0, real32(__thiscall*)(c_game_engine*, datum), this, player_index);
}

uint32 c_ctf_engine::function_21(datum object_index)
{
	return INVOKE_TYPE(0xD440F, 0, uint32(__thiscall*)(c_game_engine*, datum), this, object_index);
}

void c_ctf_engine::update_object_color_change(datum object_index)
{
	INVOKE_TYPE(0xD446A, 0, void(__thiscall*)(c_game_engine*, datum), this, object_index);
}

void c_ctf_engine::handle_object_taken_event(datum weapon_index, datum biped_index)
{
	INVOKE_TYPE(0xD44BB, 0, void(__thiscall*)(c_game_engine*, datum, datum), this, weapon_index, biped_index);
}

void c_ctf_engine::handle_object_dropped_event(datum weapon_index, datum biped_index)
{
	INVOKE_TYPE(0xD4667, 0, void(__thiscall*)(c_game_engine*, datum, datum), this, weapon_index, biped_index);
}

int32 c_ctf_engine::get_sudden_death_timer(int32 time_remaining_in_ticks, bool unk, bool unk_2)
{
	return INVOKE_TYPE(0xD3CD9, 0, int32(__thiscall*)(c_game_engine*, int32, bool, bool), this, time_remaining_in_ticks, unk, unk_2);
}

e_game_team c_ctf_engine::get_primary_team_index()
{
	return INVOKE_TYPE(0xD3D74, 0, e_game_team(__thiscall*)(c_game_engine*), this);
}

void c_ctf_engine::function_31(datum player_index, datum player_index_2, bool a3, int32 a4)
{
	INVOKE_TYPE(0xD3BFE, 0, void(__thiscall*)(c_game_engine*, datum, datum, bool, int32), this, player_index, player_index_2, a3, a4);
}

int32 c_ctf_engine::get_player_killed_event_id(datum player_index, datum player_index_2, bool a3)
{
	return INVOKE_TYPE(0xD3C94, 0, int32(__thiscall*)(c_game_engine*, datum, datum, bool), this, player_index, player_index_2, a3);
}

bool c_ctf_engine::function_34(datum player_index, void* unk)
{
	return INVOKE_TYPE(0xD47E9, 0, bool(__thiscall*)(c_game_engine*, datum, void*), this, player_index, unk);
}

bool c_ctf_engine::function_35(int32 unk_index)
{
	return unk_index == 0;
}

bool c_ctf_engine::test_variant_engine_flag(datum player_index, e_game_engine_variant_flag_test_type type)
{
	return INVOKE_TYPE(0xD49DD, 0, bool(__thiscall*)(c_game_engine*, datum, e_game_engine_variant_flag_test_type), this, player_index, type);
}

void c_ctf_engine::function_37(int32 unk_always_1)
{
	INVOKE_TYPE(0xD7D9E, 0, void(__thiscall*)(c_game_engine*, int32), this, unk_always_1);
}

void c_ctf_engine::get_player_state_index(datum player_index, bool* always_returned_true)
{
	INVOKE_TYPE(0xD3E8D, 0, void(__thiscall*)(c_game_engine*, datum, bool*), this, player_index, always_returned_true);
}

uint32 c_ctf_engine::get_game_engine_entity_type()
{
	// todo: replace with enum value when the simulation entity definitions are done
	return 1;
}

void c_ctf_engine::function_41()
{
	INVOKE_TYPE(0xD7E9C, 0, void(__thiscall*)(c_game_engine*), this);
}

void c_ctf_engine::set_simulation_baseline_data(int32 unused, void* unk)
{
	INVOKE_TYPE(0xD4091, 0, void(__thiscall*)(c_game_engine*, int32, void*), this, unused, unk);
}

void c_ctf_engine::build_simulation_update(void* unk, int32 unused, void* unk_2)
{
	INVOKE_TYPE(0xD575E, 0, void(__thiscall*)(c_game_engine*, void*, int32, void*), this, unk, unused, unk_2);
}

bool c_ctf_engine::apply_simulation_update(int16 flags, int32 unused, void* unk)
{
	return INVOKE_TYPE(0xD5BE6, 0, bool(__thiscall*)(c_game_engine*, int16, int32, void*), this, flags, unused, unk);
}
