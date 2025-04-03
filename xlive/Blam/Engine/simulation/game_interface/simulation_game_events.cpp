#include "stdafx.h"
#include "simulation_game_events.h"

void game_engine_event_new(e_multiplayer_event_response_game_type game_type, e_multiplayer_event_response_event event_type, s_game_engine_event* event)
{
	event->game_type = game_type;
	event->pad = 0;
	event->event_type = event_type;
	event->player_index = NONE;
	event->causing_player_index = NONE;
	event->causing_player_team = NONE;
	event->field_14 = NONE;
	event->field_18 = NONE;
	event->field_1C = 0;
	event->field_20 = 0xFFFF;
}

void __cdecl game_engine_send_event(s_game_engine_event* event)
{
	INVOKE(0x5DC77, 0, game_engine_send_event, event);
}