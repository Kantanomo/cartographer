#include "stdafx.h"
#include "simulation_game_internal.h"

/* public code */

void __cdecl simulation_write_definition_index(
	const char* description,
	c_bitstream* packet,
	datum index)
{
	ASSERT(description);
	ASSERT(packet);

	INVOKE(0x1F4CC0, 0x1DFD59, simulation_write_definition_index, description, packet, index);
	return;
}

datum __cdecl simulation_read_definition_index(
	const char* description,
	c_bitstream* packet)
{
	ASSERT(description);
	ASSERT(packet);

	return INVOKE(0x1F4CE9, 0x1DFD82, simulation_read_definition_index, description, packet);
}
