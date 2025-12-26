#pragma once
#include "game_allegiance.h"
#include "player_constants.h"
#include "networking/network_game_definitions.h"
#include "saved_games/game_variant.h"
#include "simulation/machine_id.h"

/* constants */

enum
{
	k_game_results_maximum_game_events = 1000
};

/* enums */

enum e_game_results_player_statistic : int32
{
	_game_results_player_statistic_games_played,
	_game_results_player_statistic_games_quit,
	_game_results_player_statistic_games_disconnected,
	_game_results_player_statistic_games_completed,
	_game_results_player_statistic_games_won,
	_game_results_player_statistic_games_tied,
	_game_results_player_statistic_rounds_won,
	_game_results_player_statistic_kills,
	_game_results_player_statistic_assists,
	_game_results_player_statistic_deaths,
	_game_results_player_statistic_betrayals,
	_game_results_player_statistic_suicides,
	_game_results_player_statistic_most_kills_in_a_row,
	_game_results_player_statistic_seconds_alive,
	_game_results_player_statistic_ctf_flag_scores,
	_game_results_player_statistic_ctf_flag_grabs,
	_game_results_player_statistic_ctf_flag_carrier_kills,
	_game_results_player_statistic_ctf_flag_returns,
	_game_results_player_statistic_ctf_bomb_scores,
	_game_results_player_statistic_ctf_bomb_plants,
	_game_results_player_statistic_ctf_bomb_carrier_kills,
	_game_results_player_statistic_ctf_bomb_grabs,
	_game_results_player_statistic_ctf_bomb_returns,
	_game_results_player_statistic_oddball_time_with_ball,
	_game_results_player_statistic_oddball_unused,
	_game_results_player_statistic_oddball_killas_as_carrier,
	_game_results_player_statistic_oddball_ball_carrier_kills,
	_game_results_player_statistic_king_time_on_hill,
	_game_results_player_statistic_king_total_control_time,
	_game_results_player_statistic_king_unused_1,
	_game_results_player_statistic_king_unused_2,
	_game_results_player_statistic_unused_1,
	_game_results_player_statistic_unused_2,
	_game_results_player_statistic_unused_3,
	_game_results_player_statistic_unused_4,
	_game_results_player_statistic_unused_5,
	_game_results_player_statistic_unused_6,
	_game_results_player_statistic_unused_7,
	_game_results_player_statistic_juggernaut_kills,
	_game_results_player_statistic_juggernaut_kills_as_juggernaut,
	_game_results_player_statistic_juggernaut_total_control_time,
	_game_results_player_statistic_juggernaut_unused_1,
	_game_results_player_statistic_juggernaut_unused_2,
	_game_results_player_statistic_territories_taken,
	_game_results_player_statistic_territories_lost,
	k_game_results_player_statistic_count
};

enum e_game_results_damage_statistic : int32
{
	_game_results_damage_statistic_kills,
	_game_results_damage_statistic_deaths,
	_game_results_damage_statistic_betrayals,
	_game_results_damage_statistic_suicides,
	_game_results_damage_statistic_shots_fired,
	_game_results_damage_statistic_shots_hit,
	_game_results_damage_statistic_headshots,
	_game_results_damage_statistic_unused,
	k_game_results_damage_statistic_count,
};

enum e_game_results_medal_statistic : int32
{
	 _game_results_medal_statistic_multiple_kill_2,
	 _game_results_medal_statistic_multiple_kill_3,
	 _game_results_medal_statistic_multiple_kill_4,
	 _game_results_medal_statistic_multiple_kill_5,
	 _game_results_medal_statistic_multiple_kill_6,
	 _game_results_medal_statistic_multiple_kill_7_or_more,
	 _game_results_medal_statistic_sniper_kill,
	 _game_results_medal_statistic_collision_kill,
	 _game_results_medal_statistic_bash_kill,
	 _game_results_medal_statistic_stealth_kill,
	 _game_results_medal_statistic_killed_vehicle,
	 _game_results_medal_statistic_boarded_vehicle,
	 _game_results_medal_statistic_grenade_stick,
	 _game_results_medal_statistic_5_kills_in_a_row,
	 _game_results_medal_statistic_10_kills_in_a_row,
	 _game_results_medal_statistic_15_kills_in_a_row,
	 _game_results_medal_statistic_20_kills_in_a_row,
	 _game_results_medal_statistic_25_kills_in_a_row,
	 _game_results_medal_statistic_ctf_flag_grab,
	 _game_results_medal_statistic_ctf_flag_carrier_kill,
	 _game_results_medal_statistic_ctf_flag_returned,
	 _game_results_medal_statistic_ctf_bomb_planted,
	 _game_results_medal_statistic_ctf_bomb_carrier_kill,
	 _game_results_medal_statistic_ctf_bomb_defused,
	 _game_results_medal_statistic_unused_1,
	 _game_results_medal_statistic_unused_2,
	 _game_results_medal_statistic_unused_3,
	 _game_results_medal_statistic_unused_4,
	 _game_results_medal_statistic_unused_5,
	 _game_results_medal_statistic_unused_6,
	 _game_results_medal_statistic_unused_7,
	 _game_results_medal_statistic_unused_8,
	 k_game_results_medal_statistic_count
};

