#pragma once

#include "game_engine_ctf.h"
#include "game_engine_headhunter.h"
#include "game/game_allegiance.h"
#include "game_statborg.h"
#include "players.h"

#include "main/game_preferences.h"
#include "math/color_math.h"
#include "tag_files/string_id.h"
#include "tag_files/tag_block.h"
#include "tag_files/tag_reference.h"

/* constants */

enum
{
	k_maximum_game_engine_event_responses_per_type = 128
};

/* enums */

enum e_network_game_simulation_protocol
{
	_network_game_simulation_protocol_offline = 0,
	_network_game_simulation_protocol_synchronous = 1,
	_network_game_simulation_protocol_distributed = 2,
	k_network_game_simulation_protocol_count = 3,
};

enum e_game_engine_timer_type
{

	k_game_engine_timer_count = 3
};

enum e_game_engine_state
{

	k_game_engine_state_count = 4
};

enum e_valid_multiplayer_games : short
{
	valid_multiplayer_game_capture_the_flag = FLAG(0),
	valid_multiplayer_game_slayer = FLAG(1),
	valid_multiplayer_game_oddball = FLAG(2),
	valid_multiplayer_game_king_of_the_hill = FLAG(3),
	valid_multiplayer_game_juggernaut = FLAG(4),
	valid_multiplayer_game_territories = FLAG(5),
	valid_multiplayer_game_assault = FLAG(6),
};

enum e_relevant_multiplayer_games : int
{
	relevant_multiplayer_game_capture_the_flag = FLAG(0),
	relevant_multiplayer_game_slayer = FLAG(1),
	relevant_multiplayer_game_oddball = FLAG(2),
	relevant_multiplayer_game_king_of_the_hill = FLAG(3),
	relevant_multiplayer_game_juggernaut = FLAG(4),
	relevant_multiplayer_game_territories = FLAG(5),
	relevant_multiplayer_game_assault = FLAG(6)
};

enum e_multiplayer_event_response_flags : uint16
{
	_multiplayer_event_response_quantity_message = 0,
	k_multiplayer_event_response_flags_count
};

enum e_multiplayer_event_response_event : uint16
{
	// general events
	_multiplayer_event_response_general_kill = 0,
	_multiplayer_event_response_general_suicide = 1,
	_multiplayer_event_response_general_kill_teammate = 2,
	_multiplayer_event_response_general_victory = 3,
	_multiplayer_event_response_general_team_victory = 4,
	_multiplayer_event_response_general_unused1 = 5,
	_multiplayer_event_response_general_unused2 = 6,
	_multiplayer_event_response_general_one_minute_to_win = 7,
	_multiplayer_event_response_general_team_one_minute_to_win = 8,
	_multiplayer_event_response_general_thirty_seconds_to_win = 9,
	_multiplayer_event_response_general_team_thirty_seconds_to_win = 10,
	_multiplayer_event_response_general_player_quit = 11,
	_multiplayer_event_response_general_player_joined = 12,
	_multiplayer_event_response_general_killed_by_unknown = 13,
	_multiplayer_event_response_general_thirty_minutes_left = 14,
	_multiplayer_event_response_general_fifteen_minutes_left = 15,
	_multiplayer_event_response_general_five_minutes_left = 16,
	_multiplayer_event_response_general_one_minute_left = 17,
	_multiplayer_event_response_general_time_expired = 18,
	_multiplayer_event_response_general_game_over = 19,
	_multiplayer_event_response_general_respawn_tick = 20,
	_multiplayer_event_response_general_last_respawn_tick = 21,
	_multiplayer_event_response_general_teleporter_used = 22,
	_multiplayer_event_response_general_player_changed_team = 23,
	_multiplayer_event_response_general_player_rejoined = 24,
	_multiplayer_event_response_general_gained_lead = 25,
	_multiplayer_event_response_general_gained_team_lead = 26,
	_multiplayer_event_response_general_lost_lead = 27,
	_multiplayer_event_response_general_lost_team_lead = 28,
	_multiplayer_event_response_general_tied_leader = 29,
	_multiplayer_event_response_general_tied_team_leader = 30,
	_multiplayer_event_response_general_round_over = 31,
	_multiplayer_event_response_general_thirty_seconds_left = 32,
	_multiplayer_event_response_general_ten_seconds_left = 33,
	_multiplayer_event_response_general_kill_falling = 34,
	_multiplayer_event_response_general_kill_collision = 35,
	_multiplayer_event_response_general_kill_melee = 36,
	_multiplayer_event_response_general_sudden_death = 37,
	_multiplayer_event_response_general_player_booted_player = 38,
	_multiplayer_event_response_general_kill_flag_carrier = 39,
	_multiplayer_event_response_general_kill_bomb_carrier = 40,
	_multiplayer_event_response_general_kill_sticky_grenade = 41,
	_multiplayer_event_response_general_kill_sniper = 42,
	_multiplayer_event_response_general_kill_stealth_melee = 43,
	_multiplayer_event_response_general_boarded_vehicle = 44,
	_multiplayer_event_response_general_start_team_notification = 45,
	_multiplayer_event_response_general_telefrag = 46,
	_multiplayer_event_response_general_ten_seconds_to_win = 47,
	_multiplayer_event_response_general_team_ten_seconds_to_win = 48,

