#include "stdafx.h"
#include "screen_network_squad_browser.h"

#include "interface/user_interface_controller.h"
#include "shell/shell.h"

/* typedefs */

typedef c_screen_network_squad_browser* (__cdecl* load_network_browser_t)(s_screen_parameters*);
load_network_browser_t p_load_network_browser;
load_network_browser_t p_load_live_browser;

/* public code */

void c_network_squad_list::sort_event(s_event_record* event, int32 selected_column_index)
{
	INVOKE_TYPE(0x218EE7, 0, void(__thiscall*)(c_network_squad_list*, s_event_record*, int32), this, event, selected_column_index);
}

c_screen_network_squad_browser* c_screen_network_squad_browser::load_network_browser(s_screen_parameters* parameters)
{
	c_screen_network_squad_browser* screen = p_load_network_browser(parameters);

	return screen;
}

c_screen_network_squad_browser* c_screen_network_squad_browser::load_live_browser(s_screen_parameters* parameters)
{
	c_screen_network_squad_browser* screen = p_load_live_browser(parameters);

	screen->m_selected_column_index = _screen_network_squad_browser_column_players;
	screen->m_list_is_sorted = true;

	s_event_record t_event{};
	t_event.type = _user_interface_event_type_mouse_button_left_click;

	// this forces the server list to sort by player count default
	// this called twice because the default behaviour in sorting
	// is ascending instead of descending
	screen->m_list.sort_event(&t_event, _screen_network_squad_browser_column_players);
	screen->m_list.sort_event(&t_event, _screen_network_squad_browser_column_players);

	screen->m_selected_column_index = NONE;

	return screen;
}


#pragma region Live list fix for disappearing labels
CLASS_HOOK_DECLARE_LABEL(c_screen_network_squad_browser__build_players_list_fix, c_screen_network_squad_browser::build_players_list_fix);

void c_screen_network_squad_browser::build_players_list_fix(c_player_widget_representation* representations, int32 player_count)
{
	if (m_live_list)
	{
		// don't load the player list from data, 
		// since we are in the live list, and we don't use the details pane located in the lower right part where players are listed
		return;
	}

	apply_new_representations_to_players(representations, player_count);
}

__declspec(naked) void jmp_build_player_list()
{ 
	CLASS_HOOK_JMP(c_screen_network_squad_browser__build_players_list_fix, c_screen_network_squad_browser::build_players_list_fix);
}
#pragma endregion

void* __cdecl c_screen_network_squad_browser::load(s_screen_parameters* parameters)
{
	return INVOKE(0x21A238, 0x0, c_screen_network_squad_browser::load, parameters);
}

void c_screen_network_squad_browser::apply_patches()
{
	if (shell_is_dedicated_server()) return;

	PatchCall(Memory::GetAddressRelative(0x619650), jmp_build_player_list);

	DETOUR_ATTACH(p_load_network_browser, Memory::GetAddress<load_network_browser_t>(0x21A238), c_screen_network_squad_browser::load_network_browser);
	DETOUR_ATTACH(p_load_live_browser, Memory::GetAddress<load_network_browser_t>(0x21A2E4), c_screen_network_squad_browser::load_live_browser);
}
