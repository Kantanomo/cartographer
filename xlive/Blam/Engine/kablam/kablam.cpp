#include "stdafx.h"
#include "kablam.h"

#include "shell/shell.h"
#include "shell/shell_windows.h"

#include "H2MOD/GUI/ImGui_Integration/Console/CommandHandler.h"
#include "H2MOD/Modules/EventHandler/EventHandler.hpp"
#include "H2MOD/Modules/OnScreenDebug/OnscreenDebug.h"
#include "H2MOD/Modules/Shell/Config.h"

/* typedefs */

typedef void* (__cdecl* dedi_command_t)(wchar_t** a1, int32 a2, bool a3);
typedef int32(__cdecl* kablam_vip_add_t)(const wchar_t* player_name);
typedef int32(__cdecl* kablam_vip_clear_t)();
typedef int(__cdecl* hookServ1_t)(HKEY, LPCWSTR);
typedef bool(__cdecl kablam_command_play_t)(wchar_t* playlist_file_path, int32 a2);

/* constants */

static const std::map<const wchar_t*, e_server_console_commands> k_commands_map =
{
	{L"ban", _kablam_command_ban},
	{L"description", _kablam_command_description},
	{L"exit", _kablam_command_exit},
	{L"kick", _kablam_command_kick},
	{L"live", _kablam_command_live},
	{L"name", _kablam_command_name},
	{L"players", _kablam_command_players},
	{L"playing", _kablam_command_playing},
	{L"privacy", _kablam_command_privacy},
	{L"sendmsg", _kablam_command_sendmsg},
	{L"skip", _kablam_command_skip},
	{L"statsfolder", _kablam_command_statsfolder},
	{L"status", _kablam_command_status},
	{L"unban", _kablam_command_unban},
	{L"vip", _kablam_command_vip},
	{L"any", _kablam_command_any}
};

/* globals */

dedi_command_t p_kablam_command_handler;
kablam_vip_add_t p_kablam_vip_add;
kablam_vip_clear_t p_kablam_vip_clear;
kablam_command_play_t* p_kablam_command_play;
hookServ1_t p_hookServ1;

static int32 g_message_timeout = 0;

/* prototypes */

static void vip_lock(e_game_life_cycle state);

static int __cdecl dedi_registry_hook(HKEY hKey, LPCWSTR lpSubKey);

static bool __cdecl should_start_pregame_countdown_hook(void);

static int32 get_active_count_from_bitflags(uint16 teams_bit_flags);

static void* __cdecl kablam_command_handler_hook(wchar_t** command_line_split_wide, int split_count, bool a3);

static bool __cdecl kablam_command_play(wchar_t* playlist_file_path, int32 a2);

static void server_console_apply_hooks(void);

static int server_console_log_to_dedicated_server_console_wide(const wchar_t* fmt, ...);

static int server_console_log_to_dedicated_server_console(const char* fmt, ...);

static void server_console_send_command(wchar_t** command, int32 split_commands_size, bool a3);

static void server_console_add_vip(const wchar_t* player_name);

static void server_console_clear_vip(void);

static void server_console_send_msg(const wchar_t* message, bool timeout);

static int __cdecl server_console_output_cb(StringHeaderFlags flags, const char* fmt, ...);

/* public code */

void kablam_apply_patches(void)
{
	// fix human turret variant setting not working on dedicated servers
	WriteValue<int32>(Memory::GetAddress(0x0, 0x3557FC), 1);

	// set the additional post-game carnage report time
	WriteValue<uint8>(Memory::GetAddress(0, 0xE590) + 2, H2Config_additional_pcr_time);

	p_hookServ1 = (hookServ1_t)DetourFunc(Memory::GetAddress<BYTE*>(0, 0x8EFA), (BYTE*)dedi_registry_hook, 11);

	PatchCall(Memory::GetAddress(0x0, 0xBF43), should_start_pregame_countdown_hook);


	server_console_apply_hooks();

	if (H2Config_vip_lock)
	{
		EventHandler::register_callback(vip_lock, EventType::gamelifecycle_change, EventExecutionType::execute_after);
	}
	return;
}

