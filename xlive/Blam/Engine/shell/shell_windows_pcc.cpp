#include "stdafx.h"
#include "shell_windows_pcc.h"

/* public code */

bool __cdecl shell_windows_pcc_initialize(void)
{
	return INVOKE(0x260DDD, 0x0, shell_windows_pcc_initialize);
}

s_shell_pcc_settings* shell_windows_pcc_get_properties(void)
{
	return Memory::GetAddress<s_shell_pcc_settings*>(0x9DA8B0);
}

bool shell_windows_pcc_allows_vsync(void)
{
	return shell_windows_pcc_get_properties()->allow_vsync;
}
