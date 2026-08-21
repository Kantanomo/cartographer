#include "stdafx.h"
#include "rasterizer_dx9_stencil_shadow_skinning.h"

#include "rasterizer_dx9_errors.h"		// rasterizer_dx9_log_hr - the decoded-HRESULT report
#include "rasterizer_dx9_stencil_shadow_tunables.h"
#include "math/matrix_math.h"			// matrix4x3_multiply - skinning = node_world x inverse_bind
#include "main/interpolator.h"			// halo_interpolator_interpolate_object_node_matrix
#include "objects/objects.h"
#include "models/render_model_definitions.h"
#include "render/render.h"				// global_frame_index_get - the per-frame animate guard
#include "render/render_lod_new.h"		// the skinning pool mirror + node accessor
#include "networking/network_event.h"

/* globals */

// Per-map one-shot latches owned by this module, reset by stencil_shadow_skinning_reset_diagnostics.
// A latch that reports once MUST be file scope, or it describes only the first map of a session.
static bool g_stencil_shadow_warned_no_bind = false;

// Pool parity probe accumulators. Sampled across frames keeping the maximum, because a single sample
// cannot distinguish "matches" from "sampled when the correction happened to be zero".
static bool g_stencil_shadow_pool_probe_reported = false;
static uint32 g_stencil_shadow_pool_probe_samples = 0;
static uint32 g_stencil_shadow_pool_probe_interp_ran = 0;
static real32 g_stencil_shadow_pool_probe_max_basis = 0.f;
static real32 g_stencil_shadow_pool_probe_max_pos = 0.f;

// Per-phase timer: skin / VB write / plane recompute, measured rather than estimated. `vb_skipped`
// counts animates where the GPU path made the write unnecessary, which is the number that justifies
// (or indicts) the GPU skinning work.
static uint32 g_stencil_shadow_animate_count = 0;
static uint32 g_stencil_shadow_animate_vb_skipped = 0;
static int64 g_stencil_shadow_animate_skin_ticks = 0;
static int64 g_stencil_shadow_animate_vb_ticks = 0;
static int64 g_stencil_shadow_animate_plane_ticks = 0;

// Lookup-outcome tally, reported once per map. Index = the mirror's miss stage (0 = hit, 1..6 = its
// stages), 7 = the palette layout was required and unusable: no region from the caller, an
// empty/absent region header, or the header's count disagreeing with the section's cached node_map,
// which means LOD divergence.
static bool g_stencil_shadow_pool_tally_reported = false;
static uint32 g_stencil_shadow_pool_tally[8] = {};

/* prototypes */

// FOUR PLANES PER ITERATION, LANE-PARALLEL. Do not replace this with `cross_product3d`: that helper
// is the shuffle-based SSE cross product, and for a single one-off call it spends more work
// marshalling three scalars into an __m128, shuffling and extracting the result than the arithmetic
// costs - once per plane per articulated section per frame, which profiled as this function's hotspot.
//
// The output layout is the reason this works: `planes_soa` stores 4-plane blocks as
// [nx x4][ny x4][nz x4][d x4], exactly the shape lane-parallel SIMD produces with no shuffles at all.
// Same expressions per lane as the scalar tail below, in the same order, so the facing sign test sees
// identical values.
//
// _mm_storeu on purpose: `planes_soa` comes from `new real32[]`, 8-byte aligned on x86, and the facing
// test already reads these blocks with _mm_loadu_ps.
static void stencil_shadow_compute_planes(
	s_stencil_shadow_section const* shadow);

/* public code */

