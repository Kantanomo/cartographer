#include "stdafx.h"
#include "halo_playlist_oddball_property.h"

//PLAYLIST_ITEM_COLLECTION(g_halo_playlist_oddball_property_item_collection,
//    k_halo_playlist_oddball_property_item_count,
//    { L"Score to Win Round",    _halo_playlist_oddball_property_score_to_win_round },
//    { L"Team Play",             _halo_playlist_oddball_property_team_play },
//    { L"Team Scoring",          _halo_playlist_oddball_property_team_scoring },
//    { L"Team Changing",         _halo_playlist_oddball_property_team_changing },
//    { L"Force Even Teams",      _halo_playlist_oddball_property_force_even_teams },
//    { L"Ball Count",            _halo_playlist_oddball_property_ball_count },
//    { L"Ball Hit Damage",       _halo_playlist_oddball_property_ball_hit_damage },
//    { L"Speed With Ball",       _halo_playlist_oddball_property_speed_with_ball },
//    { L"Toughness With Ball",   _halo_playlist_oddball_property_toughness_with_ball },
//    { L"Active Camo With Ball", _halo_playlist_oddball_property_active_camo_with_ball },
//    { L"Vehicle Operation",     _halo_playlist_oddball_property_vehicle_operation },
//    { L"Ball Indicator",        _halo_playlist_oddball_property_ball_indicator }
//);

e_halo_playlist_oddball_property halo_playlist_item_collection_oddball_property_get_value(wchar_t* value)
{
	s_halo_playlist_item_collection* g_halo_playlist_oddball_property_item_collection = Memory::GetAddress<s_halo_playlist_item_collection*>(0, 0x355E60);
	return (e_halo_playlist_oddball_property)halo_playlist_item_collection_get_value(g_halo_playlist_oddball_property_item_collection, value);
}