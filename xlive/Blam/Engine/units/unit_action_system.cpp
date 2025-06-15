#include "stdafx.h"
#include "unit_action_system.h"

#include "units.h"
#include "game/game.h"
#include "game/game_globals.h"
#include "simulation/game_interface/simulation_game_action.h"
#include "tag_files/global_string_ids.h"


/* private functions */

bool unit_throw_grenade_actors_unlimited_grenades()
{
	return true;
}

void unit_throw_grenade_move_to_hand(datum unit_index)
{
	unit_datum* unit = (unit_datum*)object_get_fast_unsafe(unit_index);

	int8* object_header = ((int8*)unit) + unit->unit.object_header_358.offset;

	if (unit->unit.current_grenade_index == NONE)
	{
		object_header[3] = 3;
		return;
	}

	s_game_globals* game_globals = scenario_get_game_globals();
	s_game_globals_grenade* current_grenade = nullptr;

	if (game_globals->grenades.count)
		current_grenade = game_globals->grenades[unit->unit.current_grenade_index];

	bool unit_can_throw_grenade = true;

	if (unit->unit.actor_datum == NONE || !unit_throw_grenade_actors_unlimited_grenades() && !object_header[13])
	{
		if (unit->unit.grenade_counts[unit->unit.current_grenade_index] <= 0)
		{
			unit_can_throw_grenade = false;
		}
		else
		{
			s_game_variant* variant = get_game_variant();

			if (game_is_multiplayer() && variant)
			{
				if (!variant->cartographer_settings.flags.test(_cartographer_variant_infinite_grenades))
				{
					--unit->unit.grenade_counts[unit->unit.current_grenade_index];
					simulation_action_object_update(unit_index, 0x400000);
				}
			}
			else
			{
				--unit->unit.grenade_counts[unit->unit.current_grenade_index];
				simulation_action_object_update(unit_index, 0x400000);
			}
		}
	}

	if (!current_grenade || !unit_can_throw_grenade)
	{
		object_header[3] = 3;
		return;
	}

	object_placement_data placement_data{};
	object_marker spawn_marker{};
	s_damage_owner damage_owner{};
	

	object_get_markers_by_string_id(unit_index, _string_id_left_hand, &spawn_marker, 1);

	damage_owner.owner_player_index = unit->unit.controlling_player_index;
	damage_owner.owner_team_index = unit->unit.unit_team;
	damage_owner.owner_object_index = unit_index;

	object_placement_data_new(&placement_data, current_grenade->projectile.index, unit_index, &damage_owner);
	placement_data.flags.set(_scenario_object_placement_bit_2, true);

	unit_get_aiming_vector(unit_index, &placement_data.forward);
	normalize3d(perpendicular3d(&placement_data.forward, &placement_data.up));

	placement_data.position = spawn_marker.matrix.position;

	datum new_object_index = object_new(&placement_data);

	if (new_object_index == NONE)
	{
		object_header[3] = 3;
		return;
	}
	else
	{
		object_attach_to_node(unit_index, new_object_index, spawn_marker.node_index);
		*((datum*)object_header + 4) = new_object_index;
		*((int16*)object_header + 10) = unit->unit.current_grenade_index;
		object_header[12] = 2;
	}
}

__declspec(naked) void unit_throw_grenade_move_to_hand_usercall_to_rewritten(void)
{
	__asm
	{
		pushad
		pushfd

		push edi

		call unit_throw_grenade_move_to_hand

		add esp, 4

		popfd
		popad

		retn
	}
}

/* public functions */

void unit_action_system_apply_patches()
{
	PatchCall(Memory::GetAddress(0x166F5E, 0x15CA1E), unit_throw_grenade_move_to_hand_usercall_to_rewritten);
	PatchCall(Memory::GetAddress(0x169B04, 0x15F5C4), unit_throw_grenade_move_to_hand_usercall_to_rewritten);
}
