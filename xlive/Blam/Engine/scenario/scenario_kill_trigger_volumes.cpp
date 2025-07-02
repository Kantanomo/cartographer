#include "stdafx.h"
#include "scenario_kill_trigger_volumes.h"

#include "scenario.h"
#include "scenario_definitions.h"

int32 get_scenario_volume_count() 
{
	return global_scenario_get()->scenario_kill_triggers.count;
}

typedef void(__cdecl* kill_volume_enable_or_disable_t)(int volume_id);
void scenario_kill_trigger_volume_disable(int volume_id) 
{
	kill_volume_enable_or_disable_t p_scenario_kill_trigger_volume_disable = Memory::GetAddress<kill_volume_enable_or_disable_t>(0xB3AB8, 0xA3CCB);
	p_scenario_kill_trigger_volume_disable(volume_id);

}

void scenario_kill_trigger_volume_enable(int volume_id) 
{
	kill_volume_enable_or_disable_t p_scenario_kill_trigger_volume_enable = Memory::GetAddress<kill_volume_enable_or_disable_t>(0xB3A64, 0xA3C77);
	p_scenario_kill_trigger_volume_enable(volume_id);
}

void toggle_kill_trigger_volumes(bool enable_volumes) 
{
	// Dont toggle if we're not the host
	// Apparently kill volumes are client sided?
	// if (!NetworkSession::LocalPeerIsSessionHost()) { return; }

	const int32 volume_count = get_scenario_volume_count();
	const kill_volume_enable_or_disable_t kill_trigger_volume_action = (enable_volumes ? scenario_kill_trigger_volume_enable : scenario_kill_trigger_volume_disable);

	for (int32 i = 0; i < volume_count; i++)
	{
		kill_trigger_volume_action(i);
	}
}