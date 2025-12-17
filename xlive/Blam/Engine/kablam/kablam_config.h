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

class c_kablam_config_live
{
public:
	wchar_t m_live_id[256];
	wchar_t m_description[32];
	wchar_t m_owner[16];
	int32 m_privacy_type;
	bool m_enable_multiple_instances;
	int32 m_override_port;
	void* m_kablam_registry_key;
};

/* prototypes */

void __cdecl kablam_config_initialize(HKEY key, LPCWSTR subkey);

c_kablam_config_live* kablam_config_live_get();

void kablam_config_apply_patches(void);
