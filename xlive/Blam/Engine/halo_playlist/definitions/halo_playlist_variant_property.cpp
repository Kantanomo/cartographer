#include "stdafx.h"
#include "halo_playlist_variant_property.h"

//PLAYLIST_ITEM_COLLECTION(g_halo_playlist_variant_property_item_collection, k_halo_playlist_variant_property_item_count,
//    { L"Name",         _halo_playlist_variant_property_name },
//    { L"Base Variant", _halo_playlist_variant_property_base_variant },
//    { L"Game Type",    _halo_playlist_variant_property_game_type }
//);

e_halo_playlist_variant_property_type halo_playlist_item_collection_get_variant_property_type(wchar_t* value)
{
	s_halo_playlist_item_collection* g_halo_playlist_variant_property_item_collection = Memory::GetAddress<s_halo_playlist_item_collection*>(0, 0x355338);

	return (e_halo_playlist_variant_property_type)halo_playlist_item_collection_get_value(g_halo_playlist_variant_property_item_collection, value);
}