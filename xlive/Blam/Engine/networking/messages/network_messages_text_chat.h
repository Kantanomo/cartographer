#pragma once
#include "network_message_type_collection.h"

/* structures */

#pragma pack(push, 1)
struct s_network_message_text_chat
{
	XNKID session_id;
	uint32 routed_players_mask;
	uint32 metadata;
	bool source_is_server;
	uint64 source_player_id;
	uint64 destination_players_ids[16];
	uint8 gap_99[3];
	int32 destination_player_count;
	wchar_t text_message[122];
};
#pragma pack(pop)
ASSERT_STRUCT_SIZE(s_network_message_text_chat, 404);

/* prototypes */

void __cdecl network_message_types_register_text_chat(c_network_message_type_collection* message_collection);
