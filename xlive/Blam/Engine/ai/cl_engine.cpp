#include "stdafx.h"
#include "cl_engine.h"

#include "memory/data.h"
#include "scenario/scenario.h"
#include "scenario/scenario_definitions.h"

/* public code */

void __cdecl cs_initialize_for_new_map(void)
{
	INVOKE(0x34D686, 0x2F83F6, cs_initialize_for_new_map);
	return;
}

data_array* command_script_data_get(void)
{
	return *Memory::GetAddress<data_array**>(0xA9DAC4, 0x9A9144);
}

cs_script_data* cs_scenario_get_script_data(struct scenario* scenario)
{
	return TAG_BLOCK_GET_ELEMENT(&scenario->cs_scripting_data, 0, cs_script_data);
}

cs_point_set* cs_get_point_set(int32 index)
{
	const cs_script_data* script_data = cs_scenario_get_script_data(global_scenario_get());
	return TAG_BLOCK_GET_ELEMENT(&script_data->point_sets, index, cs_point_set);
}
