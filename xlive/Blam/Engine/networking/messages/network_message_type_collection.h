#pragma once

#include "H2MOD/Modules/CustomVariantSettings/CustomVariantSettings.h"

struct s_network_message_type
{
	bool initialized;
	uint8 pad[3];
	const char* name;
	uint32 flags;
	int32 message_size;
	int32 message_size_maximum;
	void* encode_function;
	void* decode_function;
	void* unknown_function;
};

class c_network_message_type_collection
{
public:

	void clear_message_types(void);
	void check_message_types(void) const;

	void register_message_type(int32 type, const char* message_type_name, uint32 flags, int32 message_size, int32 message_size_maximum, void* encode_function, void* decode_function, void* unk_callback)
	{
		INVOKE_TYPE(0x1E81D6, 0x1CA199, void(__thiscall*)(void*, int32, const char*, int32, int32, int32, void*, void*, void*),
			this, type, message_type_name, flags, message_size, message_size_maximum, encode_function, decode_function, unk_callback);
	}
private:
	s_network_message_type m_message_types[32];
};

enum e_network_message_type_collection : int32
{
	_ping,
	_pong,
	_broadcast_search,
	_broadcast_reply,
	_connect_request,
	_connect_refuse,
	_connect_establish,
	_connect_closed,
	_join_request,
	_join_abort,
	_join_refuse,
	_leave_session,
	_leave_acknowledge,
	_session_disband,
	_session_boot,
	_host_handoff,
	_peer_handoff,
	_host_transition,
	_host_reestablish,
	_host_decline,
	_peer_reestablish,
	_peer_establish,
	_election,
	_election_refuse,
	_time_synchronize,
	_membership_update,
	_peer_properties,
	_delegate_leadership,
	_boot_machine,
	_player_add,
	_player_refuse,
	_player_remove,
	_player_properties,
	_parameters_update,
	_parameters_request,
	_countdown_timer,
	_mode_acknowledge,
	_virtual_couch_update,
	_virtual_couch_request,
	_vote_update,
	_view_establishment,
	_player_acknowledge,
	_synchronous_update,
	_synchronous_actions,
	_synchronous_join,
	_synchronous_gamestate,
	_game_results,
	_text_chat,
	_test,

	// custom network meesages below
	_request_map_filename,
	_custom_map_filename,
	_rank_change,
	_anti_cheat,
	_custom_variant_settings,

	k_network_message_type_collection_count
};

static const char* k_network_message_type_collection_description[] = {
	"ping",
	"pong",
	"broadcast_search",
	"broadcast_reply",
	"connect_request",
	"connect_refuse",
	"connect_establish",
	"connect_closed",
	"join_request",
	"join_abort",
	"join_refuse",
	"leave_session",
	"leave_acknowledge",
	"session_disband",
	"session_boot",
	"host_handoff",
	"peer_handoff",
	"host_transition",
	"host_reestablish",
	"host_decline",
	"peer_reestablish",
	"peer_establish",
	"election",
	"election_refuse",
	"time_synchronize",
	"membership_update",
	"peer_properties",
	"delegate_leadership",
	"boot_machine",
	"player_add",
	"player_refuse",
	"player_remove",
	"player_properties",
	"parameters_update",
	"parameters_request",
	"countdown_timer",
	"mode_acknowledge",
	"virtual_couch_update",
	"virtual_couch_request",
	"vote_update",
	"view_establishment",
	"player_acknowledge",
	"synchronous_update",
	"synchronous_actions",
	"synchronous_join",
	"synchronous_gamestate",
	"game_results",
	"text_chat",
	"test",

	//custom packets below
	"request_map_filename",
	"map_file_name",
	"team_change",
	"rank_change",
	"anti_cheat",
	"custom_variant_settings",

	"end"
};

struct s_network_message_session_data
{
	XNKID session_id;
};

struct s_network_message_custom_map_filename
{
	s_network_message_session_data session_data;
	wchar_t file_name[32];
	int map_download_id;
};

struct s_network_message_request_map_filename
{
	s_network_message_session_data session_data;
	unsigned long long player_id;
	int map_download_id;
};

struct s_network_message_rank_change
{
	s_network_message_session_data session_data;
	int8 rank;
};

struct s_network_message_anti_cheat
{
	s_network_message_session_data session_data;
	bool enabled;
};

struct s_network_message_session_custom_variant_settings
{
	s_network_message_session_data session_data;
	CustomVariantSettings::s_variant_settings settings;
};

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

const char* get_network_message_description(int32 type);
bool is_message_custom(e_network_message_type_collection type);

namespace NetworkMessage
{
	void ApplyGamePatches();
	void SendRequestMapFilename(int mapDownloadId);
	void SendRankChange(int32 peer_index, int8 rank);
	void SendAntiCheat(int32 peer_index);
}