bool stencil_shadow_pool_resolve(
	datum object_index,
	const render_model_definition* render_model,
	int32 region_index,
	const s_stencil_shadow_section* shadow,
	s_stencil_shadow_pool_ref* out_ref)
{
	out_ref->pool = NULL;
	out_ref->base_index = 0;
	out_ref->matrix_count = 0;
	out_ref->palette = false;
	if (!render_model)
	{
		return false;
	}

	int32 miss_stage = 0;
	int32 pool_matrix_count = 0;
	void* pool = render_object_cache_get_skinning_pool(object_index, &pool_matrix_count, &miss_stage);
	bool usable = false;
	if (pool)
	{
		if ((render_model->flags & _render_model_definition_force_node_maps) == 0)
		{
			// per-node layout: [0, matrix_count) indexed by MODEL node. Verified in the builder but
			// measured absent from Vista content; kept for correctness on any content without the flag.
			out_ref->pool = pool;
			out_ref->matrix_count = pool_matrix_count;
			usable = true;
		}
		else
		{
			// palette layout. Region header: {int16 first, uint16 count} at pool + 4 + 4*region;
			// region count word at pool + 2. All from render_model_build_skinning (halo2.exe 0x77DEBD).
			const int32 pool_region_count = (int32)*((uint16*)pool + 1);
			if (VALID_INDEX(region_index, pool_region_count)
				&& shadow && shadow->pool_node_map)
			{
				const int32 first = (int32)*((int16*)pool + 2 + 2 * region_index);
				const int32 count = (int32)*((uint16*)pool + 3 + 2 * region_index);
				// The count matching our cached node_map is the guard that the palette was built
				// from the SAME section we cached - the engine palettes the section IT selected
				// for the region, and on an LOD-divergent frame that is a different section whose
				// palette order would silently mis-bind every vertex.
				if (count > 0 && first >= 0 && count == shadow->pool_node_map_count)
				{
					out_ref->pool = pool;
					out_ref->base_index = first;
					out_ref->matrix_count = count;
					out_ref->palette = true;
					usable = true;
				}
			}
			if (!usable)
			{
				miss_stage = 7;
			}
		}
	}
	g_stencil_shadow_pool_tally[usable ? 0 : (miss_stage & 7)]++;

	if (!g_stencil_shadow_pool_tally_reported)
	{
		uint32 total = 0;
		for (int32 stage = 0; stage < 8; stage++)
		{
			total += g_stencil_shadow_pool_tally[stage];
		}
		if (total >= 600)
		{
			g_stencil_shadow_pool_tally_reported = true;
			event(_event_status, "rasterizer:dx9:stencil:pool: lookups=%u hit=%u no_entry=%u datum_fail=%u owner=%u no_offset=%u stale_stamp=%u pool_fail=%u palette_miss=%u (hit=0 means the adoption is INACTIVE and every caster is on the fallback; the dominant miss stage says which check to investigate)",
				total,
				g_stencil_shadow_pool_tally[0], g_stencil_shadow_pool_tally[1],
				g_stencil_shadow_pool_tally[2], g_stencil_shadow_pool_tally[3],
				g_stencil_shadow_pool_tally[4], g_stencil_shadow_pool_tally[5],
				g_stencil_shadow_pool_tally[6], g_stencil_shadow_pool_tally[7]);
		}
	}
	return usable;
}

