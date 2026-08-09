#include "stdafx.h"
#include "data_mining.h"

#ifdef EVENTS_ENABLED

/* public code */

void data_mine_add_event(e_event_level event_level, const char* event_text)
{
	ASSERT(VALID_INDEX(event_level, k_network_event_level_count));
	ASSERT(event_text);

	// TODO: implement
	return;
}

#endif
