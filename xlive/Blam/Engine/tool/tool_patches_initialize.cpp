#include "stdafx.h"
#include "tool_patches_initialize.h"

#include "cseries/cseries_windows.h"

/* public code */

void tool_apply_patches(void)
{
	cseries_windows_tool_apply_patches();
	return;
}
