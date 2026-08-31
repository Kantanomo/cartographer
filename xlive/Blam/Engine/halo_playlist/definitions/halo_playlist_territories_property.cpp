#include "stdafx.h"
#include "halo_playlist_territories_property.h"

/* globals */

//PLAYLIST_ITEM_COLLECTION(g_halo_playlist_territories_property_item_collection, k_halo_playlist_territories_property_item_count,
//    { L"Score to Win Round", _halo_playlist_territories_property_score_to_win_round },
//    { L"Team Changing",      _halo_playlist_territories_property_team_changing },
//    { L"Force Even Teams",   _halo_playlist_territories_property_force_even_teams },
//    { L"Territory Count",    _halo_playlist_territories_property_territory_count },
//    { L"Contest Time",       _halo_playlist_territories_property_contest_time },
//    { L"Control Time",       _halo_playlist_territories_property_control_time }
//);

/* public code */

e_halo_playlist_territories_property halo_playlist_item_collection_territories_property_get_value(wchar_t const* value)
{
	s_halo_playlist_item_collection* g_halo_playlist_territories_property_item_collection = Memory::GetAddress<s_halo_playlist_item_collection*>(0, 0x356468);
	return (e_halo_playlist_territories_property)halo_playlist_item_collection_get_value(g_halo_playlist_territories_property_item_collection, value);
}
