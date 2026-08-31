#include "stdafx.h"
#include "halo_playlist_king_property.h"

/* globals */

//PLAYLIST_ITEM_COLLECTION(g_halo_playlist_king_property_item_collection, k_halo_playlist_king_property_item_count,
//    { L"Score to Win Round",         _halo_playlist_king_property_score_to_win },
//    { L"Team Play",                  _halo_playlist_king_property_team_play },
//    { L"Team Scoring",               _halo_playlist_king_property_team_scoring },
//    { L"Team Changing",              _halo_playlist_king_property_team_changing },
//    { L"Force Even Teams",           _halo_playlist_king_property_force_even_teams },
//    { L"Uncontested Hill",           _halo_playlist_king_property_uncontested_hill },
//    { L"Moving Hill",                _halo_playlist_king_property_moving_hill },
//    { L"Team Time Multiplier",       _halo_playlist_king_property_team_time_multiplier },
//    { L"Extra Damage on Hill",       _halo_playlist_king_property_extra_damage_on_hill },
//    { L"Damage Resistance on Hill",  _halo_playlist_king_property_damage_resistance_on_hill },
//    { L"Active Camo on Hill",        _halo_playlist_king_property_active_camo_on_hill }
//);

/* public code */

e_halo_playlist_king_property halo_playlist_item_collection_king_property_get_value(wchar_t const* value)
{
	s_halo_playlist_item_collection* g_halo_playlist_king_property_item_collection = Memory::GetAddress<s_halo_playlist_item_collection*>(0, 0x355CE8);
	return (e_halo_playlist_king_property)halo_playlist_item_collection_get_value(g_halo_playlist_king_property_item_collection, value);
}
