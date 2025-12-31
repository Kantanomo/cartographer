#pragma once
#include "items.h"
#include "weapon_definitions.h"

/* enums */

enum e_weapon_flags : uint16
{
	_weapon_create_projectiles_bit,
	_weapon_overheated_bit,
	_weapon_overheated_exit_bit,
	_weapon_overheat_recoil_bit,
	_weapon_power_changing_bit,
	_weapon_turned_on_bit,
	_weapon_multiplayer_tracking_bit,
	_weapon_multiplayer_inventory_flag,
	_weapon_ready_for_use_bit,
	_weapon_game_engine_first_bit,
	_weapon_game_engine_last_bi,
	k_weapon_flags_count,
};

enum e_weapon_state : uint32
{
	_weapon_state_idle,
	_weapon_state_fire_primary,
	_weapon_state_fire_secondary,
	_weapon_state_chamber_primary,
	_weapon_state_chamber_secondary,
	_weapon_state_reload_primary,
	_weapon_state_reload_secondary,
	_weapon_state_charged_primary,
	_weapon_state_charged_secondary,
	_weapon_state_ready,
	_weapon_state_put_away,
	k_weapon_state_count,
};

enum e_weapon_trigger_state : uint8
{
	_weapon_trigger_state_idle,
	_weapon_trigger_state_charging,
	_weapon_trigger_state_charged,
	_weapon_trigger_state_tracking,
	_weapon_trigger_state_releasing,
	_weapon_trigger_state_locking,
	_weapon_trigger_state_locked,
	_weapon_trigger_state_overcharged,
	k_weapon_trigger_state_count,
};

enum e_weapon_trigger_flags : uint16
{
	_weapon_trigger_released_since_last_shot_bit,
	_weapon_trigger_destroyed_bit,
	_weapon_trigger_fired_before_charging_bit,
	_weapon_trigger_did_primary_autofire_action_bit,
	_weapon_trigger_did_secondary_autofire_action_bit,
	_weapon_trigger_spewing_bit,
	_weapon_trigger_charge_from_predicted_trigger,
	_weapon_trigger_held_from_script_bit,
	_weapon_trigger_trying_to_track,
	_weapon_trigger_began_tracking_cycle,
	k_weapon_trigger_flag_count = 0xA,
};

enum e_weapon_magazine_state : uint16
{
	_weapon_magazine_idle,
	_weapon_magazine_reloading_single,
	_weapon_magazine_reloading_continuous_starting,
	_weapon_magazine_reloading_continuous_underway,
	_weapon_magazine_reloading_continuous_ending,
	_weapon_magazine_unchambered,
	_weapon_magazine_chambering,
	_weapon_magazine_busy,
	NUMBER_OF_WEAPON_MAGAZINE_STATES,
};

enum e_weapon_barrel_state : uint8
{
	_weapon_barrel_state_idle,
	_weapon_barrel_state_firing,
	_weapon_barrel_state_locked_recovering,
	_weapon_barrel_state_locked_recovering_empty,
	_weapon_barrel_state_recovering,
	k_barrel_state_count,
};

enum e_weapon_barrel_flags : uint16
{
	_weapon_barrel_fire_bit,
	_weapon_barrel_create_projectiles_bit,
	_weapon_barrel_destroyed_bit,
	_weapon_barrel_blurred_bit,
	_weapon_barrel_fired_before_charging_bit,
	_weapon_barrel_damaged_bit,
	_weapon_barrel_did_empty_click_bit,
	_weapon_barrel_did_firing_effect_this_burst_bit,
	k_weapon_barrel_flag_count,
};


/* structure */

struct weapon_barrel
{
	int8 firing_idle_ticks;
	e_weapon_barrel_state state;
	int16 state_timer;
	c_flags<e_weapon_barrel_flags, uint16, k_weapon_barrel_flag_count> flags;
	uint16 fire_count;
	uint16 firing_effects_used_flags;
	int16 firing_effect_index;
	int16 firing_effect_shots_remaining;
	int16 sequential_non_tracer_rounds;
	real32 rate_of_fire;
	real32 ejection_port_position;
	real32 illumination;
	real32 current_error;
	real32 angle_change_scale;
	real32 bonus_shot_fraction;
	real32 recovery_overflow;
	uint16 bonus_shot_count;
	uint8 staggered_marker_offset;
	uint8 predicted_recovery_timer;
	int32 effect_index;
};
ASSERT_STRUCT_SIZE(weapon_barrel, 52);

struct weapon_trigger
{
	e_weapon_trigger_state state;
	uint8 field_02;
	int16 state_timer;
	c_flags<e_weapon_trigger_flags, uint16, k_weapon_trigger_flag_count> flags;
	int16 pad;
	int32 charging_effect_index;
};
ASSERT_STRUCT_SIZE(weapon_trigger, 12);

struct weapon_magazine
{
	e_weapon_magazine_state state;
	int16 state_timer;
	int16 original_time;
	int16 rounds_inventory;
	int16 rounds_loaded;
	int16 rounds_loaded_delayed;
	int16 reload_timer;
	int16 reload_weapon_disable_duration;
};
ASSERT_STRUCT_SIZE(weapon_magazine, 16);

struct weapon_overlay_animation
{
	int32 graph_index;
	int32 animation_id;
	real32 animation_time;
};
ASSERT_STRUCT_SIZE(weapon_overlay_animation, 12);

struct _weapon_datum
{
	c_flags<e_weapon_flags, uint16, k_weapon_flags_count> flags;
	uint16 control_flags;
	uint8 primary_trigger;
	uint8 last_primary_trigger;
	uint8 last_hill_or_valley;
	int8 primary_trigger_direction;
	int8 primary_trigger_down_ticks;
	uint8 barrel_spin;
	uint8 _pad_0A;
	int8 tracked_model_target_index;
	e_weapon_state state;
	int16 first_person_animation_duration_ticks;
	int16 multiplayer_weapon_identifier;
	real32 heat;
	real32 age;
	real32 overcharged;
	real32 power;
	real32 field_1C;
	int32 tracked_object_index;
	real32 recoil_angular_velocity;
	int16 recoil_recovery_time;
	int16 turn_on_timer;
	int16 alternate_shots_loaded;
	int16 _pad_36;
	weapon_barrel barrels[k_weapon_barrel_count];
	weapon_trigger triggers[k_weapon_trigger_count];
	weapon_magazine magazines[MAXIMUM_NUMBER_OF_MAGAZINES_PER_WEAPON];
	int32 overheated_effect_index;
	int32 game_time_last_fired;
	int32 tracked_object_last_acquisition_time;
	weapon_overlay_animation overlay;
};
ASSERT_STRUCT_SIZE(_weapon_datum, 240);

struct weapon_datum
{
	datum definition_index;
	_object_datum object;
	_item_datum item;
	_weapon_datum weapon;
};
ASSERT_STRUCT_SIZE(weapon_datum, 604);

/* prototypes */

int32 __cdecl weapon_get_rounds_total(datum object_index, int32 magazine_index, bool a3);

void __cdecl weapons_fire_barrels(void);

/* macros */

#define weapon_get(index) ((weapon_datum*)(object_get_and_verify_type((index), _object_mask_weapon)))
#define weapon_try_and_get(index) ((weapon_datum*)(object_try_and_get_and_verify_type((index), _object_mask_weapon)))
