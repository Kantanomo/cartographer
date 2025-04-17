#include "stdafx.h"

#include "network_message_type_collection.h"
#include "network_message_handler.h"

#include "cartographer/twizzler/twizzler.h"
#include "interface/user_interface_controller.h"
#include "memory/bitstream.h"
#include "networking/delivery/network_channel.h"

#include "H2MOD/Modules/EventHandler/EventHandler.hpp"
#include "H2MOD/Modules/MapManager/MapManager.h"

c_network_message_type_collection g_network_message_type_collection[k_network_message_type_collection_count];

const char* get_network_message_description(int32 type)
{
	return k_network_message_type_collection_description[type];
}

bool is_message_custom(e_network_message_type_collection type)
{
	return type > e_network_message_type_collection::_test;
}

void __cdecl encode_map_file_name_message(c_bitstream* stream, int a2, const s_network_message_custom_map_filename* data)
{
	stream->write_raw_data("session-id", data->session_data.session_id.ab, SIZEOF_BITS(data->session_data.session_id.ab));
	stream->write_string_wchar("map-file-name", data->file_name, ARRAYSIZE(data->file_name));
	stream->write_integer("map-download-id", data->map_download_id, SIZEOF_BITS(data->map_download_id));
}
bool __cdecl decode_map_file_name_message(c_bitstream* stream, int a2, s_network_message_custom_map_filename* data)
{
	stream->read_raw_data("session-id", data->session_data.session_id.ab, SIZEOF_BITS(data->session_data.session_id.ab));
	stream->read_string_wchar("map-file-name", data->file_name, ARRAYSIZE(data->file_name));
	data->map_download_id = stream->read_integer("map-download-id", SIZEOF_BITS(data->map_download_id));
	return stream->error_occured() == false;
}

void __cdecl encode_request_map_filename_message(c_bitstream* stream, int a2, const s_network_message_request_map_filename* data)
{
	stream->write_raw_data("session-id", data->session_data.session_id.ab, SIZEOF_BITS(data->session_data.session_id.ab));
	stream->write_raw_data("user-identifier", &data->player_id, SIZEOF_BITS(data->player_id));
	stream->write_integer("map-download-id", data->map_download_id, SIZEOF_BITS(data->map_download_id));
}
bool __cdecl decode_request_map_filename_message(c_bitstream* stream, int a2, s_network_message_request_map_filename* data)
{
	stream->read_raw_data("session-id", data->session_data.session_id.ab, SIZEOF_BITS(data->session_data.session_id.ab));
	stream->read_raw_data("user-identifier", &data->player_id, SIZEOF_BITS(data->player_id));
	data->map_download_id = stream->read_integer("map-download-id", SIZEOF_BITS(data->map_download_id));
	return stream->error_occured() == false;
}

void __cdecl encode_rank_change_message(c_bitstream* stream, int a2, const s_network_message_rank_change* data)
{
	stream->write_raw_data("session-id", data->session_data.session_id.ab, SIZEOF_BITS(data->session_data.session_id.ab));
	stream->write_integer("rank", data->rank, SIZEOF_BITS(data->rank));
}
bool __cdecl decode_rank_change_message(c_bitstream* stream, int a2, s_network_message_rank_change* data)
{
	stream->read_raw_data("session-id", data->session_data.session_id.ab, SIZEOF_BITS(data->session_data.session_id.ab));
	data->rank = (int8)stream->read_integer("rank", SIZEOF_BITS(data->rank));
	return stream->error_occured() == false;
}

void __cdecl encode_anti_cheat_message(c_bitstream* stream, int a2, const s_network_message_anti_cheat* data)
{
	stream->write_raw_data("session-id", data->session_data.session_id.ab, SIZEOF_BITS(data->session_data.session_id.ab));
	stream->write_bool("", data->enabled);
}
bool __cdecl decode_anti_cheat_message(c_bitstream* stream, int a2, s_network_message_anti_cheat* data)
{
	stream->read_raw_data("session-id", data->session_data.session_id.ab, SIZEOF_BITS(data->session_data.session_id.ab));
	data->enabled = stream->read_bool("");
	return stream->error_occured() == false;
}

void __cdecl encode_custom_variant_settings(c_bitstream* stream, int a2, s_network_message_session_custom_variant_settings* data)
{
	stream->write_raw_data("session-id", data->session_data.session_id.ab, SIZEOF_BITS(data->session_data.session_id.ab));
	CustomVariantSettings::EncodeVariantSettings(stream, a2, &data->settings);
}
bool __cdecl decode_custom_variant_settings(c_bitstream* stream, int a2, s_network_message_session_custom_variant_settings* data)
{
	stream->write_raw_data("session-id", data->session_data.session_id.ab, SIZEOF_BITS(data->session_data.session_id.ab));
	CustomVariantSettings::DecodeVariantSettings(stream, a2, &data->settings);
	return stream->error_occured() == false;
}

