#pragma once
#include "saved_games/game_variant.h"

#define PLAYLIST_ITEM_COLLECTION(_name, _expected_count, ...)          \
    static s_halo_playlist_item _name##_items[] = { __VA_ARGS__ };                    \
    static_assert(ARRAYSIZE(_name##_items) == (_expected_count),                      \
        #_name " out of sync with enum");                                             \
    s_halo_playlist_item_collection _name = { _name##_items, ARRAYSIZE(_name##_items) }

/* structures */

struct s_halo_playlist_item
{
	const wchar_t* name;
	int32 value;
};
ASSERT_STRUCT_SIZE(s_halo_playlist_item, 8);

struct s_halo_playlist_item_collection
{
	s_halo_playlist_item* items;
	uint32 count;
};
ASSERT_STRUCT_SIZE(s_halo_playlist_item_collection, 8);

/* prototypes */

const wchar_t* halo_playlist_item_collection_get_name(s_halo_playlist_item_collection* collection, int32 value);

int32 halo_playlist_item_collection_get_value(s_halo_playlist_item_collection* collection, wchar_t const* value);

bool halo_playlist_item_collection_get_boolean_value(wchar_t const* value, bool* out_result);

// range 1-600 allows second and minute specifier
// used for score, respawn times, reset times, etc
int32 halo_playlist_item_collection_get_int_time_value(wchar_t const* value);

// range 1-1000
int32 halo_playlist_item_collection_get_int_value(wchar_t const* value);

// range 1-16
int16 halo_playlist_item_collection_player_count_get_value(wchar_t const* value);
