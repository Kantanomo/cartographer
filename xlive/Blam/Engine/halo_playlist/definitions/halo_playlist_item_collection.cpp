#include "stdafx.h"
#include "halo_playlist_item_collection.h"

wchar_t* halo_playlist_item_collection_get_name(s_halo_playlist_item_collection* collection, int32 value)
{
	if (collection->count <= 0)
		return L"";

	for(int32 i = 0; i < collection->count; ++i)
		if (collection->items[i].value == value)
			return collection->items[i].name;
	
	return L"";
}
