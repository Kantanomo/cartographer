#include "stdafx.h"
#include "GunGame.h"

#include "game/game.h"
#include "game/players.h"
#include "networking/network_event.h"
#include "units/units.h"

#include "H2MOD.h"

// TODO(PermaNull): Add additional levels with dual weilding

e_weapons_datum_index k_level_weapons[15] =
{
	e_weapons_datum_index::energy_blade_useless,
	e_weapons_datum_index::needler,
	e_weapons_datum_index::plasma_pistol,
	e_weapons_datum_index::magnum,
	e_weapons_datum_index::smg,
	e_weapons_datum_index::plasma_rifle,
	e_weapons_datum_index::brute_plasma_rifle,
	e_weapons_datum_index::juggernaut_powerup,
	e_weapons_datum_index::shotgun,
	e_weapons_datum_index::brute_shot,
	e_weapons_datum_index::covenant_carbine,
	e_weapons_datum_index::battle_rifle,
	e_weapons_datum_index::beam_rifle,
	e_weapons_datum_index::sniper_rifle,
	e_weapons_datum_index::rocket_launcher
};

std::unordered_map<uint64, int> GunGame::gungamePlayers;

GunGame::GunGame()
{
}

void GunGame::ResetPlayerLevels() {
	gungamePlayers.clear();
}

void GunGame::Initialize()
{
	event(_event_status, "h2mod:gungame: Peer host init");
	if (NetworkSession::LocalPeerIsSessionHost())
	{
		GunGame::ResetPlayerLevels();
	}
}

void GunGame::Dispose()
{
	ResetPlayerLevels();
}

CustomVariantId GunGame::GetVariantId()
{
	return CustomVariantId::_id_gungame;
}

void GunGame::OnMapLoad(ExecTime execTime, s_game_options* gameOptions)
{
	switch (execTime)
	{
	case ExecTime::_preEventExec:
		break;

	case ExecTime::_postEventExec:
		switch (gameOptions->game_mode)
		{
			// cleanup when loading main menu
		case _game_mode_multiplayer:
			this->Initialize();
			break;
		/*case _main_menu:
			this->Dispose();
			break;*/
		default:
			break;
		}

		break;

	case ExecTime::_ExecTimeUnknown:
	default:
		event(_event_verbose, "h2mod:gungame: %s - unknown execTime", __FUNCTION__);
		break;
	}
}

void GunGame::OnPlayerDeath(ExecTime execTime, datum playerIdx)
{
	switch (execTime)
	{
	case ExecTime::_preEventExec:
		// to note after the original function executes, the controlled unit by this player is set to NONE
		if (!game_is_predicted())
		{
			s_player::set_player_unit_grenade_count(playerIdx, _unit_grenade_human_fragmentation, 0, true);
			s_player::set_player_unit_grenade_count(playerIdx, _unit_grenade_covenant_plasma, 0, true);
		}
		break;

	case ExecTime::_postEventExec:
		break;

	case ExecTime::_ExecTimeUnknown:
	default:
		event(_event_verbose, "h2mod:gungame: %s - unknown execTime", __FUNCTION__);
		break;
	}
}

