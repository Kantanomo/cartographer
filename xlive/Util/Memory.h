#pragma once

#include "cseries/cseries.h"
#include "cseries/cseries_windows.h"

/* constants */

enum
{
	k_base_image_address_halo2 = 0x00400000,
	k_base_image_address_h2server = 0x00400000,
};

/* enums */

enum e_h2_type
{
	_h2_type_none = NONE,
	_h2_type_unsupported,
	_h2_type_game,
	_h2_type_server,

	_h2_type_ek_sapien,
	_h2_type_ek_tool,
	_h2_type_ek_guerilla
};

class Memory
{
public:
	static e_h2_type Initialize(void);

	static uintptr_t GetAddress()
	{
		return GetBaseAddress();
	}

	static uintptr_t GetAddress(uintptr_t client)
	{
		ASSERT(client != 0);
		return GetBaseAddress() + client;
	}

	static uintptr_t GetAddress(uintptr_t client, uintptr_t server)
	{
		ASSERT(g_memory_is_dedicated_server || client != 0);
		ASSERT(!g_memory_is_dedicated_server || server != 0);
		
		const uintptr_t address = g_memory_is_dedicated_server ? server : client;
		return GetBaseAddress() + address;
	}
	
	template <typename T = void*>
	static T GetAddress(uintptr_t client)
	{
		return reinterpret_cast<T>((uintptr_t)GetAddress(client));
	}

	template <typename T = void*>
	static T GetAddress(uintptr_t client, uintptr_t server)
	{
		return reinterpret_cast<T>((uintptr_t)GetAddress(client, server));
	}

	static uintptr_t GetAddressRelative(uintptr_t client, uintptr_t server = 0)
	{
		return GetAddress(client - k_base_image_address_halo2, server - k_base_image_address_h2server);
	}

	template <typename T = void*>
	static T GetAddressRelative(uintptr_t client, uintptr_t server = 0)
	{
		return reinterpret_cast<T>((uintptr_t)GetAddress(client - k_base_image_address_halo2, server - k_base_image_address_h2server));
	}

	static void SetBaseAddress(uintptr_t base, bool isDedicatedServer)
	{
		baseAddress = base;
		g_memory_is_dedicated_server = isDedicatedServer;
	}

	// gets base address
	static uintptr_t GetBaseAddress() { return baseAddress; }

	static uintptr_t baseAddress;
	static bool g_memory_is_dedicated_server;
};

