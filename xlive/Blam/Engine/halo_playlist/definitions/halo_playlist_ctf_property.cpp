#include "stdafx.h"
#include "halo_playlist_ctf_property.h"

/* globals */

//PLAYLIST_ITEM_COLLECTION(g_halo_playlist_ctf_property_item_collection, k_halo_playlist_ctf_property_item_count,
//    { L"Score to Win Round",          _halo_playlist_ctf_property_score_to_win_round },
//    { L"Team Changing",               _halo_playlist_ctf_property_team_changing },
//    { L"Force Even Teams",            _halo_playlist_ctf_property_force_even_teams },
//    { L"Flag Type",                   _halo_playlist_ctf_property_flag_type },
//    { L"Sudden Death",                _halo_playlist_ctf_property_sudden_death },
//    { L"Flag At Home To Score",       _halo_playlist_ctf_property_flag_at_home_to_score },
//    { L"Flag Touch Return",           _halo_playlist_ctf_property_flag_touch_return },
//    { L"Flag Reset Time",             _halo_playlist_ctf_property_flag_reset_time },
//    { L"Slow With Flag",              _halo_playlist_ctf_property_slow_with_flag },
//    { L"Flag Hit Damage",             _halo_playlist_ctf_property_flag_hit_damage },
//    { L"Damage Resistance With Flag", _halo_playlist_ctf_property_damage_resistance_with_flag },
//    { L"Active Camo With Flag",       _halo_playlist_ctf_property_active_camo_with_flag },
//    { L"Vehicle Operation",           _halo_playlist_ctf_property_vehicle_operation },
//    { L"Flag Indicator",              _halo_playlist_ctf_property_flag_indicator }
//);

/* public code */

e_halo_playlist_ctf_property halo_playlist_item_collection_ctf_property_get_value(wchar_t const* value)
{
	s_halo_playlist_item_collection* g_halo_playlist_ctf_property_item_collection = Memory::GetAddress<s_halo_playlist_item_collection*>(0, 0x3560C0);
	return (e_halo_playlist_ctf_property)halo_playlist_item_collection_get_value(g_halo_playlist_ctf_property_item_collection, value);
}