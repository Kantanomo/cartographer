#include "stdafx.h"
#include "main_time.h"

bool __cdecl main_time_halted(void)
{
	return INVOKE(0x3968B, 0x0, main_time_halted);
}

bool __cdecl main_time_is_throttled(void)
{
	return INVOKE(0x28729, 0x248AB, main_time_is_throttled);
}
