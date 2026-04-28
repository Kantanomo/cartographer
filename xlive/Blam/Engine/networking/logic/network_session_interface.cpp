#include "stdafx.h"
#include "network_session_interface.h"

#include "game/players.h"
#include "game/player_constants.h"
#include "networking/session/network_session_manager.h"
#include "networking/network_event.h"
#include "networking/network_game_definitions.h"

/* structures */

struct s_network_session_interface_user
{
	bool user_exists;
	s_player_identifier player_identifier;
	e_controller_index controller_index;
	s_player_configuration player_data;
	int32 player_voice_exists;
	int32 player_text_chat_exists;
	uint32 user_update_timestamp[4];
	uint32 user_remove_timestamp[3];
};
ASSERT_STRUCT_SIZE(s_network_session_interface_user, 0xB8);

struct s_network_session_interface_globals
{
	bool initialised;
	uint8 gap_1;
	wchar_t machine_name[16];
	wchar_t session_name[32];
	uint8 qos_active;
	uint8 gap_63;
	uint8 gap_64[16];
	uint32 upstream_bandwidth_bps;
	uint32 downstream_bandwidth_bps;
	uint8 gap_7C[8];
	uint32 nat_type;
	uint32 field_88;
	uint32 field_8C;
	uint32 field_90;
	int32 current_map_progress_percentage;
	s_network_session_interface_user users[k_number_of_users];
	uint32 session_connection_identifiers[6];
	s_game_variant variants[2];
	uint8 gap_5F0[68];
	uint32 sessions_manager;
};
ASSERT_STRUCT_SIZE(s_network_session_interface_globals, 0x638);

/* prototypes */

static s_network_session_interface_globals* network_session_interface_globals_get(void);

/* public code */

bool __cdecl network_session_interface_initialize(
	c_network_session_manager* session_manager)
{
	return INVOKE(0x1B07B2, 0x1968DC, network_session_interface_initialize, session_manager);
}

const wchar_t* network_session_interface_get_session_name(void)
{
	s_network_session_interface_globals* session_interface_globals = network_session_interface_globals_get();

	return session_interface_globals->session_name;
}

bool __cdecl network_session_interface_get_local_user_properties(
	int32 user_index,
	e_controller_index* controller_index,
	s_player_configuration* player_data,
	uint32* player_voice_settings,
	int32* out_player_text_chat)
{
	return INVOKE(0x1B10E0, 0x1970A8, network_session_interface_get_local_user_properties, user_index, controller_index, player_data, player_voice_settings, out_player_text_chat);
}

bool network_session_interface_set_local_user_character_type(
	int32 user_index,
	e_character_type character_type)
{
	bool success = false;
	s_network_session_interface_user* user_properties = &network_session_interface_globals_get()->users[user_index];

	// Don't change the character type if the user doesn't exist
	if (user_properties->user_exists)
	{
		user_properties->player_data.appearance.player_character_type = character_type;
		success = true;
	}

	return success;
}

bool network_session_interface_get_local_user_identifier(
	int32 user_index,
	s_player_identifier* out_identifier)
{
	bool success = false;
	s_network_session_interface_user* user_properties = &network_session_interface_globals_get()->users[user_index];

	if (user_properties->user_exists)
	{
		*out_identifier = user_properties->player_identifier;
		success = true;
	}

	return success;
}

int32 network_session_interface_get_team_index(
	int32 user_index)
{
	s_network_session_interface_globals* session_interface_globals = network_session_interface_globals_get();

	ASSERT(user_index>=0 && user_index<k_number_of_users);
	ASSERT(session_interface_globals->users[user_index].user_exists);

	return session_interface_globals->users[user_index].player_data.team_index;
}

void network_session_interface_set_local_user_rank(
	int32 user_index,
	int8 rank)
{
	s_network_session_interface_globals* session_interface_globals = network_session_interface_globals_get();

	ASSERT(user_index>=0 && user_index<k_number_of_users);
	ASSERT(session_interface_globals->users[user_index].user_exists);

	session_interface_globals->users[user_index].player_data.player_displayed_skill = rank;
	session_interface_globals->users[user_index].player_data.player_overall_skill = rank;
	
	return;
}

int32 __cdecl network_session_interface_add_local_user(
	s_player_identifier const* user_identifier)
{
	return INVOKE(0x1B1031, 0x0, network_session_interface_add_local_user, user_identifier);
}

void network_session_interface_set_user_identifier(
	int32 user_index,
	s_player_identifier const* user_identifier)
{
	s_network_session_interface_user* user_properties;

	ASSERT(0<=user_index && user_index<k_number_of_users);
	ASSERT(user_identifier!=NULL);

	user_properties = &network_session_interface_globals_get()->users[user_index];

	csmemcpy(&user_properties->player_identifier, user_identifier, sizeof(user_properties->player_identifier));

	event(_event_message, "logic:session: local user %d set user identifier=%s", user_index, player_identifier_get_string(&user_properties->player_identifier));

	return;
}

/* private code */

static s_network_session_interface_globals* network_session_interface_globals_get(void)
{
	return Memory::GetAddress<s_network_session_interface_globals*>(0x51A590, 0x520408);
}
