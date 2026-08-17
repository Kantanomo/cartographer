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

void render_lod_new_apply_patches();