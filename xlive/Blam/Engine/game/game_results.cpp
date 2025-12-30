#include "stdafx.h"
#include "game_results.h"

#include "players.h"
#include "objects/objects.h"
#include "shell/shell_windows.h"

/* constants */

static const real_point3d g_game_results_invalid_player_location{ 0, 0, 500.f };

/* prototypes */

static s_game_results_globals* game_results_globals_get(void);
static s_integer_statistic_definition* game_results_player_statistic_definition_get();
static s_integer_statistic_definition* game_results_damage_statistic_definition_get();
static s_integer_statistic_definition* game_results_pvp_statistic_definition_get();
static s_integer_statistic_definition* game_results_medal_statistic_definition_get();

/* public code */

void game_results_stop_recording(void)
{
	game_results_globals_get()->recording = false;
	return;
}

void game_results_set_recording_pause(bool pause)
{
	game_results_globals_get()->recording_paused = pause;
	return;
}

bool game_results_get_game_recording(void)
{
	return game_results_globals_get()->recording;
}

bool game_results_get_game_updating(void)
{
	return game_results_globals_get()->updating;
}

void game_results_start_updating(void)
{
	game_results_globals_get()->updating = true;
	return;
}

void game_results_stop_updating(void)
{
	game_results_globals_get()->updating = false;
	return;
}

void __cdecl game_results_update(void)
{
	INVOKE(0x692CC, 0x68CE4, game_results_update);
	return;
}

c_game_results* game_results_get()
{
	return Memory::GetAddress<c_game_results*>(0x4B1C90, 0x4DC3C0);
}

int32 game_results_get_recording_statistic(int32 player_index, int32 team_index, e_game_results_player_statistic statistic)
{
	//return INVOKE(0x66D3C, 0, game_results_get_recording_statistic, player_index, team_index, statistic);
	int32 result = NONE;
	c_game_results* game_results = game_results_get();
	s_game_results_globals* game_results_globals = game_results_globals_get();

	if (game_results_globals->recording)
	{
		if (player_index != NONE)
			result = game_results->m_player_statistics[player_index].statistics[statistic].value & SHRT_MAX;
		if (team_index != NONE)
			result = game_results->m_team_statistics[team_index].statistics[statistic].value & SHRT_MAX;
	}

	return result;
}

int32 game_results_get_finalized_statistic(int32 player_index, int32 team_index, e_game_results_player_statistic statistic)
{
	//return INVOKE(0x66D88, 0, game_results_get_finalized_statistic, player_index, team_index, statistic);
	int32 result = NONE;
	c_game_results* game_results = game_results_get();

	if (game_results->m_game_end_reason && game_results->m_initialized)
	{
		if (player_index != NONE)
			result = game_results->m_player_statistics[player_index].statistics[statistic].value & SHRT_MAX;
		if (team_index != NONE)
			result = game_results->m_team_statistics[team_index].statistics[statistic].value & SHRT_MAX;
	}

	return result;
}

int32 game_results_get_finalized_damage_statistic(int32 player_index, e_game_results_damage_statistic statistic, e_damage_reporting_type damage_type)
{
	//return INVOKE(0x66DDD, 0, game_results_get_finalized_damage_statistic, player_index, statistic, damage_type);
	int32 result = NONE;
	c_game_results* game_results = game_results_get();

	if (game_results->m_game_end_reason && game_results->m_initialized)
	{
		result = game_results->m_player_statistics[player_index].damage[damage_type].statistics[statistic].value & SHRT_MAX;
	}

	return result;
}

int32 game_results_get_finalized_medal_statistic(int32 player_index, e_game_results_medal_statistic medal)
{
	//return INVOKE(0x66E15, 0, game_results_get_finalized_medal_statistic, player_index, medal);
	int32 result = NONE;
	c_game_results* game_results = game_results_get();

	if (game_results->m_game_end_reason && game_results->m_initialized)
	{
		result = game_results->m_player_statistics[player_index].medal_statistics[medal].value & SHRT_MAX;
	}

	return result;
}

int32 game_results_get_finalized_pvp_statistic(int32 player_index, int32 vs_player_index, e_game_results_player_vs_player_statistic statistic)
{
	//return INVOKE(0x66E46, 0, game_results_get_finalized_pvp_statistic, player_index, vs_player_index, statistic);
	int32 result = NONE;
	c_game_results* game_results = game_results_get();

	if (game_results->m_game_end_reason && game_results->m_initialized)
	{
		result = game_results->m_pvp_statistics[player_index][vs_player_index].statistic[statistic].value & SHRT_MAX;
	}

	return result;
}

