#include "stdafx.h"
#include "halo_playlist_juggernaut_property.h"

//PLAYLIST_ITEM_COLLECTION(g_halo_playlist_juggernaut_property_item_collection, k_halo_playlist_juggernaut_property_item_count,
//    { L"Score to Win Round",           _halo_playlist_juggernaut_property_score_to_win_round },
//    { L"Betrayal Point Loss",          _halo_playlist_juggernaut_property_betrayal_point_loss },
//    { L"Juggernaut Extra Damage",      _halo_playlist_juggernaut_property_juggernaut_extra_damage },
//    { L"Juggernaut Infinite Ammo",     _halo_playlist_juggernaut_property_juggernaut_infinite_ammo },
//    { L"Juggernaut Overshield",        _halo_playlist_juggernaut_property_juggernaut_overshield },
//    { L"Juggernaut Active Camo",       _halo_playlist_juggernaut_property_juggernaut_active_camo },
//    { L"Juggernaut Motion Sensor",     _halo_playlist_juggernaut_property_juggernaut_motion_sensor },
//    { L"Juggernaut Movement",          _halo_playlist_juggernaut_property_juggernaut_movement },
//    { L"Juggernaut Damage Resistance", _halo_playlist_juggernaut_property_juggernaut_damage_resistance }
//);

e_halo_playlist_juggernaut_property halo_playlist_item_collection_juggernaut_property_get_value(wchar_t* value)
{
	s_halo_playlist_item_collection* g_halo_playlist_juggernaut_property_item_collection = Memory::GetAddress<s_halo_playlist_item_collection*>(0, 0x355F30);
	return (e_halo_playlist_juggernaut_property)halo_playlist_item_collection_get_value(g_halo_playlist_juggernaut_property_item_collection, value);
}