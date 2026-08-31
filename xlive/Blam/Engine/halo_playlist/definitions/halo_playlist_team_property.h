#pragma once
#include "halo_playlist_item_collection.h"

/* constants */

enum
{
	k_halo_playlist_team_property_item_count = 3
};

/* enums */

enum e_halo_playlist_team_property
{
	_halo_playlist_team_property_friendly_fire = 2,
	_halo_playlist_team_property_respawn_time_modifier,
	_halo_playlist_team_property_betrayal_penalty,

	k_halo_playlist_team_property_count = 3,
	k_halo_playlist_team_property_invalid = NONE
};

/* prototypes */

e_halo_playlist_team_property halo_playlist_item_collection_team_property_get_value(wchar_t const* value);
