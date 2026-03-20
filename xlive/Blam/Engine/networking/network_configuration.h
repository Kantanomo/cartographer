#pragma once

/* structures */

struct s_network_adapter
{
	char adapter_name[64];
	wchar_t friendly_name[64];
	wchar_t description[64];
};

struct s_network_adapter_configuration
{
	uint32 adapter_count;
	uint8 network_adapters_available;
	int32 network_adapter_index;
	s_network_adapter network_adapters[16];
};

// TODO: properly reverse this
struct s_network_configuration
{
	int8 gap_0[16];
	real32 field_10;
	int8 gap_14[3364];
	int32 game_results_update_interval_msec;
	real32 field_D3C;
	int32 field_D40;
	int32 client_join_timeout;
	int8 gap_D40[16];
	int32 max_join_attempts;
	int32 client_active_timeout;
	int8 gap_D60[2636];
	s_network_adapter_configuration network_adapter;
};
ASSERT_STRUCT_SIZE(s_network_configuration, 11192);

/* prototypes */

void network_configuration_apply_patches(void);

s_network_configuration* global_network_configuration_get(void);

int32 __cdecl network_adapter_index_get(void);

const char* __cdecl network_adapter_name_get(int32 network_adapter_index);

// initializes some interface that's used to download the network config from the bungie's website but just a stub in release
void __cdecl network_configuration_initialize(void);

void __cdecl get_network_adapters(void);