/* private code */

static void vip_lock(e_game_life_cycle state)
{
	c_network_session* session;

	if (network_life_cycle_in_squad_session(&session))
	{
		if (state == _life_cycle_post_game)
		{
			server_console_clear_vip();
			session->m_session_parameters.party_privacy = 0;
		}
		if (state == _life_cycle_in_game)
		{
			for (int32 i = 0; i < k_maximum_players; i++)
			{
				if (NetworkSession::PlayerIsActive(i))
				{
					server_console_add_vip(NetworkSession::GetPlayerName(i));
				}
			}
			session->m_session_parameters.party_privacy = 2;
		}
	}
	
	return;
}

static int __cdecl dedi_registry_hook(HKEY hKey, LPCWSTR lpSubKey)
{
	char result = p_hookServ1(hKey, lpSubKey);
	addDebugText("Post Server Registry Read.");
	if (csstrnlen(H2Config_dedi_server_playlist, 256) > 0)
	{
		wchar_t* ServerPlaylist = Memory::GetAddress<wchar_t*>(0, 0x3B3704);
		swprintf(ServerPlaylist, 256, L"%hs", H2Config_dedi_server_playlist);
	}
	return result;
}

static int32 get_active_count_from_bitflags(uint16 teams_bit_flags)
{
	int32 count = 0;
	for (int32 i = 0; i < _game_team_neutral; i++)
	{
		if (TEST_BIT(teams_bit_flags, i))
			count++;
	}
	return count;
}

static bool __cdecl should_start_pregame_countdown_hook(void)
{
	// dedicated server only
	auto p_should_start_pregame_countdown = Memory::GetAddress<decltype(&should_start_pregame_countdown_hook)>(0x0, 0xBC2A);

	c_network_session* session = NULL;
	network_life_cycle_in_squad_session(&session);

	// if the game already thinks the game timer doesn't need to start, return false and skip any processing
	if (!p_should_start_pregame_countdown()
		|| !session->is_local_peer_session_leader())
		return false;

	bool minimumPlayersConditionMet = true;
	if (H2Config_minimum_player_start > 0)
	{
		if (session->get_player_count() >= H2Config_minimum_player_start)
		{
			LOG_INFO_GAME(L"{} - minimum Player count met", __FUNCTIONW__);
			minimumPlayersConditionMet = true;
		}
		else
		{
			minimumPlayersConditionMet = false;
			server_console_send_msg(L"Waiting for Players | Esperando a los jugadores", true);
		}
	}

	if (!minimumPlayersConditionMet)
		return false;

	/*if (H2Config_even_shuffle_teams
		&& NetworkSession::IsVariantTeamPlay())
	{
		std::mt19937 mt_rand(rd());
		std::vector<int32> activePlayersIndices = NetworkSession::GetActivePlayerIndicesList();
		uint16 activeTeamsFlags = get_enabled_team_flags(NetworkSession::GetActiveNetworkSession());

		int32 max_teams = PIN(get_active_count_from_bitflags(activeTeamsFlags), 2, (int32)k_game_multiplayer_team_count);
		LOG_INFO_GAME("{} - balancing teams", __FUNCTION__);

		ServerConsole::SendMsg(L"Balancing Teams | Equilibrar equipos", true);

		int32 maxPlayersPerTeam = MAX(1, NetworkSession::GetPlayerCount() / max_teams);
		LOG_DEBUG_GAME("Players Per Team: {}", maxPlayersPerTeam);

		for (int32 i = 0; i < k_game_multiplayer_team_count; i++)
		{
			int32 currentTeamPlayers = 0;

			if (activePlayersIndices.empty())
				break;

			// check if the team is available for play
			if (!TEST_BIT(activeTeamsFlags, i))
				continue;

			std::uniform_int_distribution<int32> dist(0, activePlayersIndices.size() - 1);

			for (; currentTeamPlayers < maxPlayersPerTeam; currentTeamPlayers++)
			{
				int32 vecPlayerIdx = dist(mt_rand);
				int32 playerIndexSelected = activePlayersIndices[vecPlayerIdx];
				// swap the player index with the last one, then just pop the last element
				std::swap(activePlayersIndices[vecPlayerIdx], activePlayersIndices[activePlayersIndices.size() - 1]);
				activePlayersIndices.pop_back();

				NetworkMessage::SendTeamChange(NetworkSession::GetPeerIndex(playerIndexSelected), (e_game_team)i);
			}
		}
	}*/

	EventHandler::CountdownStartEventExecute(EventExecutionType::execute_after);
	return true;
}

