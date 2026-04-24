#include "stdafx.h"
#include "bsp3d.h"

/* public code */

int32 __cdecl bsp3d_test_point(
	bsp3d* bsp,
	int32 node_index,
	real_point3d const* point)
{
	return INVOKE(0x76EFF, 0x82D92, bsp3d_test_point, bsp, node_index, point);
}
