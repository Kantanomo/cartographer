#include "stdafx.h"
#include "simulation_game_engine.h"

uint32 c_simulation_game_engine_definition::creation_data_size()
{
	LOG_INFO_GAME("[{}]", __FUNCTION__);
	return 0;
}

int8 c_simulation_game_engine_definition::entity_replication_required_for_view_activation(s_simulation_game_entity* entity)
{
	LOG_INFO_GAME("[{}]", __FUNCTION__);
	return 1;
}

int8 c_simulation_game_engine_definition::get_object_index(s_simulation_game_entity* entity)
{
	LOG_INFO_GAME("[{}]", __FUNCTION__);
	return 0;
}

int32 c_simulation_game_engine_definition::sub_A9004D(int a1, int a2, DWORD a3)
{
	LOG_INFO_GAME("[{}]", __FUNCTION__);
	return 1;
}

int8 c_simulation_game_engine_definition::creation_minimum_required_bits(s_simulation_game_entity* entity, void* a3, int32* minimum_required_bits)
{
	LOG_INFO_GAME("[{}]", __FUNCTION__);
	*minimum_required_bits = 0;
	return (char)a3;
}

void c_simulation_game_engine_definition::write_creation_description_to_string(s_simulation_game_entity* entity, void* tel_data, int32 buffer_size, char* buffer)
{
	LOG_INFO_GAME("[{}]", __FUNCTION__);
	INVOKE_TYPE(0x1F7512, 0, void(__thiscall*)(c_simulation_game_engine_definition*, s_simulation_game_entity*, void*, int32, char*),
		this, entity, tel_data, buffer_size, buffer);
}

void c_simulation_game_engine_definition::entity_creation_encode(uint32 creation_data_size, void* creation_data, void* telemetry_data, c_bitstream* packet)
{
	LOG_INFO_GAME("[{}]", __FUNCTION__);
	return;
}

bool c_simulation_game_engine_definition::entity_creation_decode(uint32 creation_data_size, void* creation_data, c_bitstream* packet)
{
	LOG_INFO_GAME("[{}]", __FUNCTION__);
	return true;
}

uint32 c_simulation_game_engine_definition::build_creation_data(s_simulation_game_entity* entity, int32 creation_data_size, void* out_creation_data)
{
	LOG_INFO_GAME("[{}]", __FUNCTION__);
	return 0;
}

bool c_simulation_game_engine_definition::build_baseline_state_data(int32 creation_data_size, void* creation_data, int32 state_data_size, void* out_state_baseline_data)
{
	LOG_INFO_GAME("[{}]", __FUNCTION__);
	return INVOKE_TYPE(0x1F73CD, 0, bool(__thiscall*)(c_simulation_game_engine_definition*, int32, void*, int32, void*),
		this, creation_data_size, creation_data, state_data_size, out_state_baseline_data);
}

bool c_simulation_game_engine_definition::build_updated_state_data(s_simulation_game_entity* entity, uint32* update_mask, int32 state_data_size, void* state_data)
{
	LOG_INFO_GAME("[{}]", __FUNCTION__);
	return INVOKE_TYPE(0x1F7402, 0, bool(__thiscall*)(c_simulation_game_engine_definition*, s_simulation_game_entity*, uint32*, int32, void*),
		this, entity, update_mask, state_data_size, state_data);
}

uint32 c_simulation_game_engine_definition::rotate_entity_indices(s_simulation_game_entity* entity)
{
	LOG_INFO_GAME("[{}]", __FUNCTION__);
	return INVOKE_TYPE(0x1F7443, 0, uint32(__thiscall*)(c_simulation_game_engine_definition*, s_simulation_game_entity*), this, entity);
}

bool c_simulation_game_engine_definition::create_game_entity(s_simulation_game_entity* entity, int32 creation_data_size, void* creation_data, uint32 mask, int32 state_data_size, void* state_data)
{
	LOG_INFO_GAME("[{}]", __FUNCTION__);
	return INVOKE_TYPE(0x1F755F, 0, bool(__thiscall*)(c_simulation_game_engine_definition*, s_simulation_game_entity*, int32, void*, uint32, int32, void*),
		this, entity, creation_data_size, creation_data, mask, state_data_size, state_data);
}

bool c_simulation_game_engine_definition::update_game_entity(s_simulation_game_entity* entity, uint32 update_flags, int32 state_data_size, void* state_data)
{
	LOG_INFO_GAME("[{}]", __FUNCTION__);
	return INVOKE_TYPE(0x1F7482, 0, bool(__thiscall*)(c_simulation_game_engine_definition*, s_simulation_game_entity*, uint32, int32, void*),
		this, entity, update_flags, state_data_size, state_data);
}

bool c_simulation_game_engine_definition::delete_game_entity(s_simulation_game_entity* entity)
{
	LOG_INFO_GAME("[{}]", __FUNCTION__);
	return INVOKE_TYPE(0x1F75BE, 0, bool(__thiscall*)(c_simulation_game_engine_definition*, s_simulation_game_entity*), this, entity);
}

bool c_simulation_game_engine_definition::promote_game_entity_to_authority(s_simulation_game_entity* entity)
{
	LOG_INFO_GAME("[{}]", __FUNCTION__);
	return INVOKE_TYPE(0x1F74C1, 0, bool(__thiscall*)(c_simulation_game_engine_definition*, s_simulation_game_entity*), this, entity);
}
