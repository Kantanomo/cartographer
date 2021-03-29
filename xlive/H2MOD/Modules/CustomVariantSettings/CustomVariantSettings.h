#pragma once
#include "H2MOD/Modules/Networking/Memory/bitstream.h"

namespace CustomVariantSettings
{
	struct s_variantSettings
	{
		double Gravity = 4.171259403f;
		bool InfiniteAmmo = false;
	};
	void __cdecl EncodeVariantSettings(bitstream* stream, int a2, s_variantSettings* data);
	bool __cdecl DecodeVariantSettings(bitstream* stream, int a2, s_variantSettings* data);

	void RecieveCustomVariantSettings(s_variantSettings* data);
	void SendCustomVariantSettings(int peer_index);
	void ApplyHooks();
	void Initialize();
}
extern std::map<std::wstring, CustomVariantSettings::s_variantSettings> CustomVariantSettingsMap;
extern CustomVariantSettings::s_variantSettings CurrentVariantSettings;