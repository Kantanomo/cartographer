#pragma once

#include "network_message_type_collection.h"
#include "networking/session/network_session_manager.h"

class c_network_message_handler
{
	bool m_initialized;
	void* m_link;
	void* m_message_types;
	void* m_message_gateway;
	c_network_observer* m_observer;
	c_network_session_manager* m_session_manager;

public:
	void handle_request_map_filename(const network_address* address, const s_network_message_request_map_filename* received_data);
	void handle_map_filename_response(const network_address* address, int32 channel_index, const s_network_message_custom_map_filename* received_data);
	void handle_player_property_rank(const network_address* address, int32 channel_index, const s_network_message_rank_change* received_data);
	void handle_session_anticheat_status(const network_address* address, int32 channel_index, const s_network_message_anti_cheat* received_data);
	void handle_session_custom_variant_settings(const network_address* address, int32 channel_index, const s_network_message_session_custom_variant_settings* received_data);
	void handle_leave_session(const network_address* address, const s_network_message_session_data* received_data);
	void handle_membership_update(const network_address* address, int32 channel_index, const s_network_message_session_data* received_data);
	void handle_player_add(const network_address* address, const s_network_message_session_data* received_data);
};

void network_message_handler_apply_patches();