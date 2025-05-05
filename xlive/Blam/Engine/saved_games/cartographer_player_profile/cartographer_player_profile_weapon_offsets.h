#pragma once

/* enums */

enum e_weapon_offset_weapon : int8
{
	_weapon_offset_weapon_battle_rifle,
	_weapon_offset_weapon_beam_rifle,
	_weapon_offset_weapon_brute_plasma_rifle,
	_weapon_offset_weapon_brute_shot,
	_weapon_offset_weapon_covenant_carbine,
	_weapon_offset_weapon_energy_blade,
	_weapon_offset_weapon_flak_cannon,
	_weapon_offset_weapon_magnum,
	_weapon_offset_weapon_needler,
	_weapon_offset_weapon_plasma_pistol,
	_weapon_offset_weapon_plasma_rifle,
	_weapon_offset_weapon_rocket_launcher,
	_weapon_offset_weapon_sentinel_aggressor_beam,
	_weapon_offset_weapon_shotgun,
	_weapon_offset_weapon_smg,
	_weapon_offset_weapon_sniper_rifle,

	k_weapon_offset_weapon_count,
	_weapon_offset_weapon_invalid = NONE
};

/* structures */

struct s_weapon_custom_offset_constant_data
{
	const real_point3d default_offset;
	const char* const weapon_path;
};

/* constants */

extern const s_weapon_custom_offset_constant_data k_weapon_offset_constant_data[k_weapon_offset_weapon_count];

/* public code */

void cartographer_player_profile_weapon_offsets_on_map_load(void);

e_weapon_offset_weapon cartographer_player_profile_weapon_offsets_get_weapon_index_from_tag_index(datum weapon_index);
