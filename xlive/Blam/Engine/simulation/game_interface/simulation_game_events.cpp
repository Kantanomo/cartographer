#include "stdafx.h"
#include "simulation_game_events.h"

void game_engine_event_new(e_multiplayer_event_response_game_type game_type, e_multiplayer_event_response_event event_type, s_game_engine_event* event)
{
	event->game_type = game_type;
	event->event_type = event_type;
	event->player_index = NONE;
	event->field_C = NONE;
	event->field_10 = NONE;
	event->field_14 = NONE;
	event->field_18 = NONE;
	event->field_1C = 0;
	event->field_20 = 0xFFFF;
}
