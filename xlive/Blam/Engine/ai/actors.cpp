#include "stdafx.h"
#include "actors.h"

data_array* get_actor_table()
{
	return *Memory::GetAddress<data_array**>(0xA965DC, 0x9A1C5C);
}