int32 stencil_shadow_pool_build_palette(
	datum object_index,
	const render_model_definition* render_model,
	int32 region_index,
	const s_stencil_shadow_section* shadow,
	real_vector4d* out_rows)
{
	if (!shadow || !shadow->pool_node_map || shadow->pool_node_map_count <= 0
		|| shadow->pool_node_map_count > k_stencil_shadow_max_palette_nodes || !render_model)
	{
		return 0;
	}
	const int32 count = shadow->pool_node_map_count;

	// pool first: the palette is stored contiguously in exactly the layout c50 wants.
	s_stencil_shadow_pool_ref pool_ref = {};
	if (stencil_shadow_pool_resolve(object_index, render_model, region_index, shadow, &pool_ref)
		&& pool_ref.palette && pool_ref.matrix_count == count)
	{
		memcpy(out_rows, (const uint8*)pool_ref.pool + k_skinning_pool_header_size
			+ k_skinning_pool_matrix_size * pool_ref.base_index,
			(size_t)count * k_skinning_pool_matrix_size);
		return count;
	}

	// composed fallback - the pre-655 product per mapped node, packed into the pool's own row
	// convention (row r = {f[r]*s, l[r]*s, u[r]*s, pos[r]}; the same packing
	// stencil_shadow_set_node_constants uses and 0x77DD88 writes).
	for (int32 local = 0; local < count; local++)
	{
		const uint8 model_node = shadow->pool_node_map[local];
		if ((int32)model_node >= render_model->nodes.count)
		{
			return 0;	// map names a node the model does not have - cannot pose this section
		}
		const real_matrix4x3* world = object_get_node_matrix(object_index, (int16)model_node);
		if (!world)
		{
			return 0;
		}
		real_matrix4x3 interpolated;
		if (halo_interpolator_interpolate_object_node_matrix(
			object_index, (int16)model_node, &interpolated))
		{
			world = &interpolated;
		}
		real_matrix4x3 composed;
		matrix4x3_multiply(world, &render_model->nodes[model_node]->default_inverse_matrix, &composed);

		real_vector4d* rows = &out_rows[local * 3];
		for (int32 row = 0; row < 3; row++)
		{
			rows[row].i = composed.vectors.forward.n[row] * composed.scale;
			rows[row].j = composed.vectors.left.n[row] * composed.scale;
			rows[row].k = composed.vectors.up.n[row] * composed.scale;
			rows[row].l = composed.position.n[row];
		}
	}
	return count;
}

// Translate a MODEL node index into a pool matrix slot under either layout, or NONE.
// Palette layout scans the section's cached node_map for the model node; the map is short (9 on the
// measured biped section) and each node resolves once per animate through composed_cache, so the
// scan cost is nil.
int32 stencil_shadow_pool_slot_for_node(
	const s_stencil_shadow_pool_ref* pool_ref,
	const s_stencil_shadow_section* shadow,
	int32 model_node)
{
	if (!pool_ref->palette)
	{
		return VALID_INDEX(model_node, pool_ref->matrix_count) ? model_node : NONE;
	}
	for (int32 local = 0; local < shadow->pool_node_map_count; local++)
	{
		if ((int32)shadow->pool_node_map[local] == model_node)
		{
			return pool_ref->base_index + local;
		}
	}
	return NONE;
}

void stencil_shadow_pool_parity_probe(
	datum object_index,
	int32 node,
	const render_model_definition* render_model,
	const real_matrix4x3* pool_matrix)
{
	if (g_stencil_shadow_pool_probe_reported
		|| !render_model || !VALID_INDEX(node, render_model->nodes.count))
	{
		// nodes >= nodes.count are COMPOUND slots - the old composition has no equivalent to
		// compare against, which is the point of adopting the pool for them. Not an error.
		return;
	}

	// The exact composition the pre-pool code used: interpolated node_world x inverse_bind.
	const real_matrix4x3* world = object_get_node_matrix(object_index, (int16)node);
	if (!world)
	{
		return;
	}
	real_matrix4x3 interpolated;
	if (halo_interpolator_interpolate_object_node_matrix(object_index, (int16)node, &interpolated))
	{
		world = &interpolated;
		g_stencil_shadow_pool_probe_interp_ran++;
	}
	real_matrix4x3 composed;
	matrix4x3_multiply(world, &render_model->nodes[node]->default_inverse_matrix, &composed);

	// The pool folds scale into the basis and reports scale = 1 (verified at the accessor,
	// 0x77E337), so compare against our basis x our scale. Componentwise max, kept across samples.
	for (int32 row = 0; row < 3; row++)
	{
		const real32 basis_delta_f = fabsf(pool_matrix->vectors.forward.n[row]
			- composed.vectors.forward.n[row] * composed.scale);
		const real32 basis_delta_l = fabsf(pool_matrix->vectors.left.n[row]
			- composed.vectors.left.n[row] * composed.scale);
		const real32 basis_delta_u = fabsf(pool_matrix->vectors.up.n[row]
			- composed.vectors.up.n[row] * composed.scale);
		g_stencil_shadow_pool_probe_max_basis = MAX(g_stencil_shadow_pool_probe_max_basis,
			MAX(basis_delta_f, MAX(basis_delta_l, basis_delta_u)));
		const real32 position_delta = fabsf(pool_matrix->position.n[row] - composed.position.n[row]);
		g_stencil_shadow_pool_probe_max_pos = MAX(g_stencil_shadow_pool_probe_max_pos, position_delta);
	}

	if (++g_stencil_shadow_pool_probe_samples >= 240)
	{
		g_stencil_shadow_pool_probe_reported = true;
		event(_event_status, "rasterizer:dx9:stencil:pool: probe samples=%u interp_ran=%u max_basis_delta=%.5f max_pos_delta=%.5fwu (~0 = pool == our composition; SMALL = the render-time correction, expected; LARGE rotation on a plain object = pool layout wrong, set k_stencil_shadow_use_skinning_pool=false)",
			g_stencil_shadow_pool_probe_samples,
			g_stencil_shadow_pool_probe_interp_ran,
			g_stencil_shadow_pool_probe_max_basis,
			g_stencil_shadow_pool_probe_max_pos);
	}
}

