#include "stdafx.h"
#include "cartographer_player_profile_weapon_offsets.h"

#include "cache/cache_files.h"

/* constants */

const s_weapon_custom_offset_constant_data k_weapon_offset_constant_data[k_weapon_offset_weapon_count]
{
	{{{0.02f, 0.f, -0.004f}},		"objects\\weapons\\rifle\\battle_rifle\\battle_rifle"},
	{{{0.02f, 0.f, 0.f}},			"objects\\weapons\\rifle\\beam_rifle\\beam_rifle"},
	{{{0.02f, 0.f, 0.f}},			"objects\\weapons\\rifle\\brute_plasma_rifle\\brute_plasma_rifle"},
	{{{0.02f, -0.01f, 0.001f}},		"objects\\weapons\\support_low\\brute_shot\\brute_shot"},
	{{{0.02f, 0.f, -0.004f}},		"objects\\weapons\\rifle\\covenant_carbine\\covenant_carbine"},
	{{{0.05f, 0.f, 0.f}},			"objects\\weapons\\melee\\energy_blade\\energy_blade"},
	{{{0.02f, 0.f, -0.005f}},		"objects\\weapons\\support_high\\flak_cannon\\flak_cannon"},
	{{{0.03f, 0.f, -0.005f}},		"objects\\weapons\\pistol\\magnum\\magnum"},
	{{{0.03f, 0.f, -0.005f}},		"objects\\weapons\\pistol\\needler\\needler"},
	{{{0.03f, 0.f, -0.005f}},		"objects\\weapons\\pistol\\plasma_pistol\\plasma_pistol"},
	{{{0.02f, 0.f, 0.f}},			"objects\\weapons\\rifle\\plasma_rifle\\plasma_rifle"},
	{{{0.02f, 0.f, -0.005f}},		"objects\\weapons\\support_high\\rocket_launcher\\rocket_launcher"},
	{{{0.02f, 0.03f, 0.f}},			"objects\\characters\\sentinel_aggressor\\weapons\\beam\\sentinel_aggressor_beam"},
	{{{0.02f, -0.001f, -0.015f}},	"objects\\weapons\\rifle\\shotgun\\shotgun"},
	{{{0.02f, 0.f, 0.f}},			"objects\\weapons\\rifle\\smg\\smg"},
	{{{0.01f, 0.f, 0.f}},			"objects\\weapons\\rifle\\sniper_rifle\\sniper_rifle"}
};

/* globals */

static datum g_weapon_offset_tag_indices[k_weapon_offset_weapon_count] = 
{
	NONE,
	NONE,
	NONE,
	NONE,
	NONE,
	NONE,
	NONE,
	NONE,
	NONE,
	NONE,
	NONE,
	NONE,
	NONE,
	NONE,
	NONE,
	NONE
};

/* prototypes */

/* public code */

void cartographer_player_profile_weapon_offsets_on_map_load(void)
{
	// Update all of the weapon tag indicies to the updated ones on map load
	for (size_t weapon_offset_index = 0; weapon_offset_index < k_weapon_offset_weapon_count; ++weapon_offset_index)
	{
		g_weapon_offset_tag_indices[weapon_offset_index] = tag_loaded(_tag_group_weapon, k_weapon_offset_constant_data[weapon_offset_index].weapon_path);
	}

	return;
}

e_weapon_offset_weapon cartographer_player_profile_weapon_offsets_get_weapon_index_from_tag_index(datum weapon_index)
{
	ASSERT(weapon_index != NONE);
	e_weapon_offset_weapon result = _weapon_offset_weapon_invalid;
	for (int8 weapon_offset_index = 0; weapon_offset_index < k_weapon_offset_weapon_count; ++weapon_offset_index)
	{
		if (g_weapon_offset_tag_indices[weapon_offset_index] == weapon_index)
		{
			result = (e_weapon_offset_weapon)weapon_offset_index;
			break;
		}
	}

	return result;
}


/* private code */
