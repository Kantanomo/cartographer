#include "stdafx.h"
#include "scenario_kill_trigger_volumes.h"

#include "scenario.h"
#include "scenario_definitions.h"

int32 get_scenario_volume_count(void) 
{
	return global_scenario_get()->scenario_kill_triggers.count;
}

void __cdecl scenario_kill_trigger_volume_disable(int32 trigger_volume_index)
{
	INVOKE(0xB3AB8, 0xA3CCB, scenario_kill_trigger_volume_disable, trigger_volume_index);
	return;
}

void __cdecl scenario_kill_trigger_volume_enable(int32 trigger_volume_index)
{
	INVOKE(0xB3A64, 0xA3C77, scenario_kill_trigger_volume_enable, trigger_volume_index);
	return;
}
