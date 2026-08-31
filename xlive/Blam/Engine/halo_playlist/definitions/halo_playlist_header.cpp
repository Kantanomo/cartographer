#include "stdafx.h"
#include "halo_playlist_header.h"

#include "halo_playlist_item_collection.h"

/* globals */

//PLAYLIST_ITEM_COLLECTION(g_playlist_header_collection, k_halo_playlist_header_item_count,
//    { L"Playlist",       _halo_playlist_header_playlist },
//    { L"Variant",        _halo_playlist_header_variant },
//    { L"Custom Game",    _halo_playlist_header_variant },
//    { L"Custom Variant", _halo_playlist_header_variant },
//    { L"Match",          _halo_playlist_header_match }
//);

/* public code */

e_halo_playlist_header_type halo_playlist_item_collection_get_header_type(wchar_t const* value)
{
	s_halo_playlist_item_collection* g_playlist_header_collection = Memory::GetAddress<s_halo_playlist_item_collection*>(0, 0x355200);

	return (e_halo_playlist_header_type)halo_playlist_item_collection_get_value(g_playlist_header_collection, value);
	//return INVOKE_TYPE(0, 0xEE41, e_halo_playlist_header_type(*)(s_halo_playlist_item_collection*, wchar_t*), g_playlist_header_collection, value);
}
