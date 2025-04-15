#include "stdafx.h"

#include "network_message_handler.h"

#include "cartographer/twizzler/twizzler.h"

#include "networking/session/network_session.h"
#include "networking/session/network_observer.h"
#include "networking/delivery/network_channel.h"

#include "H2MOD/Modules/CustomVariantSettings/CustomVariantSettings.h"
#include "H2MOD/Modules/EventHandler/EventHandler.hpp"
#include "H2MOD/Modules/MapManager/MapManager.h"

/* constants */


/* typedefs */

typedef void(__stdcall* t_read_channel_message)(c_network_message_handler* thisx, int32 network_channel_index, e_network_message_type_collection message_type, int32 message_storage_size, uint8* packet);

typedef void(__stdcall* t_handle_out_of_band_message)(c_network_message_handler* thisx, network_address* address, e_network_message_type_collection message_type, int32 a4, uint8* packet);

/* prototypes */
void network_message_handler_apply_patches();

static void __stdcall handle_out_of_band_message_hook(c_network_message_handler* thisx, network_address* address, e_network_message_type_collection message_type, int32 a4, uint8* packet);
static void __stdcall read_channel_message_hook(c_network_message_handler* thisx, int32 network_channel_index, e_network_message_type_collection message_type, int32 message_storage_size, uint8* packet);

/* globals */

t_read_channel_message p_read_channel_message;
t_handle_out_of_band_message p_handle_out_of_band_message;

/* public code */

void network_message_handler_apply_patches()
{
	p_read_channel_message = (t_read_channel_message)DetourClassFunc(Memory::GetAddress<BYTE*>(0x1E929C, 0x1CB25C), (BYTE*)read_channel_message_hook, 8);
	p_handle_out_of_band_message = (t_handle_out_of_band_message)DetourClassFunc(Memory::GetAddress<BYTE*>(0x1E907B, 0x1CB03B), (BYTE*)handle_out_of_band_message_hook, 8);
}

/* private code */

void __stdcall handle_out_of_band_message_hook(c_network_message_handler* thisx, network_address* address, e_network_message_type_collection message_type, int32 a4, uint8* packet)
{
	c_network_session* session;
	if (network_life_cycle_in_squad_session(&session))
	{
		/* surprisingly the game doesn't use this too much, pretty much for request-join and time-sync packets */
		LOG_TRACE_NETWORK("{} - Received message: {} from peer index: {}",
			__FUNCTION__, get_network_message_description(message_type), session->get_peer_index_from_address(address));
	}

	if (!is_message_custom(message_type))
		p_handle_out_of_band_message(thisx, address, message_type, a4, packet);
}

