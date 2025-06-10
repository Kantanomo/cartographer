#pragma once

/* structures */

struct s_network_adapter
{
	char adapter_name[64];
	wchar_t friendly_name[64];
	wchar_t description[64];
};

// TODO: properly reverse this
struct s_network_configuration
{
	int8 gap_0[16];
	int8 gap_10[6044];
	uint32 adapter_count;
	uint8 network_adapters_available;
	int32 network_adapter_index;
	s_network_adapter network_adapters[16];
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