// Tag debug's soft-group recompute. The blend is FULL-WEIGHT, not dominant-node: the weighted branch
// below mirrors section_skin_from_rigid_point_groups, and snapping to the dominant node instead tore
// silhouettes exactly at the joints, which is where the silhouette edge usually lives.
bool stencil_shadow_section_animate(
	s_stencil_shadow_section* shadow,
	datum object_index,
	const render_model_definition* render_model,
	int32 region_index)
{
	// render_scene runs several times per frame - skip the re-skin when this object's pose
	// is already current (re-animates whenever a different object of the same model draws)
	uint32 frame = *global_frame_index_get();
	if (shadow->last_animated_object == object_index && shadow->last_animated_frame == frame)
	{
		return true;
	}

	// SKINNING MATRIX = node_world x INVERSE BIND, never node_world alone.
	// `render_model_build_skinning` (td 0xA68D0) builds every entry of the engine's skinning pool as
	// exactly this product, against a field that Vista's own
	// `lightmap_raycast_resolve_object_hit` (halo2.exe 0x4B2CD4) confirms is
	// `render_model_node::default_inverse_matrix`.
	//
	// Omitting it presents as a SCALE bug rather than a skinning bug: a bind-pose vertex already sits
	// at its bone's bind position, so transforming it by node_world alone applies that bone's whole
	// parent chain translation a second time. Bone translations point outward from the pelvis along
	// each limb, so every vertex is displaced radially outward roughly in proportion to its distance
	// from the root - on a biped the volume reads as a starfish, with the torso's overlapping sheets
	// cancelling to an unshadowed hole. The error is exactly zero at bind pose, so static props look
	// fine and only animated casters show it.
	//
	// `matrix4x3_multiply` leaves the product's scale in the SCALE FIELD rather than in the basis, so
	// the composed matrix is consumed exactly like a raw one and the `* m->scale` terms below stay.
	static real_matrix4x3 composed_cache[k_stencil_shadow_node_index_count];	// node_world x inverse_bind, per node
	const real_matrix4x3* node_matrix_cache[k_stencil_shadow_node_index_count] = {};	// doubles as the validity flag

	// The engine's skinning pool, resolved once per animate. When valid, every entry is the same
	// `interpolated_node_world x inverse_bind` product the lambda below composes, built once by
	// `render_model_build_skinning` for the DRAWN model, plus the render-time corrections our own
	// composition cannot reproduce. On Vista content the layout is per-REGION palettes in local
	// node_map order, which is why the region index travels into this function. An unusable pool
	// falls through to the composition path unchanged.
	s_stencil_shadow_pool_ref pool_ref = {};
	if (k_stencil_shadow_use_skinning_pool)
	{
		stencil_shadow_pool_resolve(object_index, render_model, region_index, shadow, &pool_ref);
	}

	// ONE lookup shared by BOTH the weighted and the dominant-node branch. Each branch used to
	// carry its own copy of this, which is precisely how a transform fix can land in one and not
	// the other - presenting as "only some articulated casters are wrong". Keep it shared.
	auto get_node_matrix = [&](uint8 node) -> const real_matrix4x3*
	{
		if (node_matrix_cache[node])
		{
			return node_matrix_cache[node];
		}

		// Pool first. The slot translation handles both layouts; NONE means this node is not in the
		// section's palette, and the composition below then serves it under its own nodes.count bound.
		if (pool_ref.pool)
		{
			const int32 pool_slot = stencil_shadow_pool_slot_for_node(&pool_ref, shadow, (int32)node);
			if (pool_slot != NONE)
			{
				model_skinning_get_node_matrix(pool_ref.pool, (int16)pool_slot, (real32*)&composed_cache[node]);
				stencil_shadow_pool_parity_probe(object_index, (int32)node, render_model, &composed_cache[node]);
				node_matrix_cache[node] = &composed_cache[node];
				return &composed_cache[node];
			}
		}

		const real_matrix4x3* world = object_get_node_matrix(object_index, node);
		if (!world || !render_model || (int32)node >= render_model->nodes.count)
		{
			if (!g_stencil_shadow_warned_no_bind)
			{
				g_stencil_shadow_warned_no_bind = true;
				event(_event_warning, "rasterizer:dx9:stencil:skinning: no bind matrix for node %d (nodes=%d, world=%d) - articulated section dropped",
					(int32)node, render_model ? render_model->nodes.count : NONE, world ? 1 : 0);
			}
			return NULL;
		}
		// ADOPT THE INTERPOLATED (RENDER) POSE. The engine renders objects from interpolated node
		// matrices - render_objects.cpp tries `halo_interpolator_interpolate_object_node_matrices`
		// first and only falls back to the raw array - so building volumes from the raw tick pose
		// generates the shadow from a different pose than the model that is drawn. Measured on a
		// moving caster that was 0.016 wu of position lag and 11.4 degrees of orientation lag, which
		// on a 0.705 wu biped is a visible limb offset.
		//
		// When the interpolator declines - the object cannot interpolate, or previous->target exceeded
		// the teleport guard - we keep the raw matrix, exactly as
		// `object_try_get_node_matrix_interpolated` does. Doing it here rather than through that
		// wrapper keeps the NULL/bounds check on `world` above; the wrapper dereferences
		// `object_get_node_matrix` unguarded in its fallback path. `interpolated` is a stack local,
		// but `world` is consumed by the multiply on the very next line and never escapes this lambda.
		//
		// Still divergent, deliberately: the engine also composes render-only nodes and eye tracking
		// via `object_compute_render_time_node_matrices` (halo2.exe 0x53599B) before skinning, which we skip
		// unless the pose comes from the skinning pool above. Tag debug has neither divergence,
		// because its shadow and its model share one skinning pool.
		real_matrix4x3 interpolated;
		if (halo_interpolator_interpolate_object_node_matrix(object_index, (int16)node, &interpolated))
		{
			world = &interpolated;
		}

		matrix4x3_multiply(world, &render_model->nodes[node]->default_inverse_matrix,
			&composed_cache[node]);
		node_matrix_cache[node] = &composed_cache[node];
		return &composed_cache[node];
	};

	LARGE_INTEGER animate_t0, animate_t1, animate_t2, animate_t3;
	QueryPerformanceCounter(&animate_t0);

	for (uint32 welded_index = 0; welded_index < shadow->welded_vertex_count; welded_index++)
	{
		const real_point3d* base = &shadow->base_positions[welded_index];
		real_point3d* world = &shadow->world_positions[welded_index];

		if (shadow->vertex_bone_weights)
		{
			// Tag debug's blend, section_skin_from_rigid_point_groups (td 0x19EAF0):
			//   P' = sum over bones of w_i * (P . M_i)
			// Dominant-node snapping tore silhouettes exactly at the joints, which is where the
			// silhouette edge usually lives.
			const uint8* bone_indices = &shadow->vertex_bone_indices[welded_index * 4];
			const real32* bone_weights = &shadow->vertex_bone_weights[welded_index * 4];
			real_point3d position = { 0.f, 0.f, 0.f };

			for (int32 bone = 0; bone < 4; bone++)
			{
				real32 weight = bone_weights[bone];
				if (weight <= 0.f)
				{
					continue;
				}
				uint8 node = bone_indices[bone];
				const real_matrix4x3* m = get_node_matrix(node);
				if (!m)
				{
					return false;
				}

				real_point3d temp;
				matrix4x3_transform_point(m, base, &temp);

				position.x += weight * temp.x;
				position.y += weight * temp.y;
				position.z += weight * temp.z;
			}

			world->x = position.x;
			world->y = position.y;
			world->z = position.z;
			continue;
		}

		// no weight payload (rigid_boned, or a position format without skinning): single bone.
		// Same composed matrix as the weighted branch above - a bind-pose vertex needs its bind
		// transform undone whether one bone moves it or four.
		uint8 node = shadow->vertex_nodes[welded_index];
		const real_matrix4x3* m = get_node_matrix(node);
		if (!m)
		{
			return false;
		}

		matrix4x3_transform_point(m, base, world);
	}

	QueryPerformanceCounter(&animate_t1);

	// When the GPU-skinned draw will pose this section - static skinned VB built and the skinned shader
	// pair present - the dynamic VB refresh is dead weight and is skipped. The SKIN ABOVE STAYS either
	// way: the plane recompute below needs world positions and the facing test needs the planes, so GPU
	// skinning moves the vertices, not the silhouette decision.
	const bool gpu_vb_serves = shadow->skinned_vb && stencil_shadow_skinned_ready();
	if (!gpu_vb_serves)
	{
		s_stencil_shadow_vertex* vb_data = NULL;
		if (!shadow->shadow_vb)
		{
			return false;
		}
		HRESULT hr;
		rasterizer_dx9_log_hr(
			hr,
			shadow->shadow_vb->Lock(0, 0, (void**)&vb_data, D3DLOCK_DISCARD)
		);
		if (FAILED(hr))
		{
			return false;
		}
		for (uint32 welded_index = 0; welded_index < shadow->welded_vertex_count; welded_index++)
		{
			vb_data[welded_index * 2].position = shadow->world_positions[welded_index];
			vb_data[welded_index * 2].extrude = 0.f;
			vb_data[welded_index * 2 + 1].position = shadow->world_positions[welded_index];
			vb_data[welded_index * 2 + 1].extrude = 1.f;
		}
		shadow->shadow_vb->Unlock();
	}
	else
	{
		g_stencil_shadow_animate_vb_skipped++;
	}

	QueryPerformanceCounter(&animate_t2);

	// Plane recompute from world positions, same math as the build. It writes the SoA lanes directly:
	// `planes_soa` is the only plane array the facing test reads, so building AoS and then transposing
	// walked plane_count twice to produce one consumed result. Lane addressing matches
	// stencil_shadow_planes_fill_soa, and the pad lanes of a partial tail block keep the zeros the
	// build-time fill gave them.
	stencil_shadow_compute_planes(shadow);

	QueryPerformanceCounter(&animate_t3);
	g_stencil_shadow_animate_skin_ticks += animate_t1.QuadPart - animate_t0.QuadPart;
	g_stencil_shadow_animate_vb_ticks += animate_t2.QuadPart - animate_t1.QuadPart;
	g_stencil_shadow_animate_plane_ticks += animate_t3.QuadPart - animate_t2.QuadPart;
	if ((++g_stencil_shadow_animate_count % 600) == 0)
	{
		LARGE_INTEGER frequency;
		QueryPerformanceFrequency(&frequency);
		const real64 to_microseconds = 1000000.0 / (real64)frequency.QuadPart;
		const real64 inv_count = 1.0 / (real64)g_stencil_shadow_animate_count;

		UNREFERENCED_PARAMETER(to_microseconds);
		UNREFERENCED_PARAMETER(inv_count);
		event(_event_status, "rasterizer:dx9:stencil:skinning: animates=%u vb_skipped=%u avg_us skin=%.1f vb=%.1f planes=%.1f (vb should be ~0 with vb_skipped == animates)",
			g_stencil_shadow_animate_count, g_stencil_shadow_animate_vb_skipped,
			(real64)g_stencil_shadow_animate_skin_ticks * to_microseconds * inv_count,
			(real64)g_stencil_shadow_animate_vb_ticks * to_microseconds * inv_count,
			(real64)g_stencil_shadow_animate_plane_ticks * to_microseconds * inv_count);
	}

	shadow->last_animated_object = object_index;
	shadow->last_animated_frame = frame;
	return true;
}

