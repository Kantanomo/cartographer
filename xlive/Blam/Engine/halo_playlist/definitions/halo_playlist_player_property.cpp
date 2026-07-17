#include "stdafx.h"
#include "halo_playlist_player_property.h"

//PLAYLIST_ITEM_COLLECTION(g_halo_playlist_player_property_item_collection, k_halo_playlist_player_property_item_count,
//    { L"Max Active Players", _halo_playlist_player_property_max_active_players },
//    { L"Lives Per Round",    _halo_playlist_player_property_lives_per_round },
//    { L"Respawn Time",       _halo_playlist_player_property_respawn_time },
//    { L"Suicide Penalty",    _halo_playlist_player_property_suicide_penalty },
//    { L"Shield Type",        _halo_playlist_player_property_shield_type },
//    { L"Motion Sensor",      _halo_playlist_player_property_motion_sensor },
//    { L"Active Camo",        _halo_playlist_player_property_active_camo },
//    { L"Extra Damage",       _halo_playlist_player_property_extra_damage },
//    { L"Damage Resistance",  _halo_playlist_player_property_damage_resistance }
//);

e_halo_playlist_player_property halo_playlist_item_collection_player_property_get_value(wchar_t* value)
{
	s_halo_playlist_item_collection* g_halo_playlist_player_property_item_collection = Memory::GetAddress<s_halo_playlist_item_collection*>(0, 0x3555D0);
	return (e_halo_playlist_player_property)halo_playlist_item_collection_get_value(g_halo_playlist_player_property_item_collection, value);
}