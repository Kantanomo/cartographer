#include "stdafx.h"
#include "squads.h"

/* public code */

void __cdecl squads_initialize_for_new_map(void)
{
	INVOKE(0x342385, 0x2ED0F5, squads_initialize_for_new_map);
	return;
}

bool squad_verify_script_references(int32 index, bool suppress_errors)
{
	// Does nothing in the game, editor code?
	return true;
}