void GunGame::OnPlayerSpawn(ExecTime execTime, datum playerIdx)
{
	int absPlayerIdx = DATUM_INDEX_TO_ABSOLUTE_INDEX(playerIdx);
	datum playerUnitDatum = s_player::get_unit_index(playerIdx);

	switch (execTime)
	{
		// prespawn handler
	case ExecTime::_preEventExec:
		s_player::set_unit_character_type(playerIdx, _character_type_spartan);
		break;

		// postspawn handler
	case ExecTime::_postEventExec:
		// host only (dedicated server and client)
		if (!game_is_predicted())
		{
			event(_event_verbose, "h2mod:gungame: %s player index: %d, player name: %ws", __FUNCTION__, absPlayerIdx, s_player::get_name(playerIdx));

			void* unit_object = object_try_and_get_and_verify_type(playerUnitDatum, _object_mask_biped);
			if (unit_object) {

				int level = 0;
				auto gungamePlayer = gungamePlayers.find(NetworkSession::GetPlayerId(absPlayerIdx));
				if (gungamePlayer != gungamePlayers.end())
				{
					level = gungamePlayer->second;
				}
				else
				{
					gungamePlayers.insert(std::make_pair(NetworkSession::GetPlayerId(absPlayerIdx), level));
				}

				event(_event_verbose, "h2mod:gungame: %s - player index: %d, player name: %ws - Level: %d", __FUNCTIONW__, absPlayerIdx, s_player::get_name(playerIdx), level);


				if (level == 15)
				{
					event(_event_verbose, "h2mod:gungame: %s - %ws on frag grenade level!", __FUNCTION__, s_player::get_name(playerIdx));
					s_player::set_player_unit_grenade_count(playerIdx, _unit_grenade_human_fragmentation, 99, true);
				}
				else if (level == 16)
				{
					event(_event_verbose, "h2mod:gungame: %s - %ws on plasma grenade level!", __FUNCTION__, s_player::get_name(playerIdx));
					s_player::set_player_unit_grenade_count(playerIdx, _unit_grenade_covenant_plasma, 99, true);
				}
				else
				{
					call_give_player_weapon(absPlayerIdx, (datum)k_level_weapons[level], 1);
				}
			}
		}

		break;

	case ExecTime::_ExecTimeUnknown:
	default:
		event(_event_verbose, "h2mod:gungame: %s - unknown execTime", __FUNCTION__);
		break;
	}
}

bool GunGame::c_game_statborg__adjust_player_stat(ExecTime execTime, c_game_statborg* statborg, datum player_datum, e_statborg_entry statistic, short count, int game_results_statistic, bool adjust_team_stat)
{
	int absPlayerIdx = DATUM_INDEX_TO_ABSOLUTE_INDEX(player_datum);
	uint64 playerId = NetworkSession::GetPlayerId(absPlayerIdx);

	// in gungame we just keep track of the score
	bool handled = false;

	switch (execTime)
	{
	case ExecTime::_preEventExec:
		break;
		
	case ExecTime::_postEventExec:
		if (game_results_statistic == 7
			&& !game_is_predicted())
		{
			event(_event_verbose, "h2mod:gungame: %s - player index: %d, player name: %ws", __FUNCTION__, absPlayerIdx, s_player::get_name(player_datum));

			int32 level = GunGame::gungamePlayers[playerId];
			++level;

			if (level > 16)
			{
				level = 0; // reset level, so we dont keep the player without weapons, in case the game doesnt end
			}

			GunGame::gungamePlayers[playerId] = level;

			event(_event_verbose, "h2mod:gungame: %s - player index: %d - new level: %d ", __FUNCTION__, absPlayerIdx, level);

			if (level == 15)
			{
				event(_event_verbose, "h2mod:gungame: %s - %ws Level 15 - Frag Grenades!", __FUNCTION__, s_player::get_name(player_datum));
				s_player::set_player_unit_grenade_count(player_datum, _unit_grenade_human_fragmentation, 99, true);
			}
			else if (level == 16)
			{
				event(_event_verbose, "h2mod:gungame: %s - %ws Level 16 - Plasma Grenades!", __FUNCTION__, s_player::get_name(player_datum));
				s_player::set_player_unit_grenade_count(player_datum, _unit_grenade_human_fragmentation, 0, true);
				s_player::set_player_unit_grenade_count(player_datum, _unit_grenade_covenant_plasma, 99, true);
			}
			else
			{
				event(_event_verbose, "h2mod:gungame: %s - %ws on level %d giving them weapon...", __FUNCTION__, s_player::get_name(player_datum), level);
				s_player::set_player_unit_grenade_count(player_datum, _unit_grenade_human_fragmentation, 0, true);
				s_player::set_player_unit_grenade_count(player_datum, _unit_grenade_covenant_plasma, 0, true);
				call_give_player_weapon(absPlayerIdx, (datum)k_level_weapons[level], 1);
			}
		}

		break;

	case ExecTime::_ExecTimeUnknown:
	default:
		event(_event_verbose, "h2mod:gungame: %s - unknown execTime", __FUNCTION__);
		break;
	}

	return handled;
}
