#pragma once
#include "render/render_lights.h"
#include "objects/scenery.h"

enum e_render_lod : int8
{
	_render_lod_disabled = 0,
	_render_lod_super_low = 1,
	_render_lod_low = 2,
	_render_lod_medium = 3,
	_render_lod_high = 4,
	_render_lod_super_high = 5,
	_render_lod_cinematic = 6
};

struct s_render_cache_storage
{
	datum context;
	uint8 gap_4[4];
	uint32 rasterizer_cpu_render_cache_offset;
	// kinda sucks this gets reset each frame
	uint32 render_frame_allocated;
	render_lighting lighting;
};
ASSERT_STRUCT_SIZE(s_render_cache_storage, 100);

struct s_render_object_info
{
	c_static_array<int32, 5> rasterizer_pool_offsets;
	c_static_array<int32, 5> field_18;
	c_static_array<uint8, 5> field_2C;
	c_static_array<datum, 5> object_index;
	c_static_array<s_render_cache_storage*, 5> render_info;
	c_static_array<datum, 5> render_model_tag_defs;
	c_static_array<uint8, 5> field_70;
	uint8 field_75[80];
	uint8 gap_C5;
	int8 field_C6[160];
	int16 object_count;
	int16 level_of_detail;
	e_scenery_lightmapping_policy scenery_lightmapping_policy;
	bool first_person;
	uint8 gap_16D[3];
	uint32 field_170;
	int32 field_174;
	datum shader_tag_index;
	int32 field_17C;
	int32 field_180;
	int32 field_184;
	real32 field_188;
	real32 field_18C;
	real32 field_190;
	real32 field_194;
	real32 field_198;
	real32 field_19C;
	real32 field_1A0;
	int32 field_1A4;
	uint8 field_1A8;
	int32 field_1AC;
	int32 field_1B0;
	real32 field_1B4;
	real32 field_1B8;
	uint8 gap_1BC[2];
	int16 field_1BE;
	int32 field_1C0;
	int32 field_1C4;
	int32* field_1C8;
	char field_1CC;
	uint8 gap_1CD[3];
	int32 field_1D0;
	uint8 field_1D4;
	uint8 gap_1D5[3];
	real32 field_1D8;
	int32 field_1DC;
	real32 field_1E0;
};
ASSERT_STRUCT_SIZE(s_render_object_info, 480);

struct s_render_object
{
	datum object_index;
	s_render_object_info info;
};
ASSERT_STRUCT_SIZE(s_render_object, 484);

// Engine accessor (halo2.exe 0x596397): cached render state for an object, or NULL.
//
// WARNING (it. 654): this routes through the engine ALLOCATOR (object_get_cached_render_state,
// 0x59604D) — on a miss it datum_new's, and when the pool is full it LRU-EVICTS another object's
// entry. Never call it from a render hook; use the read-only mirrors below instead.
s_render_cache_storage* __cdecl render_object_cache_get_render_state(datum object_index);

// Engine accessor (halo2.exe 0x596364): the object's current level of detail for the
// bound window (0..5 = l1..l6), or NONE.
int8 render_object_cache_get_level_of_detail(datum object_index);

// Gates the rasterizer GEOMETRY cache fields (entry+76/+80) -- NOT the lighting.
bool render_object_cache_storage_is_object_cached(s_render_cache_storage* storage);

// The object's computed render_lighting, or NULL. Read-only mirror of the engine's
// object_get_cached_render_lighting (halo2.exe 0x5969FF): ultimate-parent walk plus the
// authoritative lighting-valid gate (cache entry+3). Never allocates or evicts a cache entry,
// which is why it is not simply a call to the engine function. Use this -- not
// render_object_cache_get_render_state + is_object_cached -- to read lighting.
render_lighting* render_object_cache_get_lighting(datum object_index);

// Engine accessor (halo2.exe 0x77E337): one skinning matrix from an object's pool block, indexed
// DIRECTLY by node (entry = pool + 68 + 48*node — no region indirection). The 48-byte entry is
// 3 row-major float4 rows of `node_world x default_inverse_matrix` with scale folded into the
// basis; when `out_matrix` is given it is un-transposed into a real_matrix4x3 with scale = 1.0.
// Pass out_matrix as (real32*)&some_real_matrix4x3 — the 13 floats land exactly on that layout.
real32* __cdecl model_skinning_get_node_matrix(void* skinning_data, int16 node_index, real32* out_matrix);

// READ-ONLY mirror (it. 654): the object's skinning-matrix pool block for THIS frame in THIS
// window, or NULL. This is the pool `render_model_build_skinning` (0x77DEBD) filled during
// create_visible_render_primitives — matrices are interpolated (render_objects.cpp:56 tries the
// interpolator first) AND render-time composed (render-only nodes + eye tracking, 0x53599B runs
// inside the builder), with the inverse bind already multiplied in.
//
// Same never-allocate contract as render_object_cache_get_lighting, same guards:
//   * owner datum (entry+4) must equal object_index — the LRU recycles entries IN PLACE (verified
//     it. 654: eviction never datum_delete's, so stale handles keep valid salts and the owner
//     check is the real guard);
//   * `render_frame_allocated` must equal this frame+window's stamp — an object not submitted
//     through the visibility walk this frame (off-screen, cinematic-lit, first-person) has a stale
//     entry and gets NULL. Callers keep their own composition as the fallback.
//
// `out_matrix_count` receives the pool's matrix count (base nodes + compound nodes; the valid
// bound for node-indexed reads). NOTE: models with `_render_model_definition_force_node_maps`
// store only ONE matrix plus region palettes — node-indexed reads are invalid there, and it is the
// CALLER's job to test that flag (this function does not resolve the render model).
//
// `out_miss_stage` (optional): which check rejected, for diagnosis — 0 = hit, 1 = no object or no
// cached_render_state_index, 2 = datum lookup failed, 3 = owner mismatch (entry recycled),
// 4 = offset NONE, 5 = stamp mismatch (not filled this frame+window), 6 = pool resolve failed.
void* render_object_cache_get_skinning_pool(
	datum object_index, int32* out_matrix_count, int32* out_miss_stage = NULL);

void render_lod_new_apply_patches();