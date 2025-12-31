#include "stdafx.h"
#include "weapons.h"

#include "animations/animation_manager.h"
#include "units/units.h"

/* prototypes */

static void weapon_set_state(datum weapon_index, e_weapon_state state, bool immediate);
static bool weapon_is_dual_wielded(datum weapon_index);

/* public functions */

int32 __cdecl weapon_get_rounds_total(datum object_index, int32 magazine_index, bool a3)
{
	return INVOKE(0x15F313, 0x1435D3, weapon_get_rounds_total, object_index, magazine_index, a3);
}

void __cdecl weapons_fire_barrels(void)
{
	INVOKE(0x160AB7, 0x144D77, weapons_fire_barrels);
	return;
}


/* private functions */

void weapon_set_state(datum weapon_index, e_weapon_state state, bool immediate)
{
	weapon_datum* weapon = weapon_get(weapon_index);

	ASSERT(weapon);
	ASSERT(VALID_INDEX(state, k_weapon_state_count));

	object_wake(weapon_index);

	e_weapon_state current_weapon_state = weapon->weapon.state;

	if (immediate
		|| current_weapon_state == _weapon_state_idle
		|| current_weapon_state > _weapon_state_idle && current_weapon_state <= _weapon_state_fire_secondary && state >= current_weapon_state)
	{
		if (object_has_animation_manager(weapon_index))
		{
			string_id state_string_id = _string_id_invalid;

			switch (state)
			{
				case _weapon_state_idle:
					state_string_id = _string_id_idle;
					break;
				case _weapon_state_fire_primary:
					state_string_id = _string_id_fire_1;
					break;
				case _weapon_state_fire_secondary:
					state_string_id = _string_id_fire_2;
					break;
				case _weapon_state_chamber_primary:
					state_string_id = _string_id_chamber_1;
					break;
				case _weapon_state_chamber_secondary:
					state_string_id = _string_id_chamber_2;
					break;
				case _weapon_state_reload_primary:
				case _weapon_state_reload_secondary:
					state_string_id = _string_id_reload_1;
					break;
				case _weapon_state_charged_primary:
				case _weapon_state_charged_secondary:
					state_string_id = _string_id_chamber_2;
					break;
				case _weapon_state_ready:
					state_string_id = _string_id_ready;
					break;
				case _weapon_state_put_away:
					state_string_id = _string_id_put_away;
					break;
			}

			int32 animation_flags = 130;

			if (state_string_id == _string_id_idle)
				animation_flags = 150;

			string_id animation_class = _string_id_default;

			if (weapon->item.flags.test(_item_in_unit_inventory_bit))
			{
				if (weapon->item.inventory_owner_unit_index != NONE &&
					unit_is_dual_wielding(weapon->item.inventory_owner_unit_index))
				{
					animation_class = _string_id_dual;
				}
			}

			c_animation_manager* animation_manager = (c_animation_manager*)object_header_block_get(weapon_index, &weapon->object.animation_manager_block);

			if (animation_manager->set_goal(
				_string_id_current,
				animation_class,
				_string_id_default,
				state_string_id,
				animation_flags,
				k_animation_playback_default_flags)
				// if attempting to set the animation goal fails weapon is dual wielded just use single wielding animations instead.
				|| (animation_class == _string_id_dual 
				&& animation_manager->set_goal(
					_string_id_current,
					_string_id_default,
					_string_id_default,
					state_string_id,
					animation_flags,
					k_animation_playback_default_flags))
				)
			{
				weapon->weapon.state = state;
			}
		}

		if (weapon->item.flags.test(_item_in_unit_inventory_bit))
		{
			if (weapon->item.inventory_owner_unit_index != NONE)
				unit_handle_weapon_state_change(weapon->item.inventory_owner_unit_index, weapon_index, state);
		}
	}
}

bool weapon_is_dual_wielded(datum weapon_index)
{
	bool result = false;

	weapon_datum* weapon = weapon_get(weapon_index);

	ASSERT(weapon);

	if (weapon->object.parent_object_index != NONE)
	{
		unit_datum* unit = unit_get(weapon->object.parent_object_index);

		if (unit->object.object_identifier.is_of_type(_object_mask_unit))
		{
			if (unit_is_dual_wielding(weapon->object.parent_object_index) && (
				weapon_index == unit_inventory_get_weapon(weapon->object.parent_object_index, unit->unit.weapon_slots[0]) ||
				weapon_index == unit_inventory_get_weapon(weapon->object.parent_object_index, unit->unit.weapon_slots[1]))
				)
			{
				result = true;
			}
		}
	}

	return result;
}

