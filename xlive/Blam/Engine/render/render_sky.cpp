#include "stdafx.h"
#include "render_sky.h"

real32 __cdecl get_current_sky_render_model_scale()
{
	return INVOKE(0x19A139, 0x186E29, get_current_sky_render_model_scale);
}

bool structure_bsp_test_current_sky_owner_cluster_index(int16 sky_cluster_index)
{
	return INVOKE(0x19A17D, 0, structure_bsp_test_current_sky_owner_cluster_index, sky_cluster_index);
}

bool render_sky_modify_node_matrices(real_vector3d* position, real_matrix4x3* in_matrix, real_matrix4x3* out_matrix, int iterations)
{
	return INVOKE(0x19A1E6, 0, render_sky_modify_node_matrices, position, in_matrix, out_matrix, iterations);
}
