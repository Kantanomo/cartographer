#pragma once
#include "halo_playlist_item_collection.h"

/* constants */

enum
{
	k_halo_playlist_playlist_property_item_count = 5
};

/* enums */

enum e_halo_playlist_playlist_property_type
{
	_halo_playlist_playlist_property_shuffle,
	_halo_playlist_playlist_property_pregame_selection_delay,
	_halo_playlist_playlist_property_pregame_delay,
	_halo_playlist_playlist_property_postgame_delay,

	k_halo_playlist_playlist_property_count,
	k_halo_playlist_playlist_property_invalid = NONE
};

/* prototypes */

e_halo_playlist_playlist_property_type halo_playlist_item_collection_playlist_property_get_value(wchar_t const* value);
