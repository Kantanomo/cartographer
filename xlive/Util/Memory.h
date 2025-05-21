#pragma once

#include "cseries/cseries.h"
#include "cseries/cseries_windows.h"

#define BASE_IMAGE_ADDRESS_HALO2 0x00400000
#define BASE_IMAGE_ADDRESS_H2SERVER 0x00400000

class Memory
{
public:
	static void Initialize();

	static DWORD GetAddress()
	{
		return GetBaseAddress();
	}

	static DWORD GetAddress(DWORD client)
	{
		ASSERT(client != 0);
		return GetBaseAddress() + client;
	}

	static DWORD GetAddress(DWORD client, DWORD server)
	{
		ASSERT(g_memory_is_dedicated_server || client != 0);
		ASSERT(!g_memory_is_dedicated_server || server != 0);
		
	// NOTE:
	// if server or client is equal to 0 then we pass the same address for both (ONLY ON RELEASE BUILDS)
	// REASON:
	// optimizes out a cmov or related instruction when building release whenever we call a function that doesn't have a dedi address specified
#ifdef NDEBUG
		const uintptr_t address = 
			(g_memory_is_dedicated_server ? 
				(server) != 0 ? server : client : 
				(client) != 0 ? client : server
			);
#else
		const uintptr_t address = (g_memory_is_dedicated_server ? server : client);
#endif
		return GetBaseAddress() + address;
	}
	
	template <typename T = void*>
	static T GetAddress(DWORD client)
	{
		return reinterpret_cast<T>((DWORD)GetAddress(client));
	}

	template <typename T = void*>
	static T GetAddress(DWORD client, DWORD server)
	{
		return reinterpret_cast<T>((DWORD)GetAddress(client, server));
	}

	static DWORD GetAddressRelative(DWORD client, DWORD server = 0)
	{
		return GetAddress(client - BASE_IMAGE_ADDRESS_HALO2, server - BASE_IMAGE_ADDRESS_H2SERVER);
	}

	template <typename T = void*>
	static T GetAddressRelative(DWORD client, DWORD server = 0)
	{
		return reinterpret_cast<T>((DWORD)GetAddress(client - BASE_IMAGE_ADDRESS_HALO2, server - BASE_IMAGE_ADDRESS_H2SERVER));
	}

	static void SetBaseAddress(DWORD base, bool isDedicatedServer)
	{
		baseAddress = base;
		g_memory_is_dedicated_server = isDedicatedServer;
	}

	// gets base address
	static DWORD GetBaseAddress() { return baseAddress; }

	static DWORD baseAddress;
	static bool g_memory_is_dedicated_server;
};

