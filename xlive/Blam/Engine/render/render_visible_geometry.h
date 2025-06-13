#pragma once

/* structures */

struct s_render_visibility_data
{
	int8 gap_0[36];
};

struct s_render_visible_section
{
	uint32 flags;
	datum model_tag_index;
	int32 pool_index;
	uint32 part;
	void* field_10;
	int32 subpart_bitvector;
	int16 visibility_region_cluster_memory;
	int8 field_1A;
	int8 model_zsort_offset;
	real_matrix4x3* matrix;
};
ASSERT_STRUCT_SIZE(s_render_visible_section, 32);

struct s_render_visible_globals
{
	c_static_array<s_render_visible_section, 850> render_visible_sections;
	int32 render_visible_section_count;
	int32 last_render_visible_section_index;
	void* visibility_collection;
	c_static_array<int8, 850> render_visible_section_extern_shader_override;
	c_static_array<int8, 850> render_visible_section_shader_index_indices;
	c_static_array<datum, 192> render_visible_section_shader_index;
	int32 section_extern_shader_count;
	c_static_array<s_render_visibility_data, 32> visibility_data;
	int32 model_group_count;
// Don't enable this until we have full control of the global
// The global isnt' present in the release version of the game
//#ifdef _DEBUG
//	bool print_debug_text;
//#endif

	bool force_max_floats;	// TODO: better name
	bool unk_1;
	bool ran_out_of_section_extern_shader_overrides;
	int8 pad;
};
ASSERT_STRUCT_SIZE(s_render_visible_globals, 30844);

/* prototypes */

uint32 render_section_visibility_get_model_group_count(void);

s_render_visible_section* render_visible_section_get(int16 array_index);

int8 render_visible_section_extern_shader_override_get(int16 array_index);

datum render_visible_section_shader_index_get(int16 array_index);