void __stdcall read_channel_message_hook(c_network_message_handler* thisx, int32 network_channel_index, e_network_message_type_collection message_type, int32 message_storage_size, uint8* packet)
{
	/*
		This handles received in-band data
	*/

	network_address addr{};
	s_network_channel* peer_network_channel = s_network_channel::get(network_channel_index);

	switch (message_type)
	{
	case _request_map_filename:
	{
		if (peer_network_channel->is_channel_state_5()
			&& peer_network_channel->get_network_address(&addr))
		{
			thisx->handle_request_map_filename(&addr, (s_network_message_request_map_filename*)packet);
		}
		break;
	}

	case _custom_map_filename:
	{
		if (peer_network_channel->is_channel_state_5()
			&& peer_network_channel->get_network_address(&addr))
		{
			thisx->handle_map_filename_response(&addr, network_channel_index, (s_network_message_custom_map_filename*)packet);
		}
		break;
	}

	case _rank_change:
	{
		if (peer_network_channel->is_channel_state_5()
			&& peer_network_channel->get_network_address(&addr))
		{
			thisx->handle_player_property_rank(&addr, network_channel_index, (s_network_message_rank_change*)packet);
		}
		break;
	}

	case _anti_cheat:
	{
		if (peer_network_channel->is_channel_state_5()
			&& peer_network_channel->get_network_address(&addr))
		{
			thisx->handle_session_anticheat_status(&addr, network_channel_index, (s_network_message_anti_cheat*)packet);
		}
		break;
	}

	case _custom_variant_settings:
	{
		if (peer_network_channel->is_channel_state_5()
			&& peer_network_channel->get_network_address(&addr))
		{
			thisx->handle_session_custom_variant_settings(&addr, network_channel_index, (s_network_message_session_custom_variant_settings*)packet);
		}
		break;
	}

	// default packet
	case _leave_session:
	{
		if (peer_network_channel->is_channel_state_5()
			&& peer_network_channel->get_network_address(&addr))
		{
			thisx->handle_leave_session(&addr, (s_network_message_session_data*)packet);
		}
		break; // don't return, leave the game to update state
	}

	default:
		break;
	} // switch (message_type)

	if (peer_network_channel->get_network_address(&addr))
	{
		LOG_TRACE_NETWORK("{} - Received message: {} from network channel: {}, address: {:x}",
			__FUNCTION__, get_network_message_description(message_type), network_channel_index, ntohl(addr.address.ipv4));
	}
	else
	{
		LOG_ERROR_NETWORK("{} - Received message: {} from an unestablished network channel: {}",
			__FUNCTION__, get_network_message_description(message_type), network_channel_index);
	}

	if (!is_message_custom(message_type))
		p_read_channel_message(thisx, network_channel_index, message_type, message_storage_size, packet);

	switch (message_type)
	{
	case _membership_update:
		if (peer_network_channel->is_channel_state_5()
			&& peer_network_channel->get_network_address(&addr))
		{
			thisx->handle_membership_update(&addr, network_channel_index, (s_network_message_session_data*)packet);
		}
		break;
	case _player_add:
		if (peer_network_channel->is_channel_state_5()
			&& peer_network_channel->get_network_address(&addr))
		{
			thisx->handle_player_add(&addr, (s_network_message_session_data*)packet);
		}
		break;
	default:
		break;
	}
}

/* ### TODO move these handlers to separate files */
/* ### TODO move this to cartographer session */

void c_network_message_handler::handle_request_map_filename(const network_address* address, const s_network_message_request_map_filename* received_data)
{
	c_network_session* session = m_session_manager->get_network_session_by_id(&received_data->session_data.session_id);
	if (session)
	{
		LOG_TRACE_NETWORK("[H2MOD-CustomMessage] received on read_channel_message_hook request-map-filename from XUID: {}",
			received_data->player_id);

		int32 sender_peer_index = session->get_peer_index_from_address(address);

		if (sender_peer_index != NONE
			&& !session->is_peer_local(sender_peer_index))
		{
			s_network_message_custom_map_filename data{};

			std::wstring map_filename;
			mapManager->GetMapFilename(map_filename);
			if (!map_filename.empty())
			{
				wcsncpy_s(data.file_name, map_filename.c_str(), map_filename.length());
				data.map_download_id = received_data->map_download_id;

				LOG_TRACE_NETWORK(L"[H2MOD-CustomMessage] sending map file name packet to player id: {}, peer index: {}, map name: {}, download id {}",
					received_data->player_id,
					sender_peer_index, map_filename.c_str(), received_data->map_download_id);

				c_network_observer* observer = session->m_network_observer;
				s_session_peer* peer = session->get_session_peer(sender_peer_index);

				if (peer->is_remote_peer)
					observer->send_message(session->m_session_index, peer->observer_channel_index, false, _custom_map_filename, sizeof(s_network_message_custom_map_filename), &data);
			}
			else
			{
				LOG_TRACE_NETWORK(L"[H2MOD-CustomMessage] no map file name found, abort sending packet! player id: {}, peer idx: {} map filename: {}",
					received_data->player_id, sender_peer_index, map_filename.c_str());
			}
		}
	}
}

