#pragma once
#include "halo_playlist_item_collection.h"

/* constants */

enum 
{
	k_halo_playlist_header_item_count = 5
};

/* enums */

enum e_halo_playlist_header_type
{
	_halo_playlist_header_playlist,
	_halo_playlist_header_variant,
	_halo_playlist_header_match,

	k_halo_playlist_header_count,

	_halo_playlist_header_none = NONE,
};

/* prototypes */

e_halo_playlist_header_type halo_playlist_item_collection_get_header_type(wchar_t const* value);
