#include "stdafx.h"
#include "damage.h"

#include "game/game.h"
#include "units/units.h"

/* public code */

void damage_apply_patches(void)
{
	// Hook on call to prevent guardian glitching
	// Used to disable it in Infection only, became a bigger problem so now we have to disable it globally.....
	PatchCall(Memory::GetAddress(0x147DB8, 0x172D55), object_cause_damage);
	return;
}

void __cdecl object_cause_damage(s_damage_data* damage_data, datum object_index, int16 node_index, int16 region_index, int16 material_index, real_vector3d* object_normal)
{
	// Obtain the actor index so we can determine whether or not to disable damage
	datum actor_index = NONE;
	if (damage_data->owner.owner_object_index != NONE)
	{
		const object_header_datum* header = (object_header_datum*)datum_get(object_header_data_get(), damage_data->owner.owner_object_index);
		// If object is a unit then we can grab the actor index
		if (TEST_BIT(_object_mask_unit, header->type))
		{
			actor_index = object_get_fast_unsafe<unit_datum>(damage_data->owner.owner_object_index)->unit.actor_datum;
		}
	}

	// Disable damage if all are true:
	// 1. In multiplayer
	// 2. Not coming from an actor (actor index is none)
	// 3. Does not have a valid player index or team
	if (game_is_multiplayer() && actor_index == NONE && (damage_data->owner.owner_player_index == NONE || damage_data->owner.owner_team_index == _game_team_none))
	{
		LOG_TRACE_GAME("GUARDIAN GLITCH PREVENTED");
	}
	else
	{
		INVOKE(0x17AD81, 0x1525E1, object_cause_damage, damage_data, object_index, node_index, region_index, material_index, object_normal);
	}

	return;
}
