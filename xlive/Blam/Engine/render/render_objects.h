#pragma once

/* prototypes */

void render_object_get_model_node_data(
	datum object_index,
	bool first_person,
	int16 render_model_index_index,
	uint32* flags,
	datum* out_object_index,
	datum* render_model_index,
	const real_matrix4x3** object_node_matrices,
	int32* object_node_matrix_count);
