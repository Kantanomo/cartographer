#include "CustomVariantSettings.h"
#include "H2MOD/Modules/Networking/Memory/bitstream.h"
#include "H2MOD/Modules/Networking/NetworkSession/NetworkSession.h"
#include "H2MOD.h"
#include "H2MOD/Modules/Networking/CustomPackets/CustomPackets.h"
#include "H2MOD/Modules/EventHandler/EventHandler.h"
#include "H2MOD/Modules/Utils/Utils.h"
#include "Util/Hooks/Hook.h"

struct s_weapon_group_definition;
std::map<std::wstring, CustomVariantSettings::s_variantSettings> CustomVariantSettingsMap;
CustomVariantSettings::s_variantSettings CurrentVariantSettings;
namespace CustomVariantSettings
{
	//TODO: TEMPORARY DO NOT LEAVE, CREATE STRUCT FOR PHYSICS CONSTANTS.
	float** gravity;

	void __cdecl EncodeVariantSettings(bitstream* stream, int a2, s_variantSettings* data)
	{
		//bool negative = data->Gravity < 0;
		//int whole = (int)fabs(data->Gravity);
		//int remainder = (int)fabs((fabs(data->Gravity) - whole) * pow(10, 9));
		//LOG_INFO_GAME("{} - {} - {} - {} - {}", __FUNCTION__, negative, IntToString<double>(data->Gravity), whole, remainder);
		//stream->data_encode_bool("Gravity sign", negative);
		//stream->data_encode_integer("Gravity Whole", whole, 32);
		//stream->data_encode_integer("Gravity remainder", remainder, 32);
		stream->data_encode_bits("gravity", &data->Gravity, sizeof(data->Gravity) * CHAR_BIT);
		stream->data_encode_bool("Infinite Ammo", data->InfiniteAmmo);
	}
	bool __cdecl DecodeVariantSettings(bitstream* stream, int a2, s_variantSettings* data)
	{
		//bool negative = stream->data_decode_bool("Gravity sign");
		//signed int whole = stream->data_decode_signed_integer("Gravity Whole", 32);
		//int remainder = stream->data_decode_integer("Gravity remainder", 32);
		//double c;
		//c = (float)remainder;
		//while (c > 1.0f) c *= 0.1f; //moving the decimal point (.) to left most
		//c = (float)whole + c;
		//if (negative)
		//	c *= -1;
		//data->Gravity = c;
		//LOG_INFO_GAME("{} - {} - {} - {} - {}", __FUNCTION__, negative, IntToString<double>(c), whole, remainder);
		double gravity;
		stream->data_decode_bits("gravity", &gravity, sizeof(gravity) * CHAR_BIT);
		data->Gravity = gravity;
		data->InfiniteAmmo = stream->data_decode_bool("Infinite Ammo");
		return stream->packet_is_valid() == false;
	}

	void RecieveCustomVariantSettings(s_variantSettings* data)
	{
		CurrentVariantSettings.Gravity = data->Gravity;
		CurrentVariantSettings.InfiniteAmmo = data->InfiniteAmmo;

	}

	void SendCustomVariantSettings(int peer_index)
	{
		network_session* session = NetworkSession::getCurrentNetworkSession();
		if (NetworkSession::localPeerIsSessionHost())
		{
			//TODO: Find and mapout struct with current variant information.
			auto VariantName = std::wstring(h2mod->GetAddress<wchar_t*>(0, 0x534A18));
			if (CustomVariantSettingsMap.count(VariantName) > 0)
			{
				CurrentVariantSettings = CustomVariantSettingsMap.at(VariantName);
				network_observer* observer = session->network_observer_ptr;
				peer_observer_channel* observer_channel = NetworkSession::getPeerObserverChannel(peer_index);
				if (peer_index != -1 && peer_index != session->local_peer_index)
				{
					if (observer_channel->field_1)
					{
						observer->sendNetworkMessage(session->session_index, observer_channel->observer_index,
							network_observer::e_network_message_send_protocol::in_band, custom_variant_settings,
							13, &CustomVariantSettingsMap.at(VariantName));
					}
				}
			}
		}
	}

	void OnIngame()
	{
		//TODO: Figure out some way to make a safety check for this...
		**gravity = CurrentVariantSettings.Gravity;
		//mov [ecx+6], ax
		static BYTE InfiniteAmmoMagazineASM[] = { 0x66, 0x89, 0x41, 0x06 };
		//movss [edi+00000184],xmm0
		static BYTE InfiniteAmmoBatteryASM[] = { 0xF3, 0x0F, 0x11, 0x87, 0x84, 0x01, 0x00, 0x00 };
		if (CurrentVariantSettings.InfiniteAmmo)
		{
			NopFill(h2mod->GetAddress(0x15F3EA, 0x1436AA), 4);
			NopFill(h2mod->GetAddress(0x15f7c6, 0x143A86), 8);
		}
		else
		{
			WriteBytes(h2mod->GetAddress(0x15F3EA, 0x1436AA), InfiniteAmmoMagazineASM, 4);
			WriteBytes(h2mod->GetAddress(0x15f7c6, 0x143A86), InfiniteAmmoBatteryASM, 8);
		}
	}

	void OnLobby()
	{
		CurrentVariantSettings = s_variantSettings();
	}

	void ApplyHooks()
	{

	}

	void Initialize()
	{
		gravity = h2mod->GetAddress<float**>(0x4D2AB4, 0x4f696C);
		EventHandler::registerGameStateCallback({ "VariantSettings", life_cycle_in_game, OnIngame},	true);
		EventHandler::registerGameStateCallback({ "VariantSettings", life_cycle_pre_game, OnLobby }, true);
		EventHandler::registerNetworkPlayerAddCallback(
			{ 
				"VariantSettings",
				[](int peer)
				{
					SendCustomVariantSettings(peer);
				} 
			}, 
		true);
	}
}
