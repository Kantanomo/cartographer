#pragma once

template<typename T>
struct s_halo_playlist_item
{
	wchar_t* name;
	T value;
};
ASSERT_STRUCT_SIZE(s_halo_playlist_item<int32>, 8);

template<typename T>
struct s_halo_playlist_item_collection
{
	s_halo_playlist_item<T>* items;
	uint32 count;
};
ASSERT_STRUCT_SIZE(s_halo_playlist_item_collection<int32>, 8);