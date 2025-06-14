#pragma once
#include "network_messages_cartographer.h"

/* forward declarations */

struct transport_address;
class c_network_message_gateway;
class c_network_message_type_collection;
class c_network_observer;
class c_network_link;
class c_network_session_manager;

/* classes */

class c_network_message_handler
{
public:
	bool initialize_handler(c_network_link* link, const c_network_message_type_collection* message_types, c_network_message_gateway* message_gateway);
	void register_session_manager(c_network_session_manager* session_manager);
	void register_observer(c_network_observer* observer);
	void handle_request_map_filename(const transport_address* address, const s_network_message_request_map_filename* received_data);
	void handle_map_filename_response(const transport_address* address, int32 channel_index, const s_network_message_custom_map_filename* received_data);
	void handle_player_property_rank(const transport_address* address, int32 channel_index, const s_network_message_rank_change* received_data);
	void handle_session_anticheat_status(const transport_address* address, int32 channel_index, const s_network_message_anti_cheat* received_data);
	void handle_session_custom_variant_settings(const transport_address* address, int32 channel_index, const s_network_message_session_custom_variant_settings* received_data);
	void handle_leave_session(const transport_address* address, const s_network_message_session_data* received_data);
	void handle_membership_update(const transport_address* address, int32 channel_index, const s_network_message_session_data* received_data);
	void handle_player_add(const transport_address* address, const s_network_message_session_data* received_data);
private:
	bool m_initialized;
	c_network_link* m_link;
	const c_network_message_type_collection* m_message_types;
	c_network_message_gateway* m_message_gateway;
	c_network_observer* m_observer;
	c_network_session_manager* m_session_manager;
};

/* prototypes */

void network_message_handler_apply_patches(void);