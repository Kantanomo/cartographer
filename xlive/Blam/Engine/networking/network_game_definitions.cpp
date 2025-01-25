#include "stdafx.h"
#include "network_game_definitions.h"

#include "game/game_engine_territories.h"
#include "game/players.h"

/* typedef */
typedef bool(__cdecl* t_network_game_definitions_decode_game_variant)(c_bitstream* packet, s_game_variant* variant);
t_network_game_definitions_decode_game_variant p_network_game_definitions_decode_game_variant;

/* public */

void network_game_definitions_apply_patches()
{
	DETOUR_ATTACH(p_network_game_definitions_decode_game_variant, Memory::GetAddress<t_network_game_definitions_decode_game_variant>(0x1E2F74, 0x1B45BC), network_game_definitions_decode_game_variant);
}

bool network_game_definitions_decode_game_variant(c_bitstream* packet, s_game_variant* variant)
{
	variant->variant_game_engine_index = (e_game_engine_type)packet->read_integer("variant-game-engine-index", k_game_engine_type_bits_required);

	if (!variant->variant_game_engine_index)
	{
		csmemset(variant, 0, sizeof(s_game_variant));
		return false;
	}

	variant->flags = packet->read_integer("variant-flags", 1);

	packet->read_string_wchar("variant-desc", variant->variant_name, NUMBEROF(variant->variant_name));

	variant->description_index = (e_game_variant_description_index)packet->read_integer("description-index", k_game_variant_description_bits_required);

	variant->game_engine_flags.set_unsafe(packet->read_integer("flags", k_game_engine_flags_bits_required));

	variant->round_setting = (e_game_engine_round_setting)packet->read_integer("round-setting", k_game_engine_round_setting_bits_required);

	variant->score_to_win_round = packet->read_integer("score-to-win-round", SHORT_BITS);

	variant->round_time_limit = packet->read_integer("round-time-limit", SHORT_BITS);

	variant->join_in_progress_setting = (e_game_engine_join_in_progress)packet->read_integer("join-in-progress", k_game_engine_join_in_progress_bits_required);

	variant->max_players = packet->read_integer("max-players", bits_required_for(k_maximum_players));

	variant->max_living_players = packet->read_integer("max-living-players", bits_required_for(k_maximum_players));

	variant->lives_per_round = packet->read_integer("lives-per-round", SHORT_BITS);

	variant->respawn_time = packet->read_integer("respawn-time", SHORT_BITS);

	variant->suicide_penalty = packet->read_integer("suicide-penalty", SHORT_BITS);

	variant->shield_setting = (e_game_engine_shield_setting)packet->read_integer("shield-setting", k_game_engine_shield_setting_bits_required);

	variant->team_score_setting = (e_game_engine_team_score)packet->read_integer("team-score-setting", k_game_engine_team_score_bits_required);

	variant->team_respawn_setting = (e_game_engine_team_respawn)packet->read_integer("team-respawn-setting", k_game_engine_team_respawn_bits_required);

	variant->betrayal_penalty = packet->read_integer("betrayal-penalty", SHORT_BITS);

	variant->maximum_allowable_teams = packet->read_integer("max-allowable-teams", bits_required_for(k_game_multiplayer_team_count));

	variant->vehicle_respawn_setting = (e_game_engine_respawn_setting)packet->read_integer("vehicle-respawn-setting", k_game_engine_respawn_setting_bits_required);

	variant->primary_light_land_vehicle = (e_game_engine_light_land_vehicle)packet->read_integer("primary-light-land-vehicle", k_game_engine_light_land_vehicle_bits_required);

	variant->secondary_light_land_vehicle = (e_game_engine_light_land_vehicle)packet->read_integer("secondary-light-land-vehicle", k_game_engine_light_land_vehicle_bits_required);

	variant->primary_heavy_land_vehicle = (e_game_engine_heavy_land_vehicle)packet->read_integer("primary-heavy-land-vehicle", k_game_engine_heavy_land_vehicle_bits_required);

	variant->primary_flying_vehicle = (e_game_engine_flying_vehicle)packet->read_integer("primary-flying-vehicle", k_game_engine_flying_vehicle_bits_required);

	variant->secondary_heavy_land_vehicle = (e_game_engine_heavy_land_vehicle)packet->read_integer("secondary-heavy-land-vehicle", k_game_engine_heavy_land_vehicle_bits_required);

	variant->primary_turret_vehicle = (e_game_engine_turret_vehicle)packet->read_integer("primary-turret-vehicle", k_game_engine_turret_vehicle_bits_required);

	variant->secondary_turret_vehicle = (e_game_engine_turret_vehicle)packet->read_integer("secondary-turret-vehicle", k_game_engine_turret_vehicle_bits_required);

	variant->weapon_set = (e_game_engine_weapon_set)packet->read_integer("weapon-set", k_game_engine_weapon_set_bits_required);

	variant->weapon_respawn_setting = (e_game_engine_respawn_setting)packet->read_integer("weapon-respawn-setting", k_game_engine_respawn_setting_bits_required);

	variant->starting_equipment_primary = (e_game_engine_starting_weapon)packet->read_integer("starting-equipment-primary", k_game_engine_starting_weapon_bits_required);

	variant->starting_equipment_secondary = (e_game_engine_starting_weapon)packet->read_integer("starting-equipment-secondary", k_game_engine_starting_weapon_bits_required);

	switch(variant->variant_game_engine_index)
	{
	case _game_engine_type_ctf:
		{
			variant->game_engine_variant.ctf.flags.set_unsafe(packet->read_integer("flags", k_ctf_engine_flags_bits_required));
			variant->game_engine_variant.ctf.flag_reset_time = packet->read_integer("flag-reset-time", SHORT_BITS);
			variant->game_engine_variant.ctf.speed_with_flag = (e_ctf_engine_player_speed)packet->read_integer("speed-with-flag", k_ctf_engine_player_speed_bits_required);
			variant->game_engine_variant.ctf.flag_hit_damage = (e_game_engine_weapon_hit)packet->read_integer("flag-hit-damage", k_game_engine_weapon_hit_bits_required);
			variant->game_engine_variant.ctf.waypoint_type = (e_ctf_engine_enemy_bomb_waypoint_type)packet->read_integer("waypoint-to-home-flag-type", k_ctf_engine_enemy_bomb_waypoint_type_bits_required);
			variant->game_engine_variant.ctf.game_type = (e_ctf_game_type)packet->read_integer("game-type", k_ctf_game_type_bits_required);
			break;
		}
	case _game_engine_type_slayer:
		{
			variant->game_engine_variant.slayer.flags.set_unsafe(packet->read_integer("flags", k_slayer_engine_flags_bits_required));
			break;
		}
	case _game_engine_type_oddball:
		{
			variant->game_engine_variant.oddball.flags.set_unsafe(packet->read_integer("flags", k_oddball_engine_flags_bits_required));
			variant->game_engine_variant.oddball.ball_count = packet->read_integer("ball-count", bits_required_for(k_game_engine_oddball_maximum_balls));
			variant->game_engine_variant.oddball.ball_hit_damage = (e_game_engine_weapon_hit)packet->read_integer("ball-hit-damage", k_game_engine_weapon_hit_bits_required);
			variant->game_engine_variant.oddball.speed_with_ball = (e_oddball_player_speed)packet->read_integer("speed-with-ball", k_oddball_player_speed_bits_required);
			variant->game_engine_variant.oddball.waypoint_to_ball = (e_oddball_engine_waypoint_type)packet->read_integer("waypoint-to-ball", k_oddball_waypoint_type_bits_required);
			break;
		}
	case _game_engine_type_koth:
		{
			variant->game_engine_variant.king.flags.set_unsafe(packet->read_integer("flags", k_king_engine_flags_bits_required));
			variant->game_engine_variant.king.hill_move_time = packet->read_integer("hill-move-time", SHORT_BITS);
			break;
		}
	case _game_engine_type_race:
		{
			break;
		}
	case _game_engine_type_headhunter:
		{
			break;
		}
	case _game_engine_type_juggernaut:
		{
			variant->game_engine_variant.juggernaut.flags.set_unsafe(packet->read_integer("flags", k_juggernaut_engine_flags_bits_required));
			variant->game_engine_variant.juggernaut.juggernaut_movement_speed = (e_ctf_engine_player_speed)packet->read_integer("juggernaut-movement-speed", k_ctf_engine_player_speed_bits_required);
			break;
		}
	case _game_engine_type_territories:
		{
			variant->game_engine_variant.territories.territory_count = packet->read_integer("territory-count", bits_required_for(k_maximum_territories_flags));
			variant->game_engine_variant.territories.territory_contest_time = packet->read_integer("territory-contest-time", SHORT_BITS);
			variant->game_engine_variant.territories.territory_capture_time = packet->read_integer("territory-capture-time", SHORT_BITS);
			break;
		}
	case _game_engine_type_assault:
		{
			variant->game_engine_variant.assault.bomb_arming_time = packet->read_integer("bomb-arming-time", SHORT_BITS);
			variant->game_engine_variant.assault.bomb_fuse_time = packet->read_integer("bomb-fuse-time", SHORT_BITS);
			variant->game_engine_variant.assault.flags.set_unsafe(packet->read_integer("flags", k_ctf_engine_flags_bits_required));
			variant->game_engine_variant.assault.flag_reset_time = packet->read_integer("flag-reset-time", SHORT_BITS);
			variant->game_engine_variant.assault.speed_with_flag = (e_ctf_engine_player_speed)packet->read_integer("speed-with-flag", k_ctf_engine_player_speed_bits_required);
			variant->game_engine_variant.assault.flag_hit_damage = (e_game_engine_weapon_hit)packet->read_integer("flag-hit-damage", k_game_engine_weapon_hit_bits_required);
			variant->game_engine_variant.assault.waypoint_type = (e_ctf_engine_enemy_bomb_waypoint_type)packet->read_integer("waypoint-to-home-flag-type", k_ctf_engine_enemy_bomb_waypoint_type_bits_required);
			variant->game_engine_variant.assault.game_type = (e_ctf_game_type)packet->read_integer("game-type", k_ctf_game_type_bits_required);
			break;
		}
	case _game_engine_type_none:
		return true; // native behavior??? why coop?

	default:
		return false;
	}

	bool result =  game_variant_is_valid(variant);
	return result;
}
