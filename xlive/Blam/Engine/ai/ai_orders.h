#pragma once
#include "ai_dialogue.h"
#include "actor_moving.h"
#include "ai_scenario_definitions.h"
#include "zone.h"

enum e_order_flags : int32
{
	_order_locked_bit = 0,
	_order_always_active_bit = 1,
	_order_debug_on_bit = 2,
	_order_strict_area_def_bit = 3,
	_order_follow_closest_player_bit = 4,
	_order_follow_squad_bit = 5,
	_order_active_camo_bit = 6,
	_order_suppress_combat_until_engaged_bit = 7,
	_order_inhibit_vehicle_use_bit = 8
};

enum e_force_combat_status : int16
{
	_force_combat_status_none = 0,
	_force_combat_status_asleep = 1,
	_force_combat_status_idle = 2,
	_force_combat_status_alert = 3,
	_force_combat_status_combat = 4
};

enum e_order_trigger_reference : int
{
	order_trigger_reference_flag_not = FLAG(0)
};

enum e_trigger_flags : int
{
	trigger_flag_latch_on_when_triggered = FLAG(0)
};

enum e_order_rule_type : int16
{
	_order_rule_type_a_or_greater_alive = 0,
	_order_rule_type_a_or_fewer_alive = 1,
	_order_rule_type_x_or_greater_strength = 2,
	_order_rule_type_x_or_less_strength = 3,
	_order_rule_type_if_enemy_sighted = 4,
	_order_rule_type_after_a_ticks = 5,
	_order_rule_type_if_alerted_by_squad_a = 6,
	_order_rule_type_script_ref_true = 7,
	_order_rule_type_script_ref_false = 8,
	_order_rule_type_if_player_in_trigger_volume = 9,
	_order_rule_type_if_all_players_in_trigger_volume = 10,
	_order_rule_type_combat_status_a_or_more = 11,
	_order_rule_type_combat_status_a_or_less = 12,
	_order_rule_type_arrived = 13,
	_order_rule_type_in_vehicle = 14,
	_order_rule_type_sighted_player = 15,
	_order_rule_type_a_or_greater_fighting = 16,
	_order_rule_type_a_or_fewer_fighting = 17,
	_order_rule_type_player_within_x_world_units = 18,
	_order_rule_type_player_shot_more_than_x_seconds_ago = 19,
	_order_rule_type_game_safe_to_save = 20
};

enum e_order_completion_condition_flags
{
	order_completion_condition_flag_not = FLAG(0)
};

// max_count: 10
struct order_trigger_reference
{
	e_order_trigger_reference trigger_flags;
	short trigger;                  // Block index: trigger_definition
	short pad;
};
ASSERT_STRUCT_SIZE(order_trigger_reference, 8);

// max count: 1
struct secondary_set_triggers
{
	e_combination_rule combination_rule;
	e_dialogue_type dialogue_type;              // when this ending is triggered, launch a dialogue event of the given type
	tag_block<order_trigger_reference> triggers;
};
ASSERT_STRUCT_SIZE(secondary_set_triggers, 12);

// max count: 12
struct order_ending_definition
{
	short next_order;        // Block Index: orders_block
	e_combination_rule combination_rule;
	float delay_time;
	e_dialogue_type dialogue_type;              // when this ending is triggered, launch a dialogue event of the given type
	short pad;
	tag_block<order_trigger_reference> triggers;
};

// max count: 300
struct orders_definition
{
	/* filt */
	char name[32];
	int16 style;            // Block Index: style_palette_block
	int16 pad;
	e_order_flags flags;
	e_force_combat_status force_combat_status;
	int16 pad1;
	char entry_script[32];
	int16 entry_script_index;
	int16 follow_squad;      // Block Index: squads_block
	real32 follow_radius;

	tag_block<orders_area_reference_definition> primary_area_set;
	tag_block<orders_area_reference_definition> secondary_area_set;
	tag_block<secondary_set_triggers> secondary_set_trigger;
	tag_block<special_movement_definition> special_movement;
	tag_block<order_ending_definition> order_endings;
};
ASSERT_STRUCT_SIZE(orders_definition, 124);

// max count: 5
struct order_completion_condition_definition
{
	e_order_rule_type rule_type;
	int16 squad;            // Block Index: squads_block
	int16 squad_group;      // Block Index: squad_groups_block
	int16 a;
	real32 x;
	int16 trigger_volume;    // Block Index: scenario_trigger_volume_block
	int16 pad;
	char exit_condition_script[32];
	int16 exit_condition_script_index;
	int16 pad1;
	e_order_completion_condition_flags flags;
};
ASSERT_STRUCT_SIZE(order_completion_condition_definition, 56);

// max count: 256
struct trigger_definition
{
	/* filt */
	char name[32];
	e_trigger_flags trigger_flags;
	e_combination_rule combination_rule;
	int16 pad;
	s_tag_block conditions;	// struct: order_completion_condition_definition
};
ASSERT_STRUCT_SIZE(trigger_definition, 48);

/* prototypes */

void __cdecl orders_initialize_for_new_map(void);

bool orders_verify_script_references(orders_definition* orders, bool a2);

bool trigger_verify_script_references(trigger_definition* trigger, bool a2);
