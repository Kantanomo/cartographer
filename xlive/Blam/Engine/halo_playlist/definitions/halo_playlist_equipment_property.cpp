#include "stdafx.h"
#include "halo_playlist_equipment_property.h"

//PLAYLIST_ITEM_COLLECTION(g_halo_playlist_equipment_property_item_collection,
//    k_halo_playlist_equipment_property_item_count,
//    { L"Starting Weapon",     _halo_playlist_equipment_property_starting_weapon },
//    { L"Secondary Weapon",    _halo_playlist_equipment_property_secondary_weapon },
//    { L"Starting Grenades",   _halo_playlist_equipment_property_starting_grenades },
//    { L"Weapons on Map",      _halo_playlist_equipment_property_weapons_on_map },
//    { L"Weapon Respawn Time", _halo_playlist_equipment_property_weapon_respawn_time },
//    { L"Grenades on Map",     _halo_playlist_equipment_property_grenades_on_map },
//    { L"Overshields",         _halo_playlist_equipment_property_over_shields_on_map },
//    { L"Active Camo on Map",  _halo_playlist_equipment_property_active_camo_on_map }
//);

e_halo_playlist_equipment_property halo_playlist_item_collection_equipment_property_get_value(wchar_t* value)
{
	s_halo_playlist_item_collection* g_halo_playlist_equipment_property_item_collection = Memory::GetAddress<s_halo_playlist_item_collection*>(0, 0x355B30);
	return (e_halo_playlist_equipment_property)halo_playlist_item_collection_get_value(g_halo_playlist_equipment_property_item_collection, value);
}