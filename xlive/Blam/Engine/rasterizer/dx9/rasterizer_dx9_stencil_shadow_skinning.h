#pragma once

// Per-frame pose for ARTICULATED shadow sections — CPU skin, dynamic VB refresh, plane recompute.
//
// WHY IT IS RASTERIZER-SIDE AND NOT IN THE GEOMETRY MODULE
// -------------------------------------------------------
// It regenerates ISQ plane data every frame, so it looks like generation. It is not: the geometry
// DEFINITION is already built and cached: this poses it. tag debug draws the same line — its
// equivalent, `section_skin_from_rigid_point_groups` (td 0x19EAF0), is a rasterizer function there
// too, alongside the facing test (`rasterizer_stencilshadow_build_bitvector_from_rigid_groups`,
// td 0x1A1100). We follow the reference rather than our own taste. See
// geometry/geometry_definitions_new_runtime.h for the other side of that split.
//
// WHAT IT COSTS, AND THE STANDING PLAN TO REMOVE IT
// ------------------------------------------------
// Three full passes per articulated section per object per frame (skin -> VB write -> plane
// recompute; it. 621 switched two further passes off). Because the cache is keyed by MODEL but the
// world positions live in the shared entry, N objects of one model each force a full re-skin and
// fight over one buffer — `last_animated_object` only skips the re-skin when the SAME object draws
// twice in a frame, which happens because render_scene runs several times per frame.
//
// it. 622 scoped the fix: GPU skinning. The engine already uploads a skinning matrix palette to
// c50 (`rasterizer_dx9_set_region_skinning_constants`, halo2.exe 0x662C22 — the same base our node
// constants use, with count 1), Vista's own shadow_buffer_generation_skinned_N_bone shaders index
// it as `c[a0.x + 50/51/52]` in vs_2_0, and the pool is retrievable per object. That would make the
// VB static and dissolve the sharing problem entirely.
//
// it. 653/654 answered the two measurements that gated this (both verified at the binary): the
// per-object pool offset lives at `s_render_cache_storage::rasterizer_cpu_render_cache_offset`
// with a per-frame-per-window validity stamp, and the pool's pose IS interpolated AND render-time
// composed. it. 655 adopted the pool for the per-NODE matrices (`k_stencil_shadow_use_skinning_pool`)
// — the per-vertex skin, VB write and plane recompute below remain, and fall only to the GPU step.

#include "rasterizer_dx9_stencil_shadows.h"

/* prototypes */

// it. 655 — PARITY PROBE for the skinning-pool adoption: given the pool matrix just fetched for
// (object, node), compute the composition the pre-pool code would have used — interpolated
// `object_get_node_matrix x default_inverse_matrix` — and accumulate the componentwise delta.
// Budgeted; reports once per map via `stencil poolprobe:`.
//
// Reading it: ~0 deltas mean the pool equals our composition (the it. 487 render-time correction is
// inactive for the sampled content). SMALL deltas are the correction itself — the fidelity gain,
// expected on models with render-only nodes or eye tracking. A LARGE rotation delta on an object
// with neither would mean a pool-layout assumption is wrong: set k_stencil_shadow_use_skinning_pool
// false and re-measure before trusting any volume built from the pool.
void stencil_shadow_pool_parity_probe(
	datum object_index,
	int32 node,
	const render_model_definition* render_model,
	const real_matrix4x3* pool_matrix);

// it. 656/658 — the pool lookup BOTH adoption sites use, with miss-stage telemetry (`stencil pool:`
// once per map — the it. 656 lesson: the outcome with no telemetry is the one that happens).
//
// TWO LAYOUTS, selected by the model's force_node_maps flag (it. 657 measured Vista content as 100%
// force):
//   * per-node  — matrices [0, matrix_count) indexed directly by MODEL node. `palette` false,
//     `base_index` 0. The layout it. 653 verified in the builder; live only on non-force content.
//   * palette   — per-REGION palettes in LOCAL node_map order. `palette` true, `base_index` = the
//     region header's first_matrix_index; a model node translates to base_index + its slot in the
//     section's cached `pool_node_map`. Requires the caller's region_index and shadow; the resolve
//     REJECTS (stage palette_miss) when the region header's count differs from the cached map —
//     that means the engine skinned a different section (LOD divergence) and the palette order
//     cannot be trusted.
struct s_stencil_shadow_pool_ref
{
	void* pool;				// the block; NULL never escapes (resolve returns false instead)
	int32 base_index;		// first matrix slot of this section's palette (0 for per-node)
	int32 matrix_count;		// valid slots at [base_index, base_index + matrix_count)
	bool palette;			// true = index by LOCAL node_map slot; false = by MODEL node
};

bool stencil_shadow_pool_resolve(
	datum object_index,
	const render_model_definition* render_model,
	int32 region_index,							// NONE = caller has no region; palette layout then misses
	const struct s_stencil_shadow_section* shadow,	// for the cached node_map; may be NULL
	s_stencil_shadow_pool_ref* out_ref);

// MODEL node index -> pool matrix slot under the resolved layout, or NONE (node not in the
// section's palette — the caller falls back to composing that node itself).
int32 stencil_shadow_pool_slot_for_node(
	const s_stencil_shadow_pool_ref* pool_ref,
	const struct s_stencil_shadow_section* shadow,
	int32 model_node);

// it. 660 — build the c50 PALETTE for a section's GPU-skinned draw: 3 packed rows per node_map
// slot, in LOCAL order (matching the indices baked into the skinned VB). Pool hit = one memcpy
// (the pool's rows ARE the upload format); otherwise the rows are composed from
// `interpolated(object_get_node_matrix) x inverse_bind` — which it. 659 measured BYTE-IDENTICAL to
// the pool, so the volume cannot change across the boundary. Returns the matrix count, 0 when the
// section has no usable map or a node fails to resolve (the caller then draws the CPU path).
// `out_rows` needs capacity 3 * shadow->pool_node_map_count.
int32 stencil_shadow_pool_build_palette(
	datum object_index,
	const render_model_definition* render_model,
	int32 region_index,
	const struct s_stencil_shadow_section* shadow,
	real_vector4d* out_rows);

// Transform an articulated section's welded verts into WORLD space by each vertex's node matrix,
// refresh the doubled dynamic VB, and recompute the facing planes in place. After this the section
// draws with IDENTITY node constants and the facing test takes the WORLD-space light.
//
// Guarded per (object, frame): render_scene runs several times per frame, so the re-skin is skipped
// when this object's pose is already current. It re-animates whenever a different object of the same
// model interleaves, because they share the cache entry.
// `region_index`: the render-model region this section was selected from — the skinning-pool
// palette is keyed by region (it. 658). NONE is legal and simply keeps the pool inactive for the
// call (the composition fallback runs, exactly the pre-655 behaviour).
bool stencil_shadow_section_animate(
	s_stencil_shadow_section* shadow,
	datum object_index,
	const render_model_definition* render_model,
	int32 region_index = NONE);

// Reset this module's per-map diagnostic latches. Called by stencil_shadow_cache_clear.
void stencil_shadow_skinning_reset_diagnostics(void);
