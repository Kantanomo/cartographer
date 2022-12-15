#include "stdafx.h"
#include "HaloScriptNetworking.h"

#include "H2MOD.h"
#include "Blam/Engine/Networking/Session/NetworkSession.h"
#include "Blam/Engine/Game/GameGlobals.h"
#include "Util/Hooks/Hook.h"
#include "Blam\Engine\Memory\bitstream.h"
#include "Blam\Engine\Networking\NetworkMessageTypeCollection.h"
namespace HaloScriptNetworking
{
	e_hs_function hs_sync_table[]
	{
		e_hs_function_fade_out
	};
	template<typename C, typename T>
	bool contains(C&& c, T e) {
		return std::find(std::begin(c), std::end(c), e) != std::end(c);
	};

	unsigned int c_packet_id = 0;
	std::map<unsigned int, char*> packet_buffer;

	//Hooked function for game_is_predicited inside hs_init_threads to allow clients to execute scripts
	bool hs_init_threads_game_is_predicted()
	{
		return false;
	}

	typedef char*(__cdecl* t_hs_arguments_evaluate)(__int16 op_code, unsigned __int16 thread_id, char unk_bool);
	t_hs_arguments_evaluate p_hs_arguments_evaluate;
	char* __cdecl hs_arguments_evaluate(e_hs_function op_code, unsigned __int16 thread_id, char unk_bool)
	{
		//If there is no network session active just return default behavior
		if(NetworkSession::GetLocalSessionState() == _network_session_state_none)
			return p_hs_arguments_evaluate(op_code, thread_id, unk_bool);

		LOG_ERROR_GAME("[{}] {}", __FUNCTION__, hs_function_str[op_code]);

		if (h2mod->GetEngineType() == _single_player)
		{
			//If host send out the packets
			if(NetworkSession::LocalPeerIsSessionHost())
			{
				return p_hs_arguments_evaluate(op_code, thread_id, unk_bool);
			}
			//Client checks the sync_table
			if (contains(hs_sync_table, op_code))
			{
				auto packet_id = thread_id;
				return packet_buffer.at(packet_id);
			}
		}

		return p_hs_arguments_evaluate(op_code, thread_id, unk_bool);
	}

	void __cdecl EncodeHaloScriptSyncPacket(bitstream* stream, int a2, s_hs_sync_packet* data)
	{
		stream->data_encode_integer("hs_function_id", (WORD)data->type, 16);
		stream->data_encode_integer("data_size", data->data_size, sizeof(size_t) * CHAR_BIT);
		stream->data_encode_bits("data", &data->data, (sizeof(char) * data->data_size) * CHAR_BIT);
	}
	void __cdecl DecodeHaloScriptSyncPacket(bitstream* stream, int a2, s_hs_sync_packet* data)
	{
		data->type = (e_hs_function)stream->data_decode_integer("hs_function_id", 16);
		data->data_size = stream->data_decode_integer("data_size", sizeof(size_t) * CHAR_BIT);

		char* packet_data = (char*)calloc(data->data_size, sizeof(char));
		stream->data_decode_bits("data", &packet_data, (sizeof(char) * data->data_size) * CHAR_BIT);
		packet_buffer.emplace(c_packet_id, packet_data);
		
	}

	void ReceivedHaloScriptSyncPacket(s_hs_sync_packet data)
	{
		HaloScriptCommand* hs_function_table = Memory::GetAddress<HaloScriptCommand*>(0x41C5B0);
		hs_function_table[data.type].func(data.type, c_packet_id, 0);
		c_packet_id++;
	}

	void Initialize()
	{
		PatchCall(Memory::GetAddress(0x96C2B), hs_init_threads_game_is_predicted);
		DETOUR_BEGIN();
		DETOUR_ATTACH(p_hs_arguments_evaluate, Memory::GetAddress<t_hs_arguments_evaluate>(0x9581D, 0), hs_arguments_evaluate);
		DETOUR_COMMIT();
	}
}
