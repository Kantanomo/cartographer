#include "stdafx.h"
#include "hud_definitions.h"

/* globals */

const char* global_hud_anchor_names[5]
{
	"top_left",
	"top_right",
	"bottom_left",
	"bottom_right",
	"center"
};

/* public code */

hud_globals_definition* get_hud_globals(void)
{
	return Memory::GetAddress<hud_globals_definition*>(0x9765C8, 0x99FBB0);
}
