#pragma once

struct s_halo_playlist_item
{
	wchar_t* name;
	int32 value;
};
ASSERT_STRUCT_SIZE(s_halo_playlist_item, 8);

struct s_halo_playlist_item_collection
{
	s_halo_playlist_item* items;
	uint32 count;
};
ASSERT_STRUCT_SIZE(s_halo_playlist_item_collection, 8);

wchar_t* halo_playlist_item_collection_get_name(s_halo_playlist_item_collection* collection, int32 value);
