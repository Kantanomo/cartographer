#include "stdafx.h"
#include "halo_playlist_game_type_property.h"

PLAYLIST_ITEM_COLLECTION(g_halo_playlist_game_type_property_item_collection, k_halo_playlist_game_type_item_count,
	{ L"Slayer",           _game_variant_description_slayer },
	{ L"King of the Hill", _game_variant_description_king },
	{ L"King",             _game_variant_description_king },
	{ L"KoTH",             _game_variant_description_king },
	{ L"Oddball",          _game_variant_description_oddball },
	{ L"Juggernaut",       _game_variant_description_juggernaut },
	{ L"Capture the Flag", _game_variant_description_ctf },
	{ L"CTF",              _game_variant_description_ctf },
	{ L"Assault",          _game_variant_description_invasion },
	{ L"Territories",      _game_variant_description_territories }
);

wchar_t* halo_playlist_item_collection_game_type_get_name(e_game_variant_description_index value)
{
	return halo_playlist_item_collection_get_name(&g_halo_playlist_game_type_property_item_collection, value);
}

e_game_variant_description_index halo_playlist_item_collection_game_type_get_value(wchar_t* value)
{
	return (e_game_variant_description_index)halo_playlist_item_collection_get_value(&g_halo_playlist_game_type_property_item_collection, value);
}