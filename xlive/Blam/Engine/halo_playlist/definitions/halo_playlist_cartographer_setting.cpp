#include "stdafx.h"
#include "halo_playlist_cartographer_setting.h"

PLAYLIST_ITEM_COLLECTION(g_halo_playlist_cartographer_setting_property_item_collection, k_halo_playlist_cartographer_setting_property_count,
    { L"Engine mode",                    _halo_playlist_cartographer_setting_engine_mode },
    { L"Infinite Ammo",                  _halo_playlist_cartographer_setting_infinite_ammo },
    { L"Infinite Grenades",              _halo_playlist_cartographer_setting_infinite_grenades },
    { L"Explosion Physics",              _halo_playlist_cartographer_setting_explosion_physics },
    { L"Force Default FoV",              _halo_playlist_cartographer_setting_default_fov },
    { L"Force Default Weapon Offset",    _halo_playlist_cartographer_setting_default_weapon_offsets },
    { L"Force Default Crosshair Offset", _halo_playlist_cartographer_setting_default_cross_hair_offset },
    { L"Game Speed",                     _halo_playlist_cartographer_setting_game_speed },
    { L"Gravity",                        _halo_playlist_cartographer_setting_gravity },
    { L"Spawn Protection",               _halo_playlist_cartographer_setting_spawn_protection },
	{ L"Disable Dub Shot",               _halo_playlist_cartographer_setting_disable_dub_shot}
);

PLAYLIST_ITEM_COLLECTION(g_halo_playlist_cartographer_engine_mode_item_collection, k_halo_playlist_cartographer_engine_mode_count,
    { L"Default", _halo_playlist_cartographer_setting_engine_mode_default },
	{ L"60",      _halo_playlist_cartographer_setting_engine_mode_default },
    { L"Vista",   _halo_playlist_cartographer_setting_engine_mode_default },
    { L"Legacy",  _halo_playlist_cartographer_setting_engine_mode_legacy },
    { L"30",      _halo_playlist_cartographer_setting_engine_mode_legacy },
    { L"Xbox",    _halo_playlist_cartographer_setting_engine_mode_legacy }
);

PLAYLIST_ITEM_COLLECTION(g_halo_playlist_cartographer_game_speed_item_collection, k_halo_playlist_cartographer_game_speed_count,
    { L"Default",   _game_speed_modifier_none },
    { L"50%",       _game_speed_modifier_half },
    { L"150%",      _game_speed_modifier_hundred_fifty },
    { L"200%",      _game_speed_modifier_double },
    { L"Ludicrous", _game_speed_modifier_ludicrous }
);

PLAYLIST_ITEM_COLLECTION(g_halo_playlist_cartographer_gravity_item_collection, k_halo_playlist_cartographer_gravity_count,
    { L"Default", _game_gravity_modifier_none },
    { L"25%",     _game_gravity_modifier_twenty_five_percent },
    { L"50%",     _game_gravity_modifier_fifty_percent },
    { L"75%",     _game_gravity_modifier_seventy_five_percent },
    { L"125%",    _game_gravity_modifier_hundred_twenty_five_percent },
    { L"150%",    _game_gravity_modifier_hundred_fifty_percent },
    { L"175%",    _game_gravity_modifier_hundred_seventy_five_percent },
    { L"200%",    _game_gravity_modifier_two_hundred }
);

PLAYLIST_ITEM_COLLECTION(g_halo_playlist_cartographer_spawn_protection_item_collection, k_halo_playlist_cartographer_spawn_protection_count,
    { L"Default",       _player_spawn_protection_timer_one_second },
    { L"0",             _player_spawn_protection_timer_none },
    { L"None",          _player_spawn_protection_timer_none },
    { L"3",             _player_spawn_protection_timer_three_seconds },
    { L"Three Seconds", _player_spawn_protection_timer_three_seconds },
    { L"5",             _player_spawn_protection_timer_five_seconds },
    { L"Five Seconds",  _player_spawn_protection_timer_five_seconds },
    { L"10",            _player_spawn_protection_timer_ten_seconds },
    { L"Ten Seconds",   _player_spawn_protection_timer_ten_seconds }
);

wchar_t* halo_playlist_item_collection_cartographer_setting_get_name(e_halo_playlist_cartographer_setting_property_type value)
{
    ASSERT(IN_RANGE(value, k_halo_playlist_cartographer_setting_invalid, k_halo_playlist_cartographer_setting_count));

    return halo_playlist_item_collection_get_name(&g_halo_playlist_cartographer_setting_property_item_collection, value);
}

e_halo_playlist_cartographer_setting_property_type halo_playlist_item_collection_cartographer_setting_get_value(wchar_t* value)
{
    return (e_halo_playlist_cartographer_setting_property_type)halo_playlist_item_collection_get_value(&g_halo_playlist_cartographer_setting_property_item_collection, value);
}

e_halo_playlist_cartographer_setting_engine_mode halo_playlist_item_collection_cartographer_engine_mode_get_value(wchar_t* value)
{
    return (e_halo_playlist_cartographer_setting_engine_mode)halo_playlist_item_collection_get_value(&g_halo_playlist_cartographer_engine_mode_item_collection, value);
}

