#pragma once
#include "halo_playlist_base_types.h"

#define k_halo_playlist_header_item_count 5

//wchar_t* g_halo_playlist_header_strings[k_halo_playlist_header_item_count]
//{
//	L"Playlist",
//	L"Variant",
//	L"Custom Game",
//	L"Custom Variant",
//	L"Match"
//};

enum e_halo_playlist_header_type : int32
{
	_halo_playlist_header_playlist = 0,
	_halo_playlist_header_variant = 1,
	_halo_playlist_header_match = 2,

	k_halo_playlist_header_count,

	_halo_playlist_header_none = -1,
};

//s_halo_playlist_item<e_halo_playlist_header_type> g_playlist_header_items[k_halo_playlist_header_item_count]
//{
//	{g_halo_playlist_header_strings[0], _halo_playlist_header_playlist},
//	{g_halo_playlist_header_strings[1], _halo_playlist_header_variant},
//	{g_halo_playlist_header_strings[2], _halo_playlist_header_variant},
//	{g_halo_playlist_header_strings[3], _halo_playlist_header_variant},
//	{g_halo_playlist_header_strings[4], _halo_playlist_header_match}
//};
//
//s_halo_playlist_item_collection<e_halo_playlist_header_type> g_playlist_header_collection = { g_playlist_header_items, k_halo_playlist_header_item_count };