	// flavor
	_multiplayer_event_response_flavor_double_kill = 0,
	_multiplayer_event_response_flavor_triple_kill = 1,
	_multiplayer_event_response_flavor_killtacular = 2,
	_multiplayer_event_response_flavor_killing_spree = 3,
	_multiplayer_event_response_flavor_running_riot = 4,
	_multiplayer_event_response_flavor_well_placed_kill = 5,
	_multiplayer_event_response_flavor_broke_killing_spree = 6,
	_multiplayer_event_response_flavor_kill_frenzy = 7,
	_multiplayer_event_response_flavor_killtrocity = 8,
	_multiplayer_event_response_flavor_killimajaro = 9,
	_multiplayer_event_response_flavor_fifteen_in_a_row = 10,
	_multiplayer_event_response_flavor_twenty_in_a_row = 11,
	_multiplayer_event_response_flavor_twenty_five_in_a_row = 12,

	// slayer
	_multiplayer_event_response_slayer_game_start = 0,
	_multiplayer_event_response_slayer_new_target = 1,

	// ctf
	_multiplayer_event_response_ctf_game_start = 0,
	_multiplayer_event_response_ctf_flag_taken = 1,
	_multiplayer_event_response_ctf_flag_dropped = 2,
	_multiplayer_event_response_ctf_flag_returned_by_player = 3,
	_multiplayer_event_response_ctf_flag_returned_by_timeout = 4,
	_multiplayer_event_response_ctf_flag_captured = 5,
	_multiplayer_event_response_ctf_flag_newDefensive_team = 6,
	_multiplayer_event_response_ctf_flag_return_faliure = 7,
	_multiplayer_event_response_ctf_side_switch_tick = 8,
	_multiplayer_event_response_ctf_side_switch_final_tick = 9,
	_multiplayer_event_response_ctf_side_switch_thirty_seconds = 10,
	_multiplayer_event_response_ctf_side_switch_ten_seconds = 11,
	_multiplayer_event_response_ctf_flag_contested = 12,
	_multiplayer_event_response_ctf_flag_capture_faliure = 13,

	// oddball
	_multiplayer_event_response_oddball_game_start = 0,
	_multiplayer_event_response_oddball_ball_spawned = 1,
	_multiplayer_event_response_oddball_ball_picked_up = 2,
	_multiplayer_event_response_oddball_ball_dropped = 3,
	_multiplayer_event_response_oddball_ball_reset = 4,
	_multiplayer_event_response_oddball_ball_tick = 5,

