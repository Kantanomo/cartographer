#pragma once

/* classes */

class c_kablam_config
{
	int32 field_0; // Unused
	wchar_t m_playlist[MAX_PATH];
	wchar_t m_stats_folder[MAX_PATH];
	int32 m_maximum_players;
	wchar_t m_server_administrators_group[MAX_PATH];
	wchar_t m_custom_maps_folder[MAX_PATH];
	wchar_t m_playlist_folder[MAX_PATH];
};

/* prototypes */

void __cdecl kablam_config_initialize(HKEY key, LPCWSTR subkey);

void kablam_config_apply_patches(void);
