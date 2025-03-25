#include "stdafx.h"
#include "game_engine_king.h"

#include "math/random_math.h"
#include "scenario/scenario.h"
#include "scenario/scenario_definitions.h"

void c_king_engine_globals::setup()
{
	scenario* global_scenario = global_scenario_get();

	if(global_scenario->netgame_flags.count > 0)
	{
		uint32 total_hill_count = 0;
		uint16* g_hill_indices = c_king_engine_globals::get_hill_indices();

		c_king_engine_globals::set_hill_count(0);

		for(uint32 index = 0; index < global_scenario->netgame_flags.count; ++index)
		{
			scenario_netpoint* netpoint = global_scenario->netgame_flags[index];

			if(netpoint->type >= netpoint_type_king_hill_0 && netpoint->type <= netpoint_type_king_hill_7)
			{
				uint32 type_index = (uint32)netpoint->type - (uint32)netpoint_type_king_hill_0;

				if(total_hill_count <= 0)
				{
					g_hill_indices[total_hill_count++] = type_index;
				}
				else
				{
					uint32 temp_index = 0;
					while(g_hill_indices[temp_index] != type_index)
					{
						if(++temp_index >= total_hill_count)
						{
							g_hill_indices[total_hill_count++] = type_index;
							break;
						}
					}
				}
			}
		}

		c_king_engine_globals::set_hill_count(total_hill_count);
	}

	this->setup_colors();
}

void c_king_engine_globals::setup_colors()
{
	this->m_color_1 = *global_real_rgb_white;
	this->m_color_2 = *global_real_rgb_white;
	this->m_color_3 = *global_real_rgb_white;
	this->m_color_4 = *global_real_rgb_white;
}

uint32 c_king_engine_globals::get_next_hill_index() const
{
	return INVOKE_TYPE(0x10FE1F, 0xDC3CF, uint32(__cdecl*)(uint32), this->m_hill_id);
}

void c_king_engine_globals::setup_points(uint32 hill_index)
{
	INVOKE_TYPE(0x10FC24, 0xDC1D4, void(__thiscall*)(c_king_engine_globals*, uint32), this, hill_index);
}

uint32 c_king_engine_globals::get_hill_count()
{
	return *Memory::GetAddress<uint32*>(0x4dd0a8, 0x5008e8);
}

void c_king_engine_globals::set_hill_count(uint32 count)
{
	*Memory::GetAddress<uint32*>(0x4dd0a8, 0x5008e8) = count;
}

uint16* c_king_engine_globals::get_hill_indices()
{
	return Memory::GetAddress<uint16*>(0x4dd0b0, 0x5008F0);
}


e_game_engine_type c_king_engine::get_type()
{
	return _game_engine_type_koth;
}

bool c_king_engine::setup()
{
	return INVOKE_TYPE(0x10FA0C, 0xDBFBC, bool(__thiscall*)(c_game_engine*), this);
}

bool c_king_engine::function_4()
{
	return INVOKE_TYPE(0x10FCDC, 0xDC28C, bool(__thiscall*)(c_game_engine*), this);
}

void c_king_engine::send_game_start_event(datum player_index)
{
	INVOKE_TYPE(0x10DFA3, 0xDA553, void(__thiscall*)(c_game_engine*, datum), this, player_index);
}

void c_king_engine::function_13()
{
	return;
}

void c_king_engine::function_14(datum player_index)
{
	INVOKE_TYPE(0x10FACE, 0xDC07E, void(__thiscall*)(c_game_engine*, datum), this, player_index);
}

void c_king_engine::render_game_engine_hud_elements(uint32 user_index)
{
	INVOKE_TYPE(0x10FB47, 0xDC0F7, void(__thiscall*)(c_game_engine*, uint32), this, user_index);
}

void c_king_engine::function_16(datum player_index)
{
	INVOKE_TYPE(0x10FAE8, 0xDC098, void(__thiscall*)(c_game_engine*, datum), this, player_index);
}

void c_king_engine::update()
{
	INVOKE_TYPE(0x10FD21, 0xDC2D1, void(__thiscall*)(c_game_engine*), this);
}

void c_king_engine::get_multiplayer_score_string(wchar_t* out_string)
{
	INVOKE_TYPE(0x10E1FB, 0xDA7AB, void(__thiscall*)(c_game_engine*, wchar_t*), this, out_string);
}

void c_king_engine::function_31(datum player_index, datum player_index_2, bool a3, int32 a4)
{
	INVOKE_TYPE(0x10E291, 0xDA841, void(__thiscall*)(c_game_engine*, datum, datum, bool, int32), this, player_index, player_index_2, a3, a4);
}

void c_king_engine::function_33(datum player_index, void* unk)
{
	INVOKE_TYPE(0x10E77D, 0xDAD2D, void(__thiscall*)(c_game_engine*, datum, void*), this, player_index, unk);
}

bool c_king_engine::function_35(int32 unk_index)
{
	return false;
}

bool c_king_engine::test_variant_engine_flag(datum player_index, e_game_engine_variant_flag_test_type type)
{
	return INVOKE_TYPE(0x10E210, 0xDA7C0, bool(__thiscall*)(c_game_engine*, datum, int), this, player_index, type);
}

e_simulation_entity_type c_king_engine::get_game_engine_entity_type()
{
	return _simulation_entity_type_king_engine_globals;
}

void c_king_engine::function_41()
{
	return;
}

void c_king_engine::set_simulation_baseline_data(int32 unused, void* state_data)
{
	INVOKE_TYPE(0x10E32F, 0xDA8DF, void(__thiscall*)(c_game_engine*, int32, void*), this, unused, state_data);
}

void c_king_engine::build_simulation_update(uint32* unk, int32 unused, void* state_data)
{
	INVOKE_TYPE(0x10E360, 0xDA910, void(__thiscall*)(c_game_engine*, uint32*, int32, void*), this, unk, unused, state_data);
}

bool c_king_engine::apply_simulation_update(uint32 flags, int32 unused, void* state_data)
{
	return INVOKE_TYPE(0x10FEB1, 0xDC461, bool(__thiscall*)(c_game_engine*, uint32, int32, void*), this, flags, unused, state_data);
}
