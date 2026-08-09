#include "stdafx.h"
#include "halo_playlist_item_collection.h"

/* public code */

const wchar_t* halo_playlist_item_collection_get_name(s_halo_playlist_item_collection* collection, int32 value)
{
	if (collection->count <= 0)
		return L"";

	for (uint32 i = 0; i < collection->count; ++i)
		if (collection->items[i].value == value)
			return collection->items[i].name;

	return L"";
}

int32 halo_playlist_item_collection_get_value(s_halo_playlist_item_collection* collection, wchar_t const* value)
{
	return INVOKE_TYPE(0, 0xF1E9, int32(*)(s_halo_playlist_item_collection*, wchar_t const*), collection, value);
}

bool halo_playlist_item_collection_get_boolean_value(wchar_t const* value, bool* out_result)
{
	bool return_value = false;
	bool parse_successful = true;

	switch (*value)
	{
		case L'0':
		case L'F':
		case L'N':
		case L'f':
		case L'n':
		{
			return_value = false;
			break;
		}
		case L'1':
		case L'T':
		case L'Y':
		case L't':
		case L'y':
		{
			return_value = true;
			break;
		}
		default:
		{
			if (_wcsicmp(value, L"on") == 0)
				return_value = true;
			else if (_wcsicmp(value, L"off") == 0)
				return_value = false;
			else
				parse_successful = false;

			break;
		}
	}

	if (parse_successful)
		*out_result = return_value;

	return parse_successful;
}

int32 halo_playlist_item_collection_get_int_time_value(wchar_t const* value)
{
	s_halo_playlist_item_collection* integer_item_collection = Memory::GetAddress<s_halo_playlist_item_collection*>(0, 0x355290);
	return halo_playlist_item_collection_get_value(integer_item_collection, value);
}

int32 halo_playlist_item_collection_get_int_value(wchar_t const* value)
{
	s_halo_playlist_item_collection* integer_item_collection = Memory::GetAddress<s_halo_playlist_item_collection*>(0, 0x355210);
	return halo_playlist_item_collection_get_value(integer_item_collection, value);
}

int16 halo_playlist_item_collection_player_count_get_value(wchar_t const* value)
{
	s_halo_playlist_item_collection* player_count_item_collection = Memory::GetAddress<s_halo_playlist_item_collection*>(0, 0x355230);
	return (int16)halo_playlist_item_collection_get_value(player_count_item_collection, value);
}
