#pragma once

#include "game_engine_ctf.h"
#include "game_engine_headhunter.h"
#include "game_engine_juggernaut.h"
#include "game_engine_king.h"
#include "game_engine_oddball.h"
#include "game_engine_slayer.h"
#include "game_engine_territories.h"
#include "game_engine_test.h"
#include "game/game_allegiance.h"
#include "game_statborg.h"
#include "math/color_math.h"
#include "tag_files/string_id.h"
#include "tag_files/tag_block.h"
#include "tag_files/tag_reference.h"

/* constants */

#define k_maximum_game_engine_event_responses_per_type 128

static c_ctf_engine g_game_engine_ctf;
static c_juggernaut_engine g_juggernaut_engine;
static c_slayer_engine g_slayer_engine;
static c_king_engine g_king_engine;
static c_territories_engine g_territories_engine;
static c_oddball_engine g_oddball_engine;
static c_game_engine* g_test_engine_ptr = &g_test_engine;
static c_headhunter_engine* g_headhunter_engine_ptr = &g_headhunter_engine;
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

enum e_multiplayer_event_response_definition_flags : int16
{
	_multiplayer_event_response_definition_flag_quantity_message = FLAG(0)
};

enum e_multiplayer_event : int16
{
	_multiplayer_event_game_start = 0,
	_multiplayer_event_hill_controlled = 1,
	_multiplayer_event_hill_contested = 2,
	_multiplayer_event_hill_tick = 3,
	_multiplayer_event_hill_move = 4,
	_multiplayer_event_hill_controlled_team = 5,
	_multiplayer_event_hill_contested_team = 6
};

enum e_multiplayer_event_audience : int16
{
	_multiplayer_event_audience_cause_player = 0,
	_multiplayer_event_audience_cause_team = 1,
	_multiplayer_event_audience_effect_player = 2,
	_multiplayer_event_audience_effect_team = 3,
	_multiplayer_event_audience_all = 4
};

enum e_multiplayer_event_audience_type : int16
{
	_multiplayer_event_audience_type_none = 0,
	_multiplayer_event_audience_type_cause_player = 1,
	_multiplayer_event_audience_type_cause_team = 2,
	_multiplayer_event_audience_type_effect_player = 3,
	_multiplayer_event_audience_type_effect_team = 4
};

enum e_multiplayer_event_sound_flags : int16
{
	_multiplayer_event_sound_flag_announcer_sound = FLAG(0),
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

// max count: 1
struct s_sound_response_extra_sounds
{
	tag_reference japanese_sound;	// snd!
	tag_reference german_sound;		// snd!
	tag_reference french_sound;		// snd!
	tag_reference spanish_sound;	// snd!
	tag_reference italian_sound;	// snd!
	tag_reference korean_sound;		// snd!
	tag_reference chinese_sound;	// snd!
	tag_reference portuguese_sound;	// snd!
};
ASSERT_STRUCT_SIZE(s_sound_response_extra_sounds, 64);


// max count: 10
struct s_multiplayer_event_sound_response_definition
{
	e_multiplayer_event_sound_flags sound_flags;
	int16 pad;
	tag_reference english_sound;
	s_sound_response_extra_sounds extra_sounds;
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

bool game_engine_in_round();

c_game_engine** get_game_mode_engines();

void test_replace_game_engine_mode(e_game_engine_type type, c_game_engine* engine);

e_network_game_simulation_protocol game_engine_get_simulation_protocol(s_game_variant* variant);