int32 game_results_get_finalized_player_score(int32 player_index)
{
	//return INVOKE(0x66F43, 0, game_results_get_finalized_player_score, player_index);
	int32 result = NONE;
	c_game_results* game_results = game_results_get();

	if (game_results->m_game_end_reason &&
		game_results->m_initialized &&
		game_results->m_players[player_index].exists)
	{
		result = game_results->m_players[player_index].score;
	}

	return result;
}

int32 game_results_get_finalized_player_place(int32 player_index)
{
	//return INVOKE(0x699BD, 0, game_results_get_finalized_player_place, player_index);
	int32 result = NONE;
	c_game_results* game_results = game_results_get();

	if (game_results->m_game_end_reason &&
		game_results->m_initialized &&
		game_results->m_players[player_index].exists)
	{
		result = game_results->m_players[player_index].player_place;
	}

	return result;
}

s_player_configuration* game_results_get_finalized_player_configuration(int32 player_index)
{
	//return INVOKE(0x66FDC, 0, game_results_get_finalized_player_configuration, player_index);
	s_player_configuration* result = nullptr;
	c_game_results* game_results = game_results_get();

	if (game_results->m_game_end_reason &&
		game_results->m_initialized &&
		game_results->m_players[player_index].exists)
	{
		result = &game_results->m_players[player_index].player_configuration;
	}

	return result;
}

int8* game_results_get_finalized_player_unknown_02(int32 player_index)
{
	//return INVOKE(0x67012, 0, game_results_get_finalized_player_unknown_02, player_index);
	int8* result = nullptr;
	c_game_results* game_results = game_results_get();

	if (game_results->m_game_end_reason &&
		game_results->m_initialized &&
		game_results->m_players[player_index].exists)
	{
		result = game_results->m_players[player_index].unk_02;
	}

	return result;
}

e_game_team game_results_get_finalized_player_team(int32 player_index)
{
	//return INVOKE(0x67042, 0, game_results_get_finalized_player_team, player_index);
	e_game_team result = _game_team_observer;
	c_game_results* game_results = game_results_get();

	if (game_results->m_game_end_reason &&
		game_results->m_initialized &&
		game_results->m_players[player_index].exists)
	{
		result = (e_game_team)game_results->m_players[player_index].player_configuration.team_index;
	}

	return result;
}

void game_results_get_finalized_player_profile_traits(int32 player_index, s_player_profile_traits* profile_traits)
{
	//INVOKE(0x6706D, 0, game_results_get_finalized_player_profile_traits, player_index, profile_traits);
	c_game_results* game_results = game_results_get();

	if (game_results->m_initialized && player_index != NONE && game_results->m_players[player_index].exists)
	{
		*profile_traits = game_results->m_players[player_index].player_configuration.profile_traits;
	}
	else
	{
		player_profile_traits_initialize(profile_traits);
	}
}

bool game_results_get_player_position(real_point3d* position, int32 player_index)
{
	//usercall 0x6928E
	player_datum* player = (player_datum*)datum_get(player_data_get(), player_index);

	if (player->unit_index != NONE && player->dead_unit_index != NONE)
	{
		object_get_origin(player->unit_index, position, false);
		return true;
	}

	return false;
}

int32 game_results_get_finalized_team_score(e_game_team team)
{
	//return INVOKE(0x66F74, 0, game_results_get_finalized_team_score, team);
	int32 result = NONE;
	c_game_results* game_results = game_results_get();

	if (game_results->m_game_end_reason && team < k_game_multiplayer_team_count && game_results->m_initialized)
	{
		result = game_results->m_teams[team].score;
	}

	return result;
}

int32 game_results_get_finalized_team_place(e_game_team team)
{
	//return INVOKE(0x66FA8, 0, game_results_get_finalized_team_place, team);
	int32 result = NONE;
	c_game_results* game_results = game_results_get();

	if (game_results->m_game_end_reason && team < k_game_multiplayer_team_count && game_results->m_initialized)
	{
		result = game_results->m_teams[team].place;
	}

	return result;
}

void game_results_set_statistic(int32 player_index, e_game_team team, e_game_results_player_statistic statistic, int32 value)
{
	//INVOKE(0x66CEE, 0, game_results_set_statistic, player_index, team, statistic, value);
	c_game_results* game_results = game_results_get();
	s_game_results_globals* game_results_globals = game_results_globals_get();

	if (game_results_globals->recording && !game_results_globals->recording_paused)
	{
		s_integer_statistic_definition* definition = &game_results_player_statistic_definition_get()[statistic];

		int32 clean_value = PIN(value, definition->minimum_value, definition->maximum_value);

		if (player_index != NONE)
		{
			uint16* stat = (uint16*)&game_results->m_player_statistics[player_index].statistics[statistic];
			*stat = ((*stat & ~SHRT_MAX) | ((clean_value) & SHRT_MAX));
		}

		if (team != _game_team_observer)
		{
			uint16* stat = (uint16*)&game_results->m_team_statistics[team].statistics[statistic];
			*stat = ((*stat & ~SHRT_MAX) | ((clean_value)&SHRT_MAX));
		}
	}
}

