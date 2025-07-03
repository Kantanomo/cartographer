#include "stdafx.h"
#include "patches_initialize.h"

#include "main/map_repository.h"

/* public code */

void sapien_apply_patches(void)
{
	map_repository_apply_sapien_patches();
	return;
}