	// headhunter
	_multiplayer_event_response_headhunter_game_start = 0,
	_multiplayer_event_response_headhunter_hill_controlled = 1,
	_multiplayer_event_response_headhunter_hill_contested = 2,
	_multiplayer_event_response_headhunter_hill_tick = 3,
	_multiplayer_event_response_headhunter_hill_move = 4,
	_multiplayer_event_response_headhunter_hill_controlled_team = 5,
	_multiplayer_event_response_headhunter_hill_contested_team = 6,

	// king
	_multiplayer_event_response_king_game_start = 0,
	_multiplayer_event_response_king_hill_controlled = 1,
	_multiplayer_event_response_king_hill_contested = 2,
	_multiplayer_event_response_king_hill_tick = 3,
	_multiplayer_event_response_king_hill_move = 4,
	_multiplayer_event_response_king_hill_controlled_team = 5,
	_multiplayer_event_response_king_hill_contested_team = 6,

	// juggernaut
	_multiplayer_event_response_juggernaut_game_start = 0,
	_multiplayer_event_response_juggernaut_new_juggernaut = 1,
	_multiplayer_event_response_juggernaut_juggernaut_killed = 2,

	// territories
	_multiplayer_event_response_territories_game_start = 0,
	_multiplayer_event_response_territories_territory_control_gained = 1,
	_multiplayer_event_response_territories_territory_contest_lost = 2,
	_multiplayer_event_response_territories_all_territories_controlled = 3,
	_multiplayer_event_response_territories_team_territory_ctrl_gained = 4,
	_multiplayer_event_response_territories_team_territory_ctrl_lost = 5,
	_multiplayer_event_response_territories_team_all_territories_cntrld = 6,

	// assault
	_multiplayer_event_response_assault_game_start = 0,
	_multiplayer_event_response_assault_bomb_taken = 1,
	_multiplayer_event_response_assault_bomb_dropped = 2,
	_multiplayer_event_response_assault_bomb_returned_by_player = 3,
	_multiplayer_event_response_assault_bomb_returned_by_timeout = 4,
	_multiplayer_event_response_assault_bomb_captured = 5,
	_multiplayer_event_response_assault_bomb_new_defensive_team = 6,
	_multiplayer_event_response_assault_bomb_return_faliure = 7,
	_multiplayer_event_response_assault_side_switch_tick = 8,
	_multiplayer_event_response_assault_side_switch_final_tick = 9,
	_multiplayer_event_response_assault_side_switch_thirty_seconds = 10,
	_multiplayer_event_response_assault_side_switch_ten_seconds = 11,
	_multiplayer_event_response_assault_bomb_returned_by_defusing = 12,
	_multiplayer_event_response_assault_bomb_placed_on_enemy_post = 13,
	_multiplayer_event_response_assault_bomb_arming_started = 14,
	_multiplayer_event_response_assault_bomb_arming_completed = 15,
	_multiplayer_event_response_assault_bomb_contested = 16,
};


enum e_multiplayer_event_response_audience : uint16
{
	_multiplayer_event_response_audience_cause_player = 0,
	_multiplayer_event_response_audience_cause_team = 1,
	_multiplayer_event_response_audience_effect_player = 2,
	_multiplayer_event_response_audience_effect_team = 3,
	_multiplayer_event_response_audience_all = 4
};

enum e_multiplayer_event_response_audience_filter : uint16
{
	_multiplayer_event_response_audience_filter_none = 0,
	_multiplayer_event_response_audience_filter_cause_player = 1,
	_multiplayer_event_response_audience_filter_cause_team = 2,
	_multiplayer_event_response_audience_filter_effect_player = 3,
	_multiplayer_event_response_audience_filter_effect_team = 4,
};