void stencil_shadow_skinning_reset_diagnostics(void)
{
	g_stencil_shadow_warned_no_bind = false;
	g_stencil_shadow_pool_probe_reported = false;
	g_stencil_shadow_pool_probe_samples = 0;
	g_stencil_shadow_pool_probe_interp_ran = 0;
	g_stencil_shadow_pool_probe_max_basis = 0.f;
	g_stencil_shadow_pool_probe_max_pos = 0.f;
	g_stencil_shadow_pool_tally_reported = false;
	memset(g_stencil_shadow_pool_tally, 0, sizeof(g_stencil_shadow_pool_tally));
	g_stencil_shadow_animate_count = 0;
	g_stencil_shadow_animate_vb_skipped = 0;
	g_stencil_shadow_animate_skin_ticks = 0;
	g_stencil_shadow_animate_vb_ticks = 0;
	g_stencil_shadow_animate_plane_ticks = 0;
}

/* private code */

static void stencil_shadow_compute_planes(
	s_stencil_shadow_section const* shadow)
{
	const uint32 plane_count = shadow->plane_count;
	const uint32 block_count = plane_count / 4;

	for (uint32 block = 0; block < block_count; block++)
	{
		// gather: 4 triangles x 3 vertices, component-planar
		real32 p0x[4], p0y[4], p0z[4];
		real32 p1x[4], p1y[4], p1z[4];
		real32 p2x[4], p2y[4], p2z[4];
		const uint16* tri = &shadow->triangles[block * 4 * 3];
		for (uint32 lane = 0; lane < 4; lane++, tri += 3)
		{
			const real_point3d* p0 = &shadow->world_positions[tri[0]];
			const real_point3d* p1 = &shadow->world_positions[tri[1]];
			const real_point3d* p2 = &shadow->world_positions[tri[2]];
			p0x[lane] = p0->x; p0y[lane] = p0->y; p0z[lane] = p0->z;
			p1x[lane] = p1->x; p1y[lane] = p1->y; p1z[lane] = p1->z;
			p2x[lane] = p2->x; p2y[lane] = p2->y; p2z[lane] = p2->z;
		}

		const __m128 ax = _mm_loadu_ps(p0x), ay = _mm_loadu_ps(p0y), az = _mm_loadu_ps(p0z);

		// edge_1 = p0 - p1, edge_2 = p0 - p2 - the build-time convention (and the pre-helper
		// inline's), normals matching bit-for-bit per lane
		const __m128 e1x = _mm_sub_ps(ax, _mm_loadu_ps(p1x));
		const __m128 e1y = _mm_sub_ps(ay, _mm_loadu_ps(p1y));
		const __m128 e1z = _mm_sub_ps(az, _mm_loadu_ps(p1z));
		const __m128 e2x = _mm_sub_ps(ax, _mm_loadu_ps(p2x));
		const __m128 e2y = _mm_sub_ps(ay, _mm_loadu_ps(p2y));
		const __m128 e2z = _mm_sub_ps(az, _mm_loadu_ps(p2z));

		// n = cross(edge_1, edge_2), d = dot(n, p0) - four lanes at a time, zero shuffles
		const __m128 nx = _mm_sub_ps(_mm_mul_ps(e1y, e2z), _mm_mul_ps(e1z, e2y));
		const __m128 ny = _mm_sub_ps(_mm_mul_ps(e1z, e2x), _mm_mul_ps(e1x, e2z));
		const __m128 nz = _mm_sub_ps(_mm_mul_ps(e1x, e2y), _mm_mul_ps(e1y, e2x));
		const __m128 d = _mm_add_ps(_mm_add_ps(
			_mm_mul_ps(nx, ax), _mm_mul_ps(ny, ay)), _mm_mul_ps(nz, az));

		real32* soa = &shadow->planes_soa[block * 16];
		_mm_storeu_ps(soa, nx);
		_mm_storeu_ps(soa + 4, ny);
		_mm_storeu_ps(soa + 8, nz);
		_mm_storeu_ps(soa + 12, d);

		// Off by default. When on, AoS is kept in step with SoA exactly as before - note this
		// leaves WORLD-space planes in an array stencil_shadow_section_validate reads as though
		// they were bind-pose, which is why the default is the quiet one.
		if constexpr (k_stencil_shadow_write_aos_planes)
		{
			for (uint32 lane = 0; lane < 4; lane++)
			{
				real_plane3d* plane = &shadow->planes[block * 4 + lane];
				plane->n.i = soa[lane];
				plane->n.j = soa[4 + lane];
				plane->n.k = soa[8 + lane];
				plane->d = soa[12 + lane];
			}
		}
	}

	// scalar tail (0-3 planes) - writes only the real lanes, so the pad lanes of the final partial
	// block keep the zeros the build-time fill gave them
	for (uint32 triangle_index = block_count * 4; triangle_index < plane_count; triangle_index++)
	{
		const uint16* triangle = &shadow->triangles[triangle_index * 3];
		const real_point3d* p0 = &shadow->world_positions[triangle[0]];
		const real_point3d* p1 = &shadow->world_positions[triangle[1]];
		const real_point3d* p2 = &shadow->world_positions[triangle[2]];

		const real32 e1x = p0->x - p1->x, e1y = p0->y - p1->y, e1z = p0->z - p1->z;
		const real32 e2x = p0->x - p2->x, e2y = p0->y - p2->y, e2z = p0->z - p2->z;

		const real32 n_i = e1y * e2z - e1z * e2y;
		const real32 n_j = e1z * e2x - e1x * e2z;
		const real32 n_k = e1x * e2y - e1y * e2x;
		const real32 d = n_i * p0->x + n_j * p0->y + n_k * p0->z;

		real32* soa = &shadow->planes_soa[(triangle_index >> 2) * 16];
		const uint32 lane = triangle_index & 3;
		soa[lane] = n_i;
		soa[4 + lane] = n_j;
		soa[8 + lane] = n_k;
		soa[12 + lane] = d;

		if constexpr (k_stencil_shadow_write_aos_planes)
		{
			real_plane3d* plane = &shadow->planes[triangle_index];
			plane->n.i = n_i;
			plane->n.j = n_j;
			plane->n.k = n_k;
			plane->d = d;
		}
	}
}
