#include "stdafx.h"
#include "patches_initialize.h"

#include "main/map_repository.h"
#include "rasterizer/dx9/rasterizer_dx9_main.h"

/* public code */

void sapien_apply_patches(void)
{
	map_repository_apply_sapien_patches();
	rasterizer_dx9_sapien_apply_patches();
	return;
}