void register_custom_network_message(c_network_message_type_collection* network_message_collection)
{
	typedef void(__cdecl* register_test_packet_t)(void*);
	auto p_register_test_message = Memory::GetAddress<register_test_packet_t>(0x1ECE05, 0x1CD7BE);

	p_register_test_message(network_message_collection);

	network_message_collection->register_message_type(
		_request_map_filename, 
		"request-map-filename", 
		0, 
		sizeof(s_network_message_request_map_filename), 
		sizeof(s_network_message_request_map_filename),
		(void*)encode_request_map_filename_message, 
		(void*)decode_request_map_filename_message, 
		NULL);

	network_message_collection->register_message_type(
		_custom_map_filename,
		"map-file-name",
		0,
		sizeof(s_network_message_custom_map_filename),
		sizeof(s_network_message_custom_map_filename),
		(void*)encode_map_file_name_message,
		(void*)decode_map_file_name_message,
		NULL);

	network_message_collection->register_message_type(
		_rank_change,
		"rank-change",
		0,
		sizeof(s_network_message_rank_change),
		sizeof(s_network_message_rank_change),
		(void*)encode_rank_change_message,
		(void*)decode_rank_change_message,
		NULL);

	network_message_collection->register_message_type(
		_anti_cheat,
		"",
		0,
		sizeof(s_network_message_anti_cheat),
		sizeof(s_network_message_anti_cheat),
		(void*)encode_anti_cheat_message,
		(void*)decode_anti_cheat_message,
		NULL);

	network_message_collection->register_message_type(
		_custom_variant_settings, 
		"variant-settings", 
		0, 
		k_custom_variant_settings_packet_size, 
		k_custom_variant_settings_packet_size,
		(void*)encode_custom_variant_settings,
		(void*)decode_custom_variant_settings,
		NULL);
}

void NetworkMessage::SendRequestMapFilename(int mapDownloadId)
{
	c_network_session* session = NULL;
	s_network_message_request_map_filename data;

	if (network_life_cycle_in_squad_session(&session)
		&& session->established()
		&& !session->is_host()
		&& session->get_transport_session_id(&data.session_data.session_id))
	{
		XUserGetXUID(0, &data.player_id);
		data.map_download_id = mapDownloadId;

		c_network_observer* observer = session->m_network_observer;
		s_session_peer* peer = session->get_session_peer(session->m_session_host_peer_index);

		if (peer->is_remote_peer) {
			observer->send_message(session->m_session_index, peer->observer_channel_index, false, _request_map_filename, sizeof(s_network_message_request_map_filename), &data);

			LOG_TRACE_NETWORK("{} session host peer index: {}, observer index {}, observer is remote peer: {}, session index: {}",
				__FUNCTION__,
				session->m_session_host_peer_index,
				peer->observer_channel_index,
				peer->is_remote_peer,
				session->m_session_index);
		}
	}
}

void NetworkMessage::SendRankChange(int32 peer_index, int8 rank)
{
	c_network_session* session = NULL;
	s_network_message_rank_change data;

	if (network_life_cycle_in_squad_session(&session) 
		&& session->is_host()
		&& session->get_transport_session_id(&data.session_data.session_id))
	{
		data.rank = rank;

		c_network_observer* observer = session->m_network_observer;
		s_session_peer* peer = session->get_session_peer(peer_index);

		if (peer_index != NONE && !session->is_peer_local(peer_index))
		{
			if (peer->is_remote_peer) {
				observer->send_message(session->m_session_index, peer->observer_channel_index, false, _rank_change, sizeof(s_network_message_rank_change), &data);
			}
		}
	}
}
void NetworkMessage::SendAntiCheat(int32 peer_index)
{
	c_network_session* session = NULL;
	s_network_message_anti_cheat data;

	if (network_life_cycle_in_squad_session(&session) 
		&& session->is_host()
		&& session->get_transport_session_id(&data.session_data.session_id))
	{
		c_network_observer* observer = session->m_network_observer;
		s_session_peer* peer = session->get_session_peer(peer_index);

		data.enabled = g_twizzler_status;
		if (peer_index != NONE && !session->is_peer_local(peer_index)) {
			if (peer->is_remote_peer) {
				observer->send_message(session->m_session_index, peer->observer_channel_index, false, _anti_cheat, sizeof(s_network_message_anti_cheat), &data);
			}
		}
	}
}

void NetworkMessage::ApplyGamePatches()
{
	WritePointer(Memory::GetAddress(0x1AC733, 0x1AC901), g_network_message_type_collection);
	WritePointer(Memory::GetAddress(0x1AC8F8, 0x1ACAC6), g_network_message_type_collection);
	WriteValue<uint8>(Memory::GetAddress(0x1E825E, 0x1CA221), e_network_message_type_collection::k_network_message_type_collection_count);
	WriteValue<int32>(Memory::GetAddress(0x1E81C6, 0x1CA189), e_network_message_type_collection::k_network_message_type_collection_count * 32);

	PatchCall(Memory::GetAddress(0x1B5196, 0x1A8EF4), register_custom_network_message);

	network_message_handler_apply_patches();
}