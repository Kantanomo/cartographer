#pragma once
#include "halo_playlist_item_collection.h"

/* constants */

enum
{
	k_halo_playlist_juggernaut_property_item_count = 9
};

/* enums */

enum e_halo_playlist_juggernaut_property
{
	_halo_playlist_juggernaut_property_score_to_win_round,
	_halo_playlist_juggernaut_property_betrayal_point_loss,
	_halo_playlist_juggernaut_property_juggernaut_extra_damage,
	_halo_playlist_juggernaut_property_juggernaut_infinite_ammo,
	_halo_playlist_juggernaut_property_juggernaut_overshield,
	_halo_playlist_juggernaut_property_juggernaut_active_camo,
	_halo_playlist_juggernaut_property_juggernaut_motion_sensor,
	_halo_playlist_juggernaut_property_juggernaut_movement,
	_halo_playlist_juggernaut_property_juggernaut_damage_resistance,

	k_halo_playlist_juggernaut_property_count,
	k_halo_playlist_juggernaut_property_invalid = NONE
};

/* prototypes */

e_halo_playlist_juggernaut_property halo_playlist_item_collection_juggernaut_property_get_value(wchar_t const* value);
