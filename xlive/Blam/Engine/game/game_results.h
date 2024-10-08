#pragma once
#include "players.h"

#define k_game_results_tracked_damage_count 45

/* enums */

enum e_game_results_event_type : int8
{
	_game_results_event_type_none,
	_game_results_event_type_kill,
	_game_results_event_type_score,
	_game_results_event_type_carry,
	k_game_results_event_type_count
};

enum e_game_results_damage_statistic_type
{
	_game_results_damage_statistic_kills,
	_game_results_damage_statistic_deaths,
	_game_results_damage_statistic_suicide,
	_game_results_damage_statistic_shots_fired,
	_game_results_damage_statistic_shots_hit,
	_game_results_damage_statistic_head_shots,
	_game_results_damage_statistic_unk,
	k_game_results_damage_statistic_count
};

/* structures */

struct s_game_result_event_player_kill
{
	real_vector3d source_player_position;
	real_vector3d effected_player_position;
	int32 statistic_index;
};

struct s_game_result_event_player_score
{
	real_vector3d scoring_player_position;
	int32 unk_type;
	int32 score_type;
	int8 pad[8];
};

struct s_game_result_event_player_carry
{
	real_vector3d source_player_position;
	int32 unk_type;
	int32 carry_type;
	int8 pad[8];
};

union s_game_result_event_data
{
	int8 data[28];
	s_game_result_event_player_kill kill_event;
	s_game_result_event_player_score score_event;
	s_game_result_event_player_carry carry_event;
};

struct s_game_result_event
{
	e_game_results_event_type type;
	int8 source_player_index;
	int8 effected_player_index;
	int8 pad_3;
	s_game_result_event_data data;
	int32 time_stamp;
};
ASSERT_STRUCT_SIZE(s_game_result_event, 36);

struct s_game_results_globals
{
	bool recording;
	bool recording_pause;
	bool updating;
};

struct s_game_results_player
{
	bool exists;
	int8 gap1[15];
	s_player_properties properties;
};
ASSERT_STRUCT_SIZE(s_game_results_player, 0x94);

struct s_game_results_team
{
	bool exists;
	int8 gap1[23];
};
ASSERT_STRUCT_SIZE(s_game_results_team, 0x18);

class c_game_results
{
	bool initialized;
	bool finalized;
	int8 gap2[322];
	wchar_t scenario_path[260];
	bool started;
	int8 gap_3[19];
	s_game_results_player players[16];
	s_game_results_team teams[16];
	int8 gap34D[52806];
};


/* prototypes */

void game_results_stop_recording(void);
void game_results_set_recording_pause(bool pause);
bool game_results_get_game_recording(void);
bool game_results_get_game_updating(void);
void game_results_start_updating(void);
void game_results_stop_updating(void);
