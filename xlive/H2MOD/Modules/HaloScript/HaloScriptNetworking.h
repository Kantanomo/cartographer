#pragma once
#include "Blam/Engine/Game/HaloScript.h"
namespace HaloScriptNetworking
{
	extern e_hs_function hs_sync_table[];
	struct s_hs_sync_packet
	{
		e_hs_function type;
		size_t data_size;
		char data[];
	};
	void Initialize();
}