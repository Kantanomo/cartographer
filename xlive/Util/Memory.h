#pragma once

#include "cseries/cseries.h"
#include "cseries/cseries_windows.h"

#define BASE_IMAGE_ADDRESS_HALO2 0x00400000
#define BASE_IMAGE_ADDRESS_H2SERVER 0x00400000

class Memory
{
public:
	static void Initialize();

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
		
	// NOTE:
	// if server or client is equal to 0 then we pass the same address for both (ONLY ON RELEASE BUILDS)
	// REASON:
	// optimizes out a cmov or related instruction when building release whenever we call a function that doesn't have a dedi address specified
#ifdef NDEBUG
		// FIXME: Enabling this will break things, there's some form of corruption in the project that gets triggered when this optimization is used
		/*
		if (server == 0)
		{
			server = client;
		}
		else if (client == 0)
		{
			client = server;
		}
		*/
#endif
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
		return GetAddress(client - BASE_IMAGE_ADDRESS_HALO2, server - BASE_IMAGE_ADDRESS_H2SERVER);
	}

	template <typename T = void*>
	static T GetAddressRelative(uintptr_t client, uintptr_t server = 0)
	{
		return reinterpret_cast<T>((uintptr_t)GetAddress(client - BASE_IMAGE_ADDRESS_HALO2, server - BASE_IMAGE_ADDRESS_H2SERVER));
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

