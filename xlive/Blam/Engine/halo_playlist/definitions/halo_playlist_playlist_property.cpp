#include "stdafx.h"
#include "halo_playlist_playlist_property.h"

//PLAYLIST_ITEM_COLLECTION(g_halo_playlist_playlist_property_item_collection, k_halo_playlist_playlist_property_item_count,
//    { L"Shuffle",                      _halo_playlist_playlist_property_shuffle },
//    { L"Pregame Team Selection Delay", _halo_playlist_playlist_property_pregame_selection_delay },
//    { L"Team Selection Delay",         _halo_playlist_playlist_property_pregame_selection_delay },
//    { L"Pregame Delay",                _halo_playlist_playlist_property_pregame_delay },
//    { L"Postgame Delay",               _halo_playlist_playlist_property_postgame_delay }
//);

e_halo_playlist_playlist_property_type halo_playlist_item_collection_playlist_property_get_value(wchar_t* value)
{
	s_halo_playlist_item_collection* g_halo_playlist_playlist_property_item_collection = Memory::GetAddress<s_halo_playlist_item_collection*>(0, 0x3552C0);
	return (e_halo_playlist_playlist_property_type)halo_playlist_item_collection_get_value(g_halo_playlist_playlist_property_item_collection, value);
}