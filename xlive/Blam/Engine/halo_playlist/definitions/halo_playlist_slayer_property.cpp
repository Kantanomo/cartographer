#include "stdafx.h"
#include "halo_playlist_slayer_property.h"

//PLAYLIST_ITEM_COLLECTION(g_halo_playlist_slayer_property_item_collection, k_halo_playlist_slayer_property_item_count,
//    { L"Score to Win Round", _halo_playlist_slayer_property_score_to_win_round },
//    { L"Team Play",          _halo_playlist_slayer_property_team_play },
//    { L"Team Scoring",       _halo_playlist_slayer_property_team_scoring },
//    { L"Team Changing",      _halo_playlist_slayer_property_team_changing },
//    { L"Force Even Teams",   _halo_playlist_slayer_property_force_even_teams },
//    { L"Bonus Points",       _halo_playlist_slayer_property_bonus_points },
//    { L"Suicide Point Loss", _halo_playlist_slayer_property_suicide_point_loss },
//    { L"Death Point Loss",   _halo_playlist_slayer_property_death_point_loss }
//);

e_halo_playlist_slayer_property halo_playlist_item_collection_slayer_property_get_value(wchar_t* value)
{
	s_halo_playlist_item_collection* g_halo_playlist_slayer_property_item_collection = Memory::GetAddress<s_halo_playlist_item_collection*>(0, 0x355BD8);
	return (e_halo_playlist_slayer_property)halo_playlist_item_collection_get_value(g_halo_playlist_slayer_property_item_collection, value);
}