#include "stdafx.h"
#include "unit_action_system.h"

/* public functions */

bool __cdecl action_submit(datum unit_index, e_unit_action action)
{
	return INVOKE(0x165E7D, 0x15B93D, action_submit, unit_index, action);
}