static void* __cdecl kablam_command_handler_hook(wchar_t** command_line_split_wide, int split_count, bool a3)
{
	wchar_t* command = command_line_split_wide[0];
	if (command[0] == L'$') {
		server_console_log_to_dedicated_server_console_wide(L"# running custom command: ");
		
		c_static_string<256> command_line;
		c_static_wchar_string<256> token_wide;

		for (int32 i = 0; i < split_count; i++)
		{
			if (i > 0)
			{
				token_wide.set(command_line_split_wide[i]);
			}
			else
			{
				// skip $ character
				token_wide.set(&command_line_split_wide[i][1]);
			}

			utf8 converted[256];
			wchar_string_to_utf8_string(token_wide.get_string(), converted, NUMBEROF(converted));

			command_line.append(converted);
			command_line.append(" ");
		}

		ConsoleCommand::HandleCommandLine(command_line.get_string(), command_line.length(), server_console_output_cb);
		return 0;
	}

	// Transform the command into lower invariant so that we can convert the string to it's enum value
	c_static_wchar_string<256> lower_command(command);
	lower_command.to_lower();

	// Execute any events that may be linked to that command.
	// EventHandler::executeServerCommandCallback(ServerConsole::s_commandsMap[LowerCommand]);

	// Commented out because it's unused but the functionality of it remains, incase it is ever needed in the future for modifying the behaviour of the commands.
	/*switch(ServerConsole::s_commandsMap[LowerCommand])
	{
		case ServerConsole::ServerConsoleCommands::skip: {
			break;
		}
		case ServerConsole::ban: break;
		case ServerConsole::description: break;
		case ServerConsole::exit: break;
		case ServerConsole::kick: break;
		case ServerConsole::live: break;
		case ServerConsole::name: break;
		case ServerConsole::play: break;
		case ServerConsole::players: break;
		case ServerConsole::playing: break;
		case ServerConsole::privacy: break;
		case ServerConsole::sendmsg: break;
		case ServerConsole::statsfolder: break;
		case ServerConsole::status: break;
		case ServerConsole::unban: break;
		case ServerConsole::vip: break;
		default: ;
	}*/

	// Temporary if statement to prevent double calling events,
	// all server command functions will be hooked in the future and these event executes will be removed.

	bool playCommand = false;
	auto playCommandFind = k_commands_map.find(lower_command.get_string());
	if (playCommandFind != k_commands_map.end() && playCommandFind->second == _kablam_command_play)
		playCommand = true;

	if (!playCommand && playCommandFind != k_commands_map.end())
		EventHandler::ServerCommandEventExecute(EventExecutionType::execute_before, k_commands_map.at(lower_command.get_string()));

	void* result = p_kablam_command_handler(command_line_split_wide, split_count, a3);

	// Temporary if statement to prevent double calling events,
	// all server command functions will be hooked in the future and these executes will be removed.
	
	if (!playCommand && playCommandFind != k_commands_map.end())
		EventHandler::ServerCommandEventExecute(EventExecutionType::execute_after, k_commands_map.at(lower_command.get_string()));

	return result;
}


static bool __cdecl kablam_command_play(wchar_t* playlist_file_path, int32 a2)
{
	LOG_INFO_GAME("[{}]: {}", __FUNCTION__, "");
	EventHandler::ServerCommandEventExecute(EventExecutionType::execute_before, _kablam_command_play);
	auto res = p_kablam_command_play(playlist_file_path, a2);
	EventHandler::ServerCommandEventExecute(EventExecutionType::execute_after, _kablam_command_play);
	return res;
}

