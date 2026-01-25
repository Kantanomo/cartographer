#pragma once

#ifdef EVENTS_ENABLED

#include "networking/network_event.h"

/* prototypes */

void data_mine_add_event(e_event_level event_level, const char* event_text);

#endif