void game_results_increment_statistic(int32 player_index, e_game_team team, e_game_results_player_statistic statistic, int32 amount)
{
	//INVOKE(0x66BD2, 0, game_results_increment_statistic, player_index, team, statistic, amount);
	c_game_results* game_results = game_results_get();
	s_game_results_globals* game_results_globals = game_results_globals_get();

	if (game_results_globals->recording && !game_results_globals->recording_paused)
	{
		s_integer_statistic_definition* definition = &game_results_player_statistic_definition_get()[statistic];

		if (player_index != NONE)
		{
			int32 new_value = game_results->m_player_statistics[player_index].statistics[statistic].value + amount;

			int32 clean_value = PIN(
				new_value, 
				definition->minimum_value, 
				definition->maximum_value);

			uint16* stat = (uint16*)&game_results->m_player_statistics[player_index].statistics[statistic];
			*stat = ((*stat & ~SHRT_MAX) | ((clean_value)&SHRT_MAX));
		}

		if (team != _game_team_observer)
		{
			int32 new_value = game_results->m_team_statistics[team].statistics[statistic].value + amount;

			int32 clean_value = PIN(
				new_value,
				definition->minimum_value,
				definition->maximum_value);

			uint16* stat = (uint16*)&game_results->m_team_statistics[team].statistics[statistic];
			*stat = ((*stat & ~SHRT_MAX) | ((clean_value)&SHRT_MAX));
		}
	}
}

void game_results_increment_pvp_statistic(int32 player_index, int32 vs_player_index, e_game_results_player_vs_player_statistic statistic, int32 amount)
{
	//INVOKE(0x670C2, 0, game_results_increment_pvp_statistic, player_index, vs_player_index, statistic, amount);
	c_game_results* game_results = game_results_get();
	s_game_results_globals* game_results_globals = game_results_globals_get();

	if (game_results_globals->recording && !game_results_globals->recording_paused)
	{
		s_integer_statistic_definition* definition = &game_results_pvp_statistic_definition_get()[statistic];

		int32 new_value = game_results->m_pvp_statistics[player_index][vs_player_index].statistic[statistic].value + amount;

		int32 clean_value = PIN(
			new_value,
			definition->maximum_value,
			definition->maximum_value
		);

		uint16* stat = (uint16*)&game_results->m_pvp_statistics[player_index][vs_player_index].statistic[statistic];
		*stat = ((*stat & ~SHRT_MAX) | ((clean_value)&SHRT_MAX));
	}
}

void game_results_increment_damage_statistic(int32 player_index, e_game_results_damage_statistic statistic, e_damage_reporting_type damage_type, int32 amount)
{
	//INVOKE(0x67149, 0, game_results_increment_damage_statistic, player_index, statistic, damage_type, amount);
	c_game_results* game_results = game_results_get();
	s_game_results_globals* game_results_globals = game_results_globals_get();

	if (game_results_globals->recording && !game_results_globals->recording_paused)
	{
		s_integer_statistic_definition* definition = &game_results_damage_statistic_definition_get()[statistic];

		int32 new_value = game_results->m_player_statistics[player_index].damage[damage_type].statistics[statistic].value + amount;

		int32 clean_value = PIN(
			new_value,
			definition->minimum_value,
			definition->maximum_value
		);

		uint16* stat = (uint16*)&game_results->m_player_statistics[player_index].damage[damage_type].statistics[statistic];
		*stat = ((*stat & ~SHRT_MAX) | ((clean_value)&SHRT_MAX));
	}
}

void game_results_increment_medal_statistic(int32 player_index, e_game_results_medal_statistic medal, int32 amount)
{
	//INVOKE(0x6738C, 0, game_results_increment_medal_statistic, player_index, medal, amount);
	c_game_results* game_results = game_results_get();
	s_game_results_globals* game_results_globals = game_results_globals_get();

	if (game_results_globals->recording && !game_results_globals->recording_paused)
	{
		s_integer_statistic_definition* definition = &game_results_medal_statistic_definition_get()[medal];

		int32 new_value = game_results->m_player_statistics[player_index].medal_statistics[medal].value + amount;

		int32 clean_value = PIN(
			new_value,
			definition->minimum_value,
			definition->maximum_value
		);

		uint16* stat = (uint16*)&game_results->m_player_statistics[player_index].medal_statistics[medal];
		*stat = ((*stat & ~SHRT_MAX) | ((clean_value)&SHRT_MAX));
	}
}