static void server_console_apply_hooks(void)
{
	if (!shell_is_dedicated_server())
		return;

	p_kablam_command_handler = (dedi_command_t)DetourFunc(Memory::GetAddress<BYTE*>(0, 0x1CCFC), (BYTE*)kablam_command_handler_hook, 7);
	p_kablam_vip_add = Memory::GetAddress<kablam_vip_add_t>(0, 0x1D932);
	p_kablam_vip_clear = Memory::GetAddress<kablam_vip_clear_t>(0, 0x1DB16);

	p_kablam_command_play = Memory::GetAddress<kablam_command_play_t*>(0, 0xE7FA);
	PatchCall(Memory::GetAddress(0, 0x724B), kablam_command_play);
	return;
}

static int server_console_log_to_dedicated_server_console_wide(const wchar_t* fmt, ...) 
{
	if (!shell_is_dedicated_server())
		return 0;

	int result;

	va_list ap;
	va_start(ap, fmt);
	result = vwprintf(fmt, ap);
	va_end(ap);

	return result;
}

static int server_console_log_to_dedicated_server_console(const char* fmt, ...)
{
	if (!shell_is_dedicated_server())
		return 0;

	int result;

	va_list ap;
	va_start(ap, fmt);
	result = vprintf(fmt, ap);
	va_end(ap);

	return result;
}

static void server_console_send_command(wchar_t** command, int32 split_commands_size, bool a3)
{
	typedef void(__cdecl* unk_func1_t)(void* a1);
	auto p_fn_unk1 = Memory::GetAddress<unk_func1_t>(0, 0x1D6EA);

	typedef void(__cdecl* free_memory_game_t)(void* mem);
	auto p_free_memory_game = Memory::GetAddress<free_memory_game_t>(0x0, 0x2344B8);

	typedef void(__thiscall* async_set_atomic_long_value_t)(void* thisx, long value);
	auto p_async_set_atomic_long_value = Memory::GetAddress<async_set_atomic_long_value_t>(0, 0x6E00);

	typedef void(__cdecl* unk_func3_t)(wchar_t* a1);
	auto p_fn_unk3 = Memory::GetAddress<unk_func3_t>(0, 0x19C93);

	uint8* unk1 = (uint8*)p_kablam_command_handler(command, split_commands_size, a3);
	uint8* threadparams = Memory::GetAddress<uint8*>(0, 0x450680);

	if (unk1)
	{
		if (*(BYTE*)(unk1 + 8))
		{
			p_async_set_atomic_long_value(*(BYTE**)(threadparams + 8), (LONG)unk1);
			p_fn_unk1(unk1);
		}
		else
		{
			p_fn_unk1(unk1);
			p_fn_unk3(*command);
		}

		// free allocated command resources
		p_free_memory_game(unk1);
	}

	BYTE v8 = (*(BYTE**)(threadparams + 8))[20];
	*(BYTE*)(threadparams + 4) = v8;
	if (!v8)
	{
		server_console_log_to_dedicated_server_console_wide(L"\r\n");
	}
	return;
}

static void server_console_add_vip(const wchar_t* player_name)
{
	p_kablam_vip_add(player_name);
	return;
}

static void server_console_clear_vip(void)
{
	p_kablam_vip_clear();
	return;
}

static void server_console_send_msg(const wchar_t* message, bool timeout)
{
	bool should_execute = !timeout;
	if (system_milliseconds() - g_message_timeout > 10 * 1000)
	{
		g_message_timeout = system_milliseconds();
		should_execute = true;
	}

	if (should_execute) {

		// first we construct kablam_command_send_msg, by manually passing the vtable pointer, and the message to be copied
		c_kablam_command_send_msg send_message(message);

		// send the message
		send_message.execute();
	}
	return;
}

static int __cdecl server_console_output_cb(StringHeaderFlags flags, const char* fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	vprintf(fmt, ap);
	printf("\n");
	va_end(ap);
	return 0;
}

