#pragma once
#include "Blam/Engine/math/matrix_math.h"
#include "Blam/Engine/math/real_math.h"

real32 __cdecl get_current_sky_render_model_scale();

bool __cdecl structure_bsp_test_current_sky_owner_cluster_index(int16 sky_cluster_index);

bool __cdecl render_sky_modify_node_matrices(real_vector3d* position, real_matrix4x3* arg4, real_matrix4x3* a3, int iterations);