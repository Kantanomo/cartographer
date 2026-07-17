#include "stdafx.h"
#include "halo_playlist_vehicle_property.h"

//PLAYLIST_ITEM_COLLECTION(g_halo_playlist_vehicle_property_item_collection, k_halo_playlist_vehicle_property_item_count,
//    { L"Vehicle Respawn Time",    _halo_playlist_vehicle_property_respawn_time },
//    { L"Primary Light Vehicle",   _halo_playlist_vehicle_property_primary_light_vehicle },
//    { L"Secondary Light Vehicle", _halo_playlist_vehicle_property_secondary_light_vehicle },
//    { L"Primary Heavy Vehicle",   _halo_playlist_vehicle_property_primary_heavy_vehicle },
//    { L"Banshee",                 _halo_playlist_vehicle_property_banshee },
//    { L"Primary Turret",          _halo_playlist_vehicle_property_primary_turret },
//    { L"Secondary Turret",        _halo_playlist_vehicle_property_secondary_turret }
//);

e_halo_playlist_vehicle_property halo_playlist_item_collection_vehicle_property_get_value(wchar_t* value)
{
	s_halo_playlist_item_collection* g_halo_playlist_vehicle_property_item_collection = Memory::GetAddress<s_halo_playlist_item_collection*>(0, 0x355890);
	return (e_halo_playlist_vehicle_property)halo_playlist_item_collection_get_value(g_halo_playlist_vehicle_property_item_collection, value);
}