void game_results_insert_event(const s_game_results_event* event)
{
	//INVOKE(0x67411, 0, game_results_insert_event, event);
	c_game_results* game_results = game_results_get();
	s_game_results_globals* game_results_globals = game_results_globals_get();

	if (game_results_globals->recording && !game_results_globals->recording_paused)
	{
		int32 event_index = game_results_globals->next_game_event_index;
		csmemcpy(&game_results->m_game_events[event_index], event, sizeof(s_game_results_event));
		game_results_globals->next_game_event_index = (++event_index) % 1000;
	}
}

void game_results_insert_kill_event(int16 player_index, int16 killed_player_index, int8 damage_reporting_info)
{
	//INVOKE(0x69A27, 0, game_results_insert_kill_event, player_index, killed_player_index, damage_reporting_info);
	c_game_results* game_results = game_results_get();
	s_game_results_globals* game_results_globals = game_results_globals_get();

	if (game_results_globals->recording && !game_results_globals->recording_paused)
	{
		s_game_results_event event{};

		event.type = _game_results_event_type_kill;
		event.player_references[0] = (int8)player_index;
		event.player_references[1] = (int8)killed_player_index;
		event.time = system_seconds() - game_results->m_start_time;
		event.data.kill_event.damage_reporting_type = (int32)damage_reporting_info;

		real_point3d player_position = g_game_results_invalid_player_location;
		real_point3d killed_player_position = g_game_results_invalid_player_location;

		if (game_results_get_player_position(&player_position, player_index) &&
			game_results_get_player_position(&killed_player_position, killed_player_index))
		{
			event.data.kill_event.killer_position = player_position;
			event.data.kill_event.killed_position = killed_player_position;

			game_results_insert_event(&event);
		}
	}
}

void game_results_insert_score_event(int16 player_index, int32 score_type, datum weapon_index)
{
	//INVOKE(0x69AF1, 0, game_results_insert_score_event, player_index, score_type, weapon_index);
	c_game_results* game_results = game_results_get();
	s_game_results_globals* game_results_globals = game_results_globals_get();

	if (game_results_globals->recording && !game_results_globals->recording_paused)
	{
		s_game_results_event event{};

		event.type = _game_results_event_type_score;
		event.player_references[0] = (int8)player_index;
		event.player_references[1] = NONE;
		event.time = system_seconds() - game_results->m_start_time;
		event.data.score_event.score_type = score_type;
		event.data.score_event.weapon_index = weapon_index;

		real_point3d player_position = g_game_results_invalid_player_location;

		if (game_results_get_player_position(&player_position, player_index))
		{
			event.data.score_event.scorer_position = player_position;

			game_results_insert_event(&event);
		}
	}
}

void game_results_insert_carry_event(int16 player_index, datum weapon_index, int32 carry_type)
{
	//INVOKE(0x69C5F, 0, game_results_insert_carry_event, player_index, weapon_index, carry_type);
	c_game_results* game_results = game_results_get();
	s_game_results_globals* game_results_globals = game_results_globals_get();

	if (game_results_globals->recording && !game_results_globals->recording_paused)
	{
		uint32 current_time = system_seconds();
		if (game_results_globals->game_event_timer - current_time > k_game_results_event_cooldown)
		{
			s_game_results_event event{};

			event.type = _game_results_event_type_carry;
			event.player_references[0] = (int8)player_index;
			event.player_references[1] = NONE;
			event.time = system_seconds() - game_results->m_start_time;
			event.data.carry_event.weapon_index = weapon_index;
			event.data.carry_event.carry_type = carry_type;

			real_point3d player_position = g_game_results_invalid_player_location;

			if (game_results_get_player_position(&player_position, player_index))
			{
				event.data.carry_event.carrier_position = player_position;

				game_results_insert_event(&event);
			}

			game_results_globals->game_event_timer = current_time;
		}
	}
}


/* private code */

s_game_results_globals* game_results_globals_get(void)
{
	return Memory::GetAddress<s_game_results_globals*>(0x4B1C80, 0x4DC3B0);
}

s_integer_statistic_definition* game_results_player_statistic_definition_get()
{
	return Memory::GetAddress<s_integer_statistic_definition*>(0x412CF8, 0x3B62D0);
}

s_integer_statistic_definition* game_results_damage_statistic_definition_get()
{
	return Memory::GetAddress<s_integer_statistic_definition*>(0x412FC8, 0x3B65A0);
}

s_integer_statistic_definition* game_results_pvp_statistic_definition_get()
{
	return Memory::GetAddress<s_integer_statistic_definition*>(0x413038, 0x3B6610);
}

s_integer_statistic_definition* game_results_medal_statistic_definition_get()
{
	return Memory::GetAddress<s_integer_statistic_definition*>(0x413058, 0x3B6630);
}