enum e_multiplayer_event_response_game_type : uint16
{
	_multiplayer_event_response_game_type_general = 0,
	_multiplayer_event_response_game_type_flavor = 1,
	_multiplayer_event_response_game_type_slayer = 2,
	_multiplayer_event_response_game_type_capture_the_flag = 3,
	_multiplayer_event_response_game_type_oddball = 4,
	_multiplayer_event_response_game_type_headhunter = 5,
	_multiplayer_event_response_game_type_king_of_the_hill = 6,
	_multiplayer_event_response_game_type_unused_7 = 7,
	_multiplayer_event_response_game_type_juggernaut = 8,
	_multiplayer_event_response_game_type_territories = 9,
	_multiplayer_event_response_game_type_assault = 10,
	_multiplayer_event_response_game_type_unused_10 = 11,
	_multiplayer_event_response_game_type_unused_11 = 12,
	_multiplayer_event_response_game_type_unused_12 = 13,
	_multiplayer_event_response_game_type_unused_13 = 14,
	k_multiplayer_event_response_game_type_count
};

enum e_multiplayer_response_sound_flags : uint16
{
	_multiplayer_event_response_sound_announcer_sound = 0,
	k_multiplayer_event_response_sound_flags_count
};

/* structures */

struct s_candy_monitor
{
	int32 object_index;
	int32 counter;
};

struct s_game_engine_global_player_info
{
	bool valid;
	uint8 pad_1[3];
	real_point3d point;
	short field_10;
	short field_12;
	uint8 field_14[4];
};
ASSERT_STRUCT_SIZE(s_game_engine_global_player_info, 24);

struct s_simulation_player_netdebug_data
{
	int32 field_0;
	int32 field_4;
	int16 client_rtt_msec;
	int16 client_packet_rate;
	int16 client_throughput;
	int16 client_packet_loss_percentage;
};
ASSERT_STRUCT_SIZE(s_simulation_player_netdebug_data, 16);

struct s_game_engine_globals
{
	uint32 flags;
	int16 team_flags;
	uint16 initial_teams;
	uint16 valid_designators;
	uint16 team_bitmask;
	uint16 active_teams;
	int16 ever_active_teams;
	uint16 initial_team_count;
	uint16 team_designators[8];
	uint32 field_24;
	uint32 field_28;
	int32 player_entity_index[k_maximum_players];
	int16 current_state;
	int16 round_index;
	int16 field_72;
	uint32 gap_74[27];
	int8 round_timer;
	real32 unk_local_player_hud_field[k_number_of_users];
	uint8 field_F4;
	uint8 pad_F5[4];
	uint8 gapF9[3];
	int8 game_engine_globals[520];
	c_game_statborg game_statborg;
	s_game_engine_global_player_info player_info[k_maximum_players];
	uint32 ticks;
	s_simulation_player_netdebug_data netdebug_data[k_maximum_players];
	s_candy_monitor m_candy_monitors[100];
	BYTE gapB3C[264];
	int32 field_C44;
	int8 gap_C45[0xC];
	int32 game_engine_index;
	int8 gapC58[132];
};
ASSERT_STRUCT_SIZE(s_game_engine_globals, 0xCDC);

// max count: 10
struct s_multiplayer_event_sound_response_definition
{
	e_multiplayer_response_sound_flags sound_flags;
	int16 pad;
	tag_reference sounds[k_language_count];
	real32 probability;
};
ASSERT_STRUCT_SIZE(s_multiplayer_event_sound_response_definition, 80);


/* prototypes */

void game_engine_apply_patches();

c_game_engine* current_game_engine(void);

s_game_engine_globals* game_engine_globals_get(void);

c_game_engine** get_game_mode_engines(void);

s_simulation_player_netdebug_data* game_engine_get_netdebug_data(datum player_index);

void __cdecl game_engine_apply_map_patches(void);

bool __cdecl game_engine_get_change_colors(s_player_profile* player_profile, e_game_team team_index, real_rgb_color* change_colors);

void __cdecl game_engine_player_activated(datum player_index);

bool __cdecl game_engine_team_is_enemy(e_game_team a, e_game_team b);

void __cdecl game_engine_render(void);

void __cdecl game_engine_adjust_score(int32 player_index, int32 amount);

bool game_engine_in_round();

c_game_engine** get_game_mode_engines();

void game_engine_list_set(e_game_engine_type type, c_game_engine* engine);

e_network_game_simulation_protocol game_engine_get_simulation_protocol(s_game_variant* variant);