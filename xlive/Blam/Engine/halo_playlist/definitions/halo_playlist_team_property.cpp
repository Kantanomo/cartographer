#include "stdafx.h"
#include "halo_playlist_team_property.h"

//PLAYLIST_ITEM_COLLECTION(g_halo_playlist_team_property_item_collection, k_halo_playlist_team_property_item_count,
//    { L"Friendly Fire",         _halo_playlist_team_property_friendly_fire },
//    { L"Respawn Time Modifier", _halo_playlist_team_property_respawn_time_modifier },
//    { L"Betrayal Penalty",      _halo_playlist_team_property_betrayal_penalty }
//);

e_halo_playlist_team_property halo_playlist_item_collection_team_property_get_value(wchar_t* value)
{
	s_halo_playlist_item_collection* g_halo_playlist_team_property_item_collection = Memory::GetAddress<s_halo_playlist_item_collection*>(0, 0x3556A0);
	return (e_halo_playlist_team_property)halo_playlist_item_collection_get_value(g_halo_playlist_team_property_item_collection, value);
}