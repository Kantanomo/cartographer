#include "stdafx.h"
#include "WeaponOffsetConfig.h"



/* constants */

const s_weapon_custom_offset_constant_data k_weapon_custom_offset_constant_data[16]
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

/* prototypes */

void weapon_offset_get_path(c_static_wchar_string<MAX_PATH>* path);

/* public code */

void ReadWeaponOffsetConfig(s_weapon_custom_offset* weaponOffsets, int count)
{
	c_static_wchar_string<MAX_PATH> path;
	weapon_offset_get_path(&path);

	FILE* file = ufopen(path.get_string(), L"r");
	if (file != NULL)
	{
		for (int i = 0; i < count; i++)
		{
			fscanf_s(file, "%f,%f,%f\n", &weaponOffsets[i].modified_offset.x, &weaponOffsets[i].modified_offset.y, &weaponOffsets[i].modified_offset.z);
		}
		ufclose(file);
	}
	return;
}

void SaveWeaponOffsetConfig(const s_weapon_custom_offset* customOffsets, int count, bool defaultOffsets)
{
	c_static_wchar_string<MAX_PATH> path;
	weapon_offset_get_path(&path);

	FILE* file = ufopen(path.get_string(), L"w");
	if (file != NULL)
	{
		for (int i = 0; i < count; i++)
		{
			const real_point3d *offset = defaultOffsets ? &k_weapon_custom_offset_constant_data[i].default_offset : &customOffsets[i].modified_offset;
			fprintf_s(file, "%.3f,%.3f,%.3f\n", offset->x, offset->y, offset->z);
		}
		ufclose(file);
	}
	return;
}

// only writes anything if file dosent already exist
void WriteDefaultFile(const s_weapon_custom_offset* weaponOffsets, int count)
{
	c_static_wchar_string<MAX_PATH> path;
	weapon_offset_get_path(&path);

	FILE* file = ufopen(path.get_string(), L"r");
	if (file == NULL)
	{
		SaveWeaponOffsetConfig(weaponOffsets, count, true);
	}
	else
	{
		fclose(file);
	}
	return;
}

/* private code */

void weapon_offset_get_path(c_static_wchar_string<MAX_PATH>* path)
{
	wchar_t localappdata[MAX_PATH];
	ugetenv(localappdata, NUMBEROF(localappdata), L"localappdata");

	path->set(localappdata);
	path->append(L"\\Microsoft\\Halo 2\\WeaponOffsets.cfg");
	return;
}