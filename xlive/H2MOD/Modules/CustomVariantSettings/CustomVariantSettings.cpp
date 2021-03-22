#include "CustomVariantSettings.h"
#include "H2MOD/Modules/Networking/Memory/bitstream.h"
#include "H2MOD/Modules/Networking/NetworkSession/NetworkSession.h"
#include "H2MOD.h"
#include "H2MOD/Modules/Networking/CustomPackets/CustomPackets.h"
#include "H2MOD/Modules/EventHandler/EventHandler.h"

struct s_weapon_group_definition;
std::map<std::wstring, CustomVariantSettings::s_variantSettings> CustomVariantSettingsMap;
CustomVariantSettings::s_variantSettings CurrentVariantSettings;
namespace CustomVariantSettings
{
	//TEMPORARY DO NOT LEAVE LAZY,
	float** gravity;

	void __cdecl EncodeVariantSettings(bitstream* stream, int a2, s_variantSettings* data)
	{
		stream->data_encode_quantized_real("Gravity", data->Gravity, 0.1f, 10.0f, 13, 0);
		stream->data_encode_bool("Infinite Ammo", data->InfiniteAmmo);
	}
	bool __cdecl DecodeVariantSettings(bitstream* stream, int a2, s_variantSettings* data)
	{
		data->Gravity = stream->data_decode_quantized_real("Gravity", 0.1f, 10.0f, 13, 0);
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
			//Temporary...
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
							sizeof(s_variantSettings), &CustomVariantSettingsMap.at(VariantName));
					}
				}
			}
		}
	}

	void OnIngame()
	{
		**gravity = CurrentVariantSettings.Gravity;
		/*if(CurrentVariantSettings.InfiniteAmmo)
		{
			auto weapons = tags::find_tags(blam_tag::tag_group_type::weapon);
			for(auto &weapon : weapons)
			{
				auto s_weapon = tags::get_tag<blam_tag::tag_group_type::weapon, s_weapon_group_definition>(weapon.first);
			}
		}*/
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
