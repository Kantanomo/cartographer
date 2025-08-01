#include "stdafx.h"
#include "achievement_live_interface.h"

/* public code */

void c_achievement_live_interface::start_upload(void)
{
	INVOKE_TYPE(0x4A808, 0x0, void(__thiscall*)(c_achievement_live_interface*), this);
	return;
}

c_achievement_live_interface* achievement_live_interface_get(void)
{
	return *Memory::GetAddress<c_achievement_live_interface**>(0x482D48);
}