enum e_game_results_event_type : int8
{
	_game_results_event_type_unused,
	_game_results_event_type_kill,
	_game_results_event_type_carry,
	_game_results_event_type_score,
	k_game_results_event_type_count,
};


/* structures */

struct s_game_results_globals
{
	bool recording;
	bool recording_paused;
	bool updating;
	bool pad;
	int32 next_game_event_index;
	int32 game_event_timer;
};
ASSERT_STRUCT_SIZE(s_game_results_globals, 12);

struct s_integer_statistic_definition
{
	const char* name;
	int8 unused[4];
	int16 minimum_value;
	int16 maximum_value;
	int32 unknown_value;
};
ASSERT_STRUCT_SIZE(s_integer_statistic_definition, 16);

struct s_integer_statistic
{
	uint16 value : 15;
	uint16 unused : 1;
};
ASSERT_STRUCT_SIZE(s_integer_statistic, 2);


struct s_game_results_player_data
{
	bool exists;
	bool machine_exists;
	int8 padding[2];
	s_machine_identifier machine;
	s_player_configuration player_configuration;
	int32 player_place;
};
ASSERT_STRUCT_SIZE(s_game_results_player_data, 148);

struct s_game_results_team_data
{
	bool exists;
	int8 standing;
	int16 score;
	int8 pad[20];
};
ASSERT_STRUCT_SIZE(s_game_results_team_data, 24);

struct s_game_results_damage_statistics
{
	s_integer_statistic statistics[k_game_results_damage_statistic_count];
};
ASSERT_STRUCT_SIZE(s_game_results_damage_statistics, 16);

struct s_game_results_player_statistics
{
	s_integer_statistic statistics[k_game_results_player_statistic_count];
	s_integer_statistic medal_statistics[k_game_results_medal_statistic_count];
	int16 pad;
	s_game_results_damage_statistics damage[k_damage_reporting_type_count];
	int8 pad2[46];
};
ASSERT_STRUCT_SIZE(s_game_results_player_statistics, 874);

struct s_game_results_player_vs_player_statistics
{
	s_integer_statistic statistic[2];
};
ASSERT_STRUCT_SIZE(s_game_results_player_vs_player_statistics, 4);

union s_game_results_event_data
{
	struct s_game_results_event_kill
	{
		real_vector3d killer_position;
		real_vector3d killed_position;
		int32 damage_reporting_type;

	} kill_event;

	struct s_game_results_event_score
	{
		real_vector3d scorer_position;
		int32 score_type;
		datum weapon_index;

	} score_event;

	struct s_game_results_event_carry
	{
		real_vector3d carrier_position;
		int32 carry_type;
		datum weapon_index;

	} carry_event;

	int8 data[28];
};
ASSERT_STRUCT_SIZE(s_game_results_event_data, 28);

struct s_game_results_event
{
	e_game_results_event_type type;
	int8 player_references[2];
	int8 pad0;
	s_game_results_event_data data;
	uint32 time;
};
ASSERT_STRUCT_SIZE(s_game_results_event, 0x24);

class c_game_results
{
public:
	uint8 m_game_end_reason;
	bool m_initialized;
	bool m_finalized;
	bool m_unreliable;
	bool m_is_matchmade_game;
	int64 m_random_data;
	s_game_variant m_game_variant;
	int32 m_map_id;
	wchar_t m_scenario_path[MAX_PATH];
	bool m_started;
	int32 m_start_time;
	bool m_finished;
	int32 m_finish_time;
	int32 m_player_count_maybe;
	s_game_results_player_data m_players[k_maximum_players];
	s_game_results_team_data m_teams[k_maximum_teams];
	s_game_results_player_statistics m_player_statistics[k_maximum_players];
	s_game_results_player_vs_player_statistics m_pvp_statistics[k_maximum_players][k_maximum_players];
	s_game_results_event m_game_events[k_game_results_maximum_game_events];

private:
	// todo: a very weird storage for s_machine_identifiers everything is accessed in a shifted pattern needs more work in the future
	int8 gap[1796];
};
ASSERT_STRUCT_SIZE(c_game_results, 0xDC68);

/* prototypes */

void game_results_stop_recording(void);
void game_results_set_recording_pause(bool pause);
bool game_results_get_game_recording(void);
bool game_results_get_game_updating(void);
void game_results_start_updating(void);
void game_results_stop_updating(void);

void __cdecl game_results_update(void);
c_game_results* game_results_get(void);