e_game_speed_modifier halo_playlist_item_collection_cartographer_game_speed_get_value(wchar_t* value)
{
    return (e_game_speed_modifier)halo_playlist_item_collection_get_value(&g_halo_playlist_cartographer_game_speed_item_collection, value);
}

e_game_gravity_modifier halo_playlist_item_collection_cartographer_gravity_get_value(wchar_t* value)
{
    return (e_game_gravity_modifier)halo_playlist_item_collection_get_value(&g_halo_playlist_cartographer_gravity_item_collection, value);
}

e_player_spawn_protection_timer halo_playlist_item_collection_cartographer_spawn_protection_get_value(wchar_t* value)
{
    return (e_player_spawn_protection_timer)halo_playlist_item_collection_get_value(&g_halo_playlist_cartographer_spawn_protection_item_collection, value);
}

static bool halo_playlist_item_collection_cartographer_setting_flag_write_to_variant(wchar_t* value, s_game_variant* variant, e_cartographer_variant_flags flag)
{
    bool result = false;
    const bool eval_result = halo_playlist_item_collection_get_boolean_value(value, &result);

    if (!eval_result)
        return eval_result;

    variant->cartographer_settings.flags.set(flag, result);

    return eval_result;
}

bool halo_playlist_item_collection_cartographer_engine_mode_write_to_variant(wchar_t* value, s_game_variant* variant)
{
    e_halo_playlist_cartographer_setting_engine_mode engine_mode = halo_playlist_item_collection_cartographer_engine_mode_get_value(value);

    if (engine_mode == k_halo_playlist_cartographer_setting_engine_mode_invalid)
        return false;

    variant->cartographer_settings.flags.set(_cartographer_variant_engine_mode, engine_mode);

    return true;
}

bool halo_playlist_item_collection_cartographer_infinite_ammo_write_to_variant(wchar_t* value, s_game_variant* variant)
{
    return halo_playlist_item_collection_cartographer_setting_flag_write_to_variant(value, variant, _cartographer_variant_infinite_ammo);
}

bool halo_playlist_item_collection_cartographer_infinite_grenades_write_to_variant(wchar_t* value, s_game_variant* variant)
{
    return halo_playlist_item_collection_cartographer_setting_flag_write_to_variant(value, variant, _cartographer_variant_infinite_grenades);
}

bool halo_playlist_item_collection_cartographer_explosion_physics_write_to_variant(wchar_t* value, s_game_variant* variant)
{
    return halo_playlist_item_collection_cartographer_setting_flag_write_to_variant(value, variant, _cartographer_variant_explosion_physics);
}

bool halo_playlist_item_collection_cartographer_default_fov_write_to_variant(wchar_t* value, s_game_variant* variant)
{
    return halo_playlist_item_collection_cartographer_setting_flag_write_to_variant(value, variant, _cartographer_variant_force_default_fov);
}

bool halo_playlist_item_collection_cartographer_default_weapon_offsets_write_to_variant(wchar_t* value, s_game_variant* variant)
{
    return halo_playlist_item_collection_cartographer_setting_flag_write_to_variant(value, variant, _cartographer_variant_force_default_weapon_offsets);
}

bool halo_playlist_item_collection_cartographer_default_crosshair_position_write_to_variant(wchar_t* value, s_game_variant* variant)
{
    return halo_playlist_item_collection_cartographer_setting_flag_write_to_variant(value, variant, _cartographer_variant_force_default_cross_hair_offset);
}

bool halo_playlist_item_collection_cartographer_disable_dub_shot_write_to_variant(wchar_t* value, s_game_variant* variant)
{
    return halo_playlist_item_collection_cartographer_setting_flag_write_to_variant(value, variant, _cartographer_variant_disable_dub_shot);
}

bool halo_playlist_item_collection_cartographer_game_speed_write_to_variant(wchar_t* value, s_game_variant* variant)
{
    e_game_speed_modifier game_speed = halo_playlist_item_collection_cartographer_game_speed_get_value(value);

    if (game_speed == k_game_speed_modifier_invalid)
        return false;

    variant->cartographer_settings.game_speed = game_speed;

    return true;
}

bool halo_playlist_item_collection_cartographer_gravity_write_to_variant(wchar_t* value, s_game_variant* variant)
{
    e_game_gravity_modifier gravity = halo_playlist_item_collection_cartographer_gravity_get_value(value);

    if (gravity == k_game_gravity_modifier_invalid)
        return false;

    variant->cartographer_settings.gravity = gravity;

    return true;
}

bool halo_playlist_item_collection_cartographer_spawn_protection_write_to_variant(wchar_t* value, s_game_variant* variant)
{
    e_player_spawn_protection_timer spawn_protection = halo_playlist_item_collection_cartographer_spawn_protection_get_value(value);

    if (spawn_protection == k_player_spawn_protection_timer_invalid)
        return false;

    variant->cartographer_settings.spawn_protection = spawn_protection;

    return true;
}