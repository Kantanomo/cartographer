#include "stdafx.h"
#include "simulation_game_engine_slayer.h"

e_simulation_entity_type c_simulation_slayer_engine_globals_definition::entity_type()
{
	LOG_INFO_GAME("[{}]", __FUNCTION__);
	return _simulation_entity_type_slayer_engine_globals;
}

const char* c_simulation_slayer_engine_globals_definition::entity_type_name()
{
	LOG_INFO_GAME("[{}]", __FUNCTION__);
	return k_simulation_slayer_engine_entity_name;
}

uint32 c_simulation_slayer_engine_globals_definition::state_data_size()
{
	LOG_INFO_GAME("[{}]", __FUNCTION__);
	return sizeof(s_slayer_engine_state_data);
}

int32 c_simulation_slayer_engine_globals_definition::update_flag_count()
{
	LOG_INFO_GAME("[{}]", __FUNCTION__);
	return 5;
}

uint32 c_simulation_slayer_engine_globals_definition::initial_update_mask()
{
	LOG_INFO_GAME("[{}]", __FUNCTION__);
	return 31;
}

int32 __stdcall test(int32* a1)
{
	return INVOKE(0x1F75E1, 0, test, a1);
}

void c_simulation_slayer_engine_globals_definition::calculate_update_relevance(int a1, int32* buffer_size,
	int32* out_relevance)
{
	LOG_INFO_GAME("[{}]", __FUNCTION__);
	*out_relevance = test(buffer_size);

	//*out_relevance = INVOKE_TYPE(0x2017AD, 0, int32(__thiscall*)(c_simulation_slayer_engine_globals_definition*, int, int32, int32*), this, a1, buffer_size, out_relevance);
}

bool c_simulation_slayer_engine_globals_definition::entity_update_encode(bool a1, uint32 update_mask,
	uint32* update_mask_written, uint32 state_data_size, void* state_data, void* telemetry_data, c_bitstream* packet,
	int32 required_leave_space_bits)
{
	LOG_INFO_GAME("[{}]", __FUNCTION__);
	return INVOKE_TYPE(0x201864, 0, bool(__thiscall*)(c_simulation_slayer_engine_globals_definition*, bool, uint32, uint32*, uint32, void*, void*, c_bitstream*, int32),
		this, a1, update_mask, update_mask_written, state_data_size, state_data, telemetry_data, packet, required_leave_space_bits);
}

bool c_simulation_slayer_engine_globals_definition::entity_update_decode(bool a1, uint32* out_update_mask,
	uint32 state_data_size, void* state_data, c_bitstream* packet)
{
	LOG_INFO_GAME("[{}]", __FUNCTION__);
	return INVOKE_TYPE(0x201885, 0, bool(__thiscall*)(c_simulation_slayer_engine_globals_definition*, bool, uint32*, uint32, void*, c_bitstream*),
		this, a1, out_update_mask, state_data_size, state_data, packet);
}

bool c_simulation_slayer_engine_globals_definition::entity_state_lossy_compare(void* a1, void* a2, int32 a3)
{
	LOG_INFO_GAME("[{}]", __FUNCTION__);
	return true;
}

bool c_simulation_slayer_engine_globals_definition::entity_creation_lossy_compare(void* a1, void* a2, int32 a3)
{
	LOG_INFO_GAME("[{}]", __FUNCTION__);
	return true;
}

void* c_simulation_slayer_engine_globals_definition::build_object_creation_data(datum object_index,
	int32 creation_data_size, void* creation_data)
{
	LOG_INFO_GAME("[{}]", __FUNCTION__);
	return INVOKE_TYPE(0x2017C0, 0, void*(__thiscall*)(c_simulation_slayer_engine_globals_definition*, datum, int32, void*),
		this, object_index, creation_data_size, creation_data);
}


