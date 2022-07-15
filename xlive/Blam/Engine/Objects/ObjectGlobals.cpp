#include "stdafx.h"
#include "ObjectGlobals.h"

s_object_globals* s_object_globals::get()
{
	return *Memory::GetAddress<s_object_globals**>(0x4E4618, 0x50C8E8);
}
