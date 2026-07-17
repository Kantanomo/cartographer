#include "stdafx.h"
#include "halo_playlist_match_property.h"

//PLAYLIST_ITEM_COLLECTION(g_halo_playlist_match_property_collection, k_halo_playlist_match_property_count,
//	{ L"Variant",         _halo_playlist_match_property_type_variant },
//	{ L"Map",             _halo_playlist_match_property_type_map },
//	{ L"Weight",          _halo_playlist_match_property_type_weight },
//	{ L"Minimum Players", _halo_playlist_match_property_type_minimum_players },
//	{ L"Maximum Players", _halo_playlist_match_property_type_maximum_players }
//);

e_halo_playlist_match_property_type halo_playlist_item_collection_get_match_property_get_value(wchar_t* value)
{
	s_halo_playlist_item_collection* g_halo_playlist_match_property_collection = Memory::GetAddress<s_halo_playlist_item_collection*>(0, 0x355260);
	return INVOKE_TYPE(0, 0xF3BD, e_halo_playlist_match_property_type(*)(s_halo_playlist_item_collection*, wchar_t*), g_halo_playlist_match_property_collection, value);
}

wchar_t* halo_playlist_item_collection_match_property_get_name(e_halo_playlist_match_property_type value)
{
	ASSERT(IN_RANGE(value, -1, k_halo_playlist_match_property_type_count));

	s_halo_playlist_item_collection* g_halo_playlist_match_property_collection = Memory::GetAddress<s_halo_playlist_item_collection*>(0, 0x355260);

	return halo_playlist_item_collection_get_name(g_halo_playlist_match_property_collection, value);
}