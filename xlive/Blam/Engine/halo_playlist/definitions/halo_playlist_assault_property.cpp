#include "stdafx.h"
#include "halo_playlist_assault_property.h"

//PLAYLIST_ITEM_COLLECTION(g_halo_playlist_assault_property_item_collection, k_halo_playlist_assault_property_item_count,
//    { L"Score to Win Round",           _halo_playlist_assault_property_score_to_win_round },
//    { L"Team Changing",                _halo_playlist_assault_property_team_changing },
//    { L"Force Even Teams",             _halo_playlist_assault_property_force_even_teams },
//    { L"Bomb Type",                    _halo_playlist_assault_property_bomb_type },
//    { L"Enemy Bomb Indicator",         _halo_playlist_assault_property_enemy_bomb_indicator },
//    { L"Sudden Death",                 _halo_playlist_assault_property_sudden_death },
//    { L"Bomb Touch Return",            _halo_playlist_assault_property_bomb_touch_return },
//    { L"Bomb Reset Time",              _halo_playlist_assault_property_bomb_reset_time },
//    { L"Bomb Arm Time",                _halo_playlist_assault_property_bomb_arm_time },
//    { L"Sticky Arming",                _halo_playlist_assault_property_sticky_arming },
//    { L"Slow With Bomb",               _halo_playlist_assault_property_slow_with_bomb },
//    { L"Bomb Hit Damage",              _halo_playlist_assault_property_bomb_hit_damage },
//    { L"Damage Resistance With Bomb",  _halo_playlist_assault_property_damage_resistance_with_bomb },
//    { L"Active Camo With Bomb",        _halo_playlist_assault_property_active_camo_with_bomb },
//    { L"Vehicle Operation",            _halo_playlist_assault_property_vehicle_operation }
//);

e_halo_playlist_assault_property halo_playlist_item_collection_assault_property_get_value(wchar_t* value)
{
	s_halo_playlist_item_collection* g_halo_playlist_assault_property_item_collection = Memory::GetAddress<s_halo_playlist_item_collection*>(0, 0x3562A0);
	return (e_halo_playlist_assault_property)halo_playlist_item_collection_get_value(g_halo_playlist_assault_property_item_collection, value);
}