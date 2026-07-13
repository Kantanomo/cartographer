#pragma once

#ifdef EVENTS_ENABLED
#define TAG_INJECTION_DEBUG false
#else
#define TAG_INJECTION_DEBUG false	// Make sure it's always off if we don't have events enabled
#endif