void c_network_message_handler::handle_map_filename_response(const network_address* address, int32 channel_index, const s_network_message_custom_map_filename* received_data)
{
	c_network_session* session = m_session_manager->get_network_session_by_id(&received_data->session_data.session_id);
	if (session)
	{
		if (session->channel_is_authoritative(channel_index))
		{
			if (received_data->map_download_id != NONE)
			{
				auto map_download_query = mapManager->GetDownloadQueryById(received_data->map_download_id);
				if (map_download_query != nullptr)
				{
					map_download_query->SetMapNameToDownload(received_data->file_name);
					LOG_TRACE_NETWORK(L"[H2MOD-CustomMessage] received on read_channel_message_hook custom_map_filename: {}",
						received_data->file_name);
				}
				else
				{
					LOG_TRACE_NETWORK("[H2MOD-CustomMessage] - query with id {:X} hasn't been found!",
						received_data->map_download_id);
				}
			}
		}
	}
}

void c_network_message_handler::handle_player_property_rank(const network_address* address, int32 channel_index, const s_network_message_rank_change* received_data)
{
	c_network_session* session = m_session_manager->get_network_session_by_id(&received_data->session_data.session_id);
	if (session)
	{
		if (session->channel_is_authoritative(channel_index))
		{
			LOG_TRACE_NETWORK(L"H2MOD-CustomMessage] recieved on read_channel_message_hook rank_change: {}",
				received_data->rank);
			network_session_interface_set_local_user_rank(0, received_data->rank);
		}
	}
}

void c_network_message_handler::handle_session_anticheat_status(const network_address* address, int32 channel_index, const s_network_message_anti_cheat* received_data)
{
	c_network_session* session = m_session_manager->get_network_session_by_id(&received_data->session_data.session_id);
	if (session)
	{
		if (session->channel_is_authoritative(channel_index))
		{
			twizzler_set_status(received_data->enabled);
		}
	}
}

void c_network_message_handler::handle_session_custom_variant_settings(const network_address* address, int32 channel_index, const s_network_message_session_custom_variant_settings* received_data)
{
	c_network_session* session = m_session_manager->get_network_session_by_id(&received_data->session_data.session_id);
	if (session)
	{
		if (session->channel_is_authoritative(channel_index))
		{
			CustomVariantSettings::UpdateCustomVariantSettings(&received_data->settings);
		}
	}
}

void c_network_message_handler::handle_leave_session(const network_address* address, const s_network_message_session_data* received_data)
{
	c_network_session* session = m_session_manager->get_network_session_by_id(&received_data->session_id);
	if (session)
	{
		int32 sender_peer_index = session->get_peer_index_from_address(address);

		if (sender_peer_index != NONE
			&& !session->is_peer_local(sender_peer_index))
		{
			EventHandler::NetworkPlayerEventExecute(EventExecutionType::execute_before, sender_peer_index, EventHandler::NetworkPlayerEventType::remove);
		}
	}
}

void c_network_message_handler::handle_membership_update(const network_address* address, int32 channel_index, const s_network_message_session_data* received_data)
{
	c_network_session* session = m_session_manager->get_network_session_by_id(&received_data->session_id);
	if (session)
	{
		if (session->channel_is_authoritative(channel_index))
		{
			network_session_membership_update_local_players_teams();
		}
	}
}

void c_network_message_handler::handle_player_add(const network_address* address, const s_network_message_session_data* received_data)
{
	c_network_session* session = m_session_manager->get_network_session_by_id(&received_data->session_id);
	if (session)
	{
		int32 sender_peer_index = session->get_peer_index_from_address(address);

		if (sender_peer_index != NONE
			&& !session->is_peer_local(sender_peer_index))
		{
			EventHandler::NetworkPlayerEventExecute(EventExecutionType::execute_after, sender_peer_index, EventHandler::NetworkPlayerEventType::add);

			if (session->is_host())
			{
				NetworkMessage::SendAntiCheat(sender_peer_index);
			}
		}
	}
}