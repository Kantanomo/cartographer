#include "stdafx.h"
#include "panorama_user_history.h"

#include "networking/network_event.h"

/* public code */

c_panorama_user_history* panorama_user_history_get(void)
{
	return Memory::GetAddress<c_panorama_user_history*>(0x518210, 0x5426A8);
}
