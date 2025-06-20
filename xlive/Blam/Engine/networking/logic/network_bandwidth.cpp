#include "stdafx.h"
#include "network_bandwidth.h"

/* public code */

bool __cdecl network_bandwidth_initialize(c_network_observer* observer, void* configuration)
{
	return INVOKE(0x1E0CE1, 0x1B5BD1, network_bandwidth_initialize, observer, configuration);
}
