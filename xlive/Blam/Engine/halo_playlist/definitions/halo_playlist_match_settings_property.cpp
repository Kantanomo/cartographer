#include "stdafx.h"
#include "halo_playlist_match_settings_property.h"

/* globals */

//PLAYLIST_ITEM_COLLECTION(g_halo_playlist_match_settings_property_item_collection, k_halo_playlist_match_settings_property_item_count,
//    { L"Number of Rounds", _halo_playlist_match_settings_property_number_of_rounds },
//    { L"Round Time Limit", _halo_playlist_match_settings_property_round_time_limit },
//    { L"Rounds Reset Map", _halo_playlist_match_settings_property_rounds_reset_map },
//    { L"Resolve Ties",     _halo_playlist_match_settings_property_resolve_ties }
//);

/* public code */

e_halo_playlist_match_settings_property halo_playlist_item_collection_match_settings_property_get_value(wchar_t const* value)
{
	s_halo_playlist_item_collection* g_halo_playlist_match_settings_property_item_collection = Memory::GetAddress<s_halo_playlist_item_collection*>(0, 0x355448);
	return (e_halo_playlist_match_settings_property)halo_playlist_item_collection_get_value(g_halo_playlist_match_settings_property_item_collection, value);
}
