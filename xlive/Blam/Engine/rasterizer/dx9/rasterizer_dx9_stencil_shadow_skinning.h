#pragma once

// Per-frame pose for ARTICULATED shadow sections - CPU skin and plane recompute, plus the engine
// skinning-pool lookup both this and the static path read. It regenerates plane data every frame, so
// it looks like generation, but the definition is already built and cached and this only poses it -
// the same line tag debug draws. What it costs and what remains: docs/11-open-items.md.

#include "rasterizer_dx9_stencil_shadows.h"

/* structures */

// Where one section's skinning-pool matrices live and how to index them: what
// stencil_shadow_pool_resolve produces, and what stencil_shadow_pool_slot_for_node translates
// through. The two layouts and how the resolve picks between them are described on the resolve.
struct s_stencil_shadow_pool_ref
{
	void* pool;				// the block; NULL never escapes (resolve returns false instead)
	int32 base_index;		// first matrix slot of this section's palette (0 for per-node)
	int32 matrix_count;		// valid slots at [base_index, base_index + matrix_count)
	bool palette;			// true = index by LOCAL node_map slot; false = by MODEL node
};

/* prototypes */

// Parity probe for the skinning-pool adoption: given the pool matrix just fetched for (object, node),
// compute the composition the pre-pool code would have used - interpolated `object_get_node_matrix x
// default_inverse_matrix` - and accumulate the componentwise delta. Budgeted; reports once per map.
//
// Reading it: ~0 deltas mean the pool equals our composition, so the render-time correction is
// inactive for the sampled content. Small deltas are that correction itself, the fidelity gain
// expected on models with render-only nodes or eye tracking. A LARGE rotation delta on an object with
// neither would mean a pool-layout assumption is wrong - set k_stencil_shadow_use_skinning_pool false
// and re-measure before trusting any volume built from the pool.
void stencil_shadow_pool_parity_probe(
	datum object_index,
	int32 node,
	const render_model_definition* render_model,
	const real_matrix4x3* pool_matrix);

// The pool lookup both adoption sites use, with miss-stage telemetry. Two layouts, selected by the
// model's force_node_maps flag (Vista content measures as entirely force):
//   * per-node - matrices [0, matrix_count) indexed directly by MODEL node. `palette` false,
//     `base_index` 0. Live only on non-force content.
//   * palette  - per-REGION palettes in LOCAL node_map order. `palette` true, `base_index` = the
//     region header's first_matrix_index, and a model node translates to base_index plus its slot in
//     the section's cached `pool_node_map`. Requires the caller's region_index and shadow. The
//     resolve REJECTS when the region header's count differs from the cached map, because that means
//     the engine skinned a different section (LOD divergence) and the palette order cannot be trusted.
bool stencil_shadow_pool_resolve(
	datum object_index,
	const render_model_definition* render_model,
	int32 region_index,							// NONE = caller has no region; palette layout then misses
	const struct s_stencil_shadow_section* shadow,	// for the cached node_map; may be NULL
	s_stencil_shadow_pool_ref* out_ref);

// MODEL node index -> pool matrix slot under the resolved layout, or NONE (node not in the
// section's palette - the caller falls back to composing that node itself).
int32 stencil_shadow_pool_slot_for_node(
	const s_stencil_shadow_pool_ref* pool_ref,
	const struct s_stencil_shadow_section* shadow,
	int32 model_node);

// Build the c50 palette for a section's GPU-skinned draw: 3 packed rows per node_map slot, in LOCAL
// order, matching the indices baked into the skinned VB. A pool hit is one memcpy, the pool's rows
// being the upload format already; otherwise the rows are composed from
// `interpolated(object_get_node_matrix) x inverse_bind`, which measures byte-identical to the pool, so
// the volume cannot change across the boundary. Returns the matrix count, or 0 when the section has no
// usable map or a node fails to resolve, at which point the caller draws the CPU path. `out_rows`
// needs capacity 3 * shadow->pool_node_map_count.
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
// `region_index` is the render-model region this section was selected from, since the skinning-pool
// palette is keyed by region. NONE is legal and simply keeps the pool inactive for the call, leaving
// the composition fallback to run.
bool stencil_shadow_section_animate(
	s_stencil_shadow_section* shadow,
	datum object_index,
	const render_model_definition* render_model,
	int32 region_index = NONE);

// Reset this module's per-map diagnostic latches. Called by stencil_shadow_cache_clear.
void stencil_shadow_skinning_reset_diagnostics(void);
