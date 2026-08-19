#include "stdafx.h"
#include "rasterizer_dx9_stencil_shadow_skinning.h"

#include "rasterizer_dx9_stencil_shadow_tunables.h"
#include "math/matrix_math.h"			// matrix4x3_multiply — skinning = node_world x inverse_bind
#include "main/interpolator.h"			// halo_interpolator_interpolate_object_node_matrix
#include "objects/objects.h"
#include "models/render_model_definitions.h"
#include "render/render.h"				// global_frame_index_get — the per-frame animate guard
#include "render/render_lod_new.h"		// it. 655 — the skinning pool mirror + node accessor
#include "H2MOD/Modules/h2log/h2log.h"

/* prototypes */

static void stencil_shadow_compute_planes(s_stencil_shadow_section const* shadow);

/* globals */

// Per-map one-shot latches owned by this module — reset by stencil_shadow_skinning_reset_diagnostics,
// which stencil_shadow_cache_clear calls. See the it. 310/330 rule in the rasterizer file: a latch
// that reports once MUST be file scope or it describes only the first map of a session.
static bool g_stencil_shadow_warned_no_bind = false;
static bool g_stencil_shadow_probed_interpolation = false;
// it. 471: the interp probe samples ACROSS FRAMES rather than once — see the probe for why a
// single sample cannot distinguish "no lag" from "sampled at the wrong instant".
static uint32 g_stencil_shadow_interp_samples = 0;
static uint32 g_stencil_shadow_interp_ran = 0;
static real32 g_stencil_shadow_interp_max_pos = 0.f;
static real32 g_stencil_shadow_interp_max_fwd = 0.f;

// it. 655 — pool parity probe accumulators. Same across-frames-keep-the-max design as the interp
// probe above, for the same reason (it. 471): a single sample cannot distinguish "matches" from
// "sampled when the correction happened to be zero".
static bool g_stencil_shadow_pool_probe_reported = false;
static uint32 g_stencil_shadow_pool_probe_samples = 0;
static uint32 g_stencil_shadow_pool_probe_interp_ran = 0;
static real32 g_stencil_shadow_pool_probe_max_basis = 0.f;
static real32 g_stencil_shadow_pool_probe_max_pos = 0.f;

// it. 660 — the per-phase timer the it. 553/556 rule demanded before optimising: skin / VB write /
// plane recompute, measured, not estimated. `vb_skipped` counts animates where the GPU path made
// the write unnecessary — the number that justifies (or indicts) the GPU skinning work.
static uint32 g_stencil_shadow_animate_count = 0;
static uint32 g_stencil_shadow_animate_vb_skipped = 0;
static int64 g_stencil_shadow_animate_skin_ticks = 0;
static int64 g_stencil_shadow_animate_vb_ticks = 0;
static int64 g_stencil_shadow_animate_plane_ticks = 0;

// it. 656 — lookup-outcome tally. Index = the mirror's miss stage (0 = hit, 1..6 = its stages),
// 7 = the palette layout was required and unusable (no region from the caller, empty/absent region
// header, or the header's count disagreeing with the section's cached node_map — LOD divergence).
// Reported once per map when enough lookups have happened.
//
// (it. 657's poolforce probe lived here and is gone: its question is ANSWERED — flags=0xc,
// pool word+0 == 1 on 6-node models. Vista content is 100% force_node_maps and the pool is
// [matrix 0][per-region palettes]. The resolve below is built on that measurement.)
static bool g_stencil_shadow_pool_tally_reported = false;
static uint32 g_stencil_shadow_pool_tally[8] = {};

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
			// per-node layout: [0, matrix_count) indexed by MODEL node. Verified in the builder
			// (it. 653) but measured ABSENT from Vista content (it. 657) — kept for correctness on
			// any content without the flag.
			out_ref->pool = pool;
			out_ref->matrix_count = pool_matrix_count;
			usable = true;
		}
		else
		{
			// palette layout. Region header: {int16 first, uint16 count} at pool + 4 + 4*region;
			// region count word at pool + 2. All from render_model_build_skinning (0x77DEBD).
			const int32 pool_region_count = (int32)*((uint16*)pool + 1);
			if (region_index >= 0 && region_index < pool_region_count
				&& shadow && shadow->pool_node_map)
			{
				const int32 first = (int32)*((int16*)pool + 2 + 2 * region_index);
				const int32 count = (int32)*((uint16*)pool + 3 + 2 * region_index);
				// The count matching our cached node_map is the guard that the palette was built
				// from the SAME section we cached — the engine palettes the section IT selected
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
#ifdef LOG_STENCIL
			LOG_INFO_GAME("stencil pool: lookups={} hit={} no_entry={} datum_fail={} owner={} no_offset={} stale_stamp={} pool_fail={} palette_miss={} (it. 656/658 — hit=0 means the adoption is INACTIVE and every caster is on the fallback; the dominant miss stage says which check to investigate)",
				total,
				g_stencil_shadow_pool_tally[0], g_stencil_shadow_pool_tally[1],
				g_stencil_shadow_pool_tally[2], g_stencil_shadow_pool_tally[3],
				g_stencil_shadow_pool_tally[4], g_stencil_shadow_pool_tally[5],
				g_stencil_shadow_pool_tally[6], g_stencil_shadow_pool_tally[7]);
#endif
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
		|| shadow->pool_node_map_count > 67 || !render_model)
	{
		return 0;
	}
	const int32 count = shadow->pool_node_map_count;

	// pool first: the palette is stored contiguously in exactly the layout c50 wants.
	s_stencil_shadow_pool_ref pool_ref = {};
	if (stencil_shadow_pool_resolve(object_index, render_model, region_index, shadow, &pool_ref)
		&& pool_ref.palette && pool_ref.matrix_count == count)
	{
		memcpy(out_rows, (const uint8*)pool_ref.pool + 68 + 48 * pool_ref.base_index,
			(size_t)count * 48);
		return count;
	}

	// composed fallback — the pre-655 product per mapped node, packed into the pool's own row
	// convention (row r = {f[r]*s, l[r]*s, u[r]*s, pos[r]}; the same packing
	// stencil_shadow_set_node_constants uses and 0x77DD88 writes).
	for (int32 local = 0; local < count; local++)
	{
		const uint8 model_node = shadow->pool_node_map[local];
		if ((int32)model_node >= render_model->nodes.count)
		{
			return 0;	// map names a node the model does not have — cannot pose this section
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

// it. 658 — translate a MODEL node index into a pool matrix slot under either layout, or NONE.
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
		return (model_node >= 0 && model_node < pool_ref->matrix_count) ? model_node : NONE;
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
		|| !render_model || node < 0 || node >= render_model->nodes.count)
	{
		// nodes >= nodes.count are COMPOUND slots — the old composition has no equivalent to
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
		if (basis_delta_f > g_stencil_shadow_pool_probe_max_basis) { g_stencil_shadow_pool_probe_max_basis = basis_delta_f; }
		if (basis_delta_l > g_stencil_shadow_pool_probe_max_basis) { g_stencil_shadow_pool_probe_max_basis = basis_delta_l; }
		if (basis_delta_u > g_stencil_shadow_pool_probe_max_basis) { g_stencil_shadow_pool_probe_max_basis = basis_delta_u; }
		const real32 position_delta = fabsf(pool_matrix->position.n[row] - composed.position.n[row]);
		if (position_delta > g_stencil_shadow_pool_probe_max_pos) { g_stencil_shadow_pool_probe_max_pos = position_delta; }
	}

	if (++g_stencil_shadow_pool_probe_samples >= 240)
	{
		g_stencil_shadow_pool_probe_reported = true;

#ifdef LOG_STENCIL
		LOG_INFO_GAME("stencil poolprobe: samples={} interp_ran={} max_basis_delta={:.5f} max_pos_delta={:.5f}wu (it. 655 — ~0 = pool == our composition; SMALL = the it. 487 render-time correction, expected; LARGE rotation on a plain object = pool layout wrong, set k_stencil_shadow_use_skinning_pool=false)",
			g_stencil_shadow_pool_probe_samples,
			g_stencil_shadow_pool_probe_interp_ran,
			g_stencil_shadow_pool_probe_max_basis,
			g_stencil_shadow_pool_probe_max_pos);
#endif
	}
}

// P3: transform an articulated section's welded verts into WORLD space by each vertex's
// node matrix, refresh the doubled dynamic VB, and recompute the facing planes in place —
// tag-debug's soft-group recompute (rasterizer_stencilshadow soft planes). After this the
// section draws with IDENTITY node constants and the facing test takes the WORLD-space light.
//
// SKINNING IS FULL-WEIGHT, NOT DOMINANT-NODE. This comment said "dominant-node" long after
// the weighted blend landed below (the `vertex_bone_weights` branch, td's
// section_skin_from_rigid_point_groups). That matters beyond tidiness: the note that disabled
// cross-section stitching gives "ours cannot until full-weight skinning lands" as the reason,
// so a reader trusting this header would conclude the blocker still stands when it does not.
// See td-isq-generation.md it. 468 -- what still blocks stitching is the removed producer,
// not the skinning.
bool stencil_shadow_section_animate(
	s_stencil_shadow_section* shadow,
	datum object_index,
	const render_model_definition* render_model,
	int32 region_index)
{
	// render_scene runs several times per frame — skip the re-skin when this object's pose
	// is already current (re-animates whenever a different object of the same model draws)
	uint32 frame = *global_frame_index_get();
	if (shadow->last_animated_object == object_index && shadow->last_animated_frame == frame)
	{
		return true;
	}

	// SKINNING MATRIX = node_world x INVERSE BIND, never node_world alone.
	//
	// `render_model_build_skinning` (td 0xA68D0, models\model_skinning.cpp) builds every entry of
	// the engine's skinning pool as exactly this product:
	//     matrix4x3_multiply(object_node_matrix, render_model_node + 68, out);
	//     model_skinning_matrix_from_real_matrix4x3(pool_slot, out);
	// where `render_model_node + 68` is td's inverse bind field == Vista's
	// `render_model_node::default_inverse_matrix`.
	//
	// The field offset is confirmed against RETAIL, not inferred: Vista's
	// `lightmap_raycast_resolve_object_hit` (halo2.exe 0x4B2CD4) indexes the nodes block as
	// `96 * node_index + nodes_base + 40` and hands that straight to
	// `matrix4x3_inverse_transform_point` — stride 96, inverse-bind matrix at +40, which is
	// term-for-term our `render_model_node` layout.
	//
	// Why omitting it looked like a *scale* bug rather than a skinning bug: a bind-pose vertex
	// already sits at its bone's bind position, so transforming it by `node_world` alone applies
	// that bone's whole parent chain translation a SECOND time. Bone translations point outward
	// from the pelvis along each limb, so every vertex is displaced radially outward, roughly in
	// proportion to its distance from the root — indistinguishable at a glance from the model
	// being scaled up about its origin. On a biped it turns limbs into rays: the volume reads as
	// a starfish centred on the object, much larger than the caster, with the torso's overlapping
	// sheets cancelling to an unshadowed hole in the middle. The error is exactly ZERO at bind
	// pose, which is why static props looked fine and only animated casters showed it.
	//
	// `matrix4x3_multiply` (td 0x6F050) leaves the product's scale in the SCALE FIELD
	// (`result->scale = a->scale * b->scale`; basis is the unscaled 3x3 product), so the composed
	// matrix is consumed exactly like a raw one — the `* m->scale` terms below stay. See the
	// scale trap in td-do-not-fix.md before touching them.
	static real_matrix4x3 composed_cache[256];		// node_world x inverse_bind, per node
	const real_matrix4x3* node_matrix_cache[256] = {};	// doubles as the validity flag

	// it. 655/658 — THE ENGINE'S SKINNING POOL, resolved once per animate. When valid, every entry
	// is the same `interpolated_node_world x inverse_bind` product the lambda below composes — built
	// once by `render_model_build_skinning` for the DRAWN model, plus the render-time corrections
	// (render-only nodes, eye tracking) our own composition cannot reproduce. On Vista content the
	// layout is per-REGION palettes in local node_map order (it. 657), which is why the region index
	// travels into this function. An unusable pool falls through to the composition path unchanged;
	// see the tunable for the full contract.
	s_stencil_shadow_pool_ref pool_ref = {};
	if (k_stencil_shadow_use_skinning_pool)
	{
		stencil_shadow_pool_resolve(object_index, render_model, region_index, shadow, &pool_ref);
	}

	// ONE lookup shared by BOTH the weighted and the dominant-node branch. Each branch used to
	// carry its own copy of this, which is precisely how a transform fix can land in one and not
	// the other — presenting as "only some articulated casters are wrong". Keep it shared.
	auto get_node_matrix = [&](uint8 node) -> const real_matrix4x3*
	{
		if (node_matrix_cache[node])
		{
			return node_matrix_cache[node];
		}

		// it. 655/658: pool first. The slot translation handles both layouts — direct model-node
		// indexing on per-node content, node_map scan into the region palette on force content
		// (all of Vista's, it. 657). NONE means this node is not in the section's palette; the
		// composition below then serves it, with its own nodes.count bound.
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
#ifdef LOG_STENCIL
				LOG_INFO_GAME("stencil WARNING: no bind matrix for node {} (nodes={}, world={}) — articulated section dropped",
					(int32)node, render_model ? render_model->nodes.count : -1, world ? 1 : 0);
#endif
			}
			return NULL;
		}
		// it. 431 PROBE — DIAGNOSTIC ONLY. The interpolated matrix is deliberately NOT used;
		// this measures a gap, it does not close it.
		//
		// it. 419 established that the engine renders objects from INTERPOLATED node matrices
		// (`render_objects.cpp:56` tries `halo_interpolator_interpolate_object_node_matrices`
		// first and only falls back to the raw array), while we build volumes from the RAW tick
		// pose here. So the shadow can be generated from a different pose than the model that is
		// drawn. What nobody has measured is HOW FAR APART the two actually are, and that decides
		// whether the ledger item is worth acting on:
		//
		//   pos_delta ~= 0 and fwd_delta ~= 0  -> interpolation is inactive or irrelevant on this
		//                                         content; it. 419 is a NO-OP and can be closed
		//                                         with no code change at all
		//   either materially non-zero         -> the pose mismatch is real, and its size bounds
		//                                         how visible the lag can be; apply
		//                                         `object_try_get_node_matrix_interpolated`
		//                                         AFTER the geometry fixes are confirmed
		//
		// Measured against the depth/scale references already established: the caster is ~0.705 wu
		// tall (it. 415), so a pos_delta of ~1e-3 wu is negligible and ~1e-2 wu is a visible limb
		// offset.
		//
		// it. 471 REDESIGNED THIS PROBE. The original was ONE-SHOT and compared
		// `object_try_get_node_matrix_interpolated` against `world`. That cannot answer it. 419,
		// because THREE different situations all produce a ~0 delta and the log could not tell
		// them apart:
		//
		//   1. the interpolator DECLINED. `object_try_get_node_matrix_interpolated` falls back to
		//      `*object_get_node_matrix(...)` — literally the same matrix as `world` — whenever
		//      `halo_interpolator_interpolate_object_node_matrix` returns false, which happens if
		//      the object cannot interpolate at all, or if previous->target moved further than
		//      `k_interpolation_distance_cutoff` (900.0, the teleport guard). Delta is then
		//      identically 0 BY CONSTRUCTION and says nothing about interpolation.
		//   2. we sampled at the wrong instant. The interpolator returns
		//      lerp(previous, target, g_interpolator_delta), and `world` is effectively the target,
		//      so the gap is (1 - g_interpolator_delta) * (target - previous). At the end of a tick
		//      that is ~0 even when interpolation is fully active and large mid-tick.
		//   3. the object genuinely is not moving — the only reading that closes it. 419.
		//
		// So: call the interpolator DIRECTLY to capture its bool, and sample ACROSS FRAMES keeping
		// the MAXIMUM, which survives cases 2 and 3. `ran` separates case 1 from the rest.
		//
		// Reading the summary line:
		//   ran=0                      -> interpolation never engaged for this caster; it. 419 is
		//                                 moot HERE, but the reason is the interpolator declining,
		//                                 NOT that poses agree. Do not close the item on this alone.
		//   ran>0 and max_pos ~0       -> interpolation ran and the poses genuinely track. CLOSE it. 419.
		//   ran>0 and max_pos >~1e-2wu -> real pose lag, bounded by max_pos. Apply the interpolated
		//                                 matrix AFTER the geometry fixes are confirmed.
		// Latches reset in stencil_shadow_cache_clear.
		//
		// ===== it. 500: THE PROBE'S ANSWER CAME BACK POSITIVE, AND THE FIX IS NOW APPLIED BELOW ====
		// The 16:37 run measured three samples (it. 498):
		//     ran=220/240  max_pos=0.00009wu  max_fwd=0.052deg   (near-static caster)
		//     ran=200/240  max_pos=0.00017wu  max_fwd=0.077deg   (near-static caster)
		//     ran=240/240  max_pos=0.01631wu  max_fwd=11.436deg  (MOVING caster)
		// `ran > 0` throughout, so the interpolator engaged — not the declined case. The third sample
		// is above this probe's own "~1e-2 wu = a visible limb offset" threshold, on a 0.705 wu caster,
		// with 11.4 degrees of orientation lag. So the branch this comment calls
		// "ran>0 and max_pos >~1e-2wu -> apply the interpolated matrix AFTER the geometry fixes are
		// confirmed" is the branch we are on, and the geometry fixes ARE confirmed: the same run reports
		// `decomp=applied` on 30/30 sections with every extent well under the normalised 2.0 span.
		//
		// The interpolator call is now made UNCONDITIONALLY (it used to run only while probing) and its
		// result is ADOPTED as the pose, below the probe. Order matters: the probe must difference
		// against the RAW `world`, so the adoption happens after the measurement, not before — otherwise
		// the probe would report 0 forever and look like a closed item.
		//
		// The probe keeps running and keeps its meaning inverted-but-useful: it now reports how much
		// correction is being applied rather than how much error is being left.
		//
		// NOT fixed here, and still open: it. 487's second divergence — the engine also composes
		// render-only nodes and eye tracking via `object_compute_render_time_node_matrices` (0x53599B)
		// before skinning, which we still skip. That is a different mechanism from interpolation and was
		// never what this probe measured. See it. 488: td cannot have either divergence because its
		// shadow and its model share one skinning pool.
		real_matrix4x3 interpolated;
		const bool interpolation_ran = halo_interpolator_interpolate_object_node_matrix(
			object_index, (int16)node, &interpolated);
		if (!g_stencil_shadow_probed_interpolation)
		{
			const bool ran = interpolation_ran;
			if (ran)
			{
				g_stencil_shadow_interp_ran++;
				real32 dx = interpolated.position.x - world->position.x;
				real32 dy = interpolated.position.y - world->position.y;
				real32 dz = interpolated.position.z - world->position.z;
				real32 pos_delta = sqrtf(dx * dx + dy * dy + dz * dz);
				real32 cos_theta = interpolated.vectors.forward.i * world->vectors.forward.i
					+ interpolated.vectors.forward.j * world->vectors.forward.j
					+ interpolated.vectors.forward.k * world->vectors.forward.k;
				if (cos_theta > 1.f) { cos_theta = 1.f; }
				if (cos_theta < -1.f) { cos_theta = -1.f; }
				real32 fwd_delta =
					(real32)(acos((real64)cos_theta) * (180.0 / 3.14159265358979323846));
				if (pos_delta > g_stencil_shadow_interp_max_pos) { g_stencil_shadow_interp_max_pos = pos_delta; }
				if (fwd_delta > g_stencil_shadow_interp_max_fwd) { g_stencil_shadow_interp_max_fwd = fwd_delta; }
			}
			// ~4 seconds of samples at 60fps before reporting, so a moving caster is very likely
			// to have been observed mid-tick at least once.
			if (++g_stencil_shadow_interp_samples >= 240)
			{
				g_stencil_shadow_probed_interpolation = true;

#ifdef LOG_STENCIL
				LOG_INFO_GAME("stencil interp probe: samples={} ran={} max_pos_delta={:.5f}wu max_fwd_delta={:.3f}deg (it. 419/471 — ran=0 means the interpolator DECLINED, which is NOT evidence the poses agree)",
					g_stencil_shadow_interp_samples,
					g_stencil_shadow_interp_ran,
					g_stencil_shadow_interp_max_pos,
					g_stencil_shadow_interp_max_fwd);
#endif
			}
		}

		// it. 500: ADOPT the interpolated (render) pose. Placed AFTER the probe on purpose — see the
		// block above. `interpolated` is a stack local, but `world` is consumed by the multiply on the
		// very next line and never escapes this lambda, so pointing at it is safe.
		//
		// When the interpolator declines (object cannot interpolate, or previous->target exceeded
		// `k_interpolation_distance_cutoff` = 900.0, the teleport guard) we keep the raw matrix, which
		// is exactly what `object_try_get_node_matrix_interpolated` does. Doing it here rather than
		// through that wrapper keeps the existing NULL/bounds check on `world` above — the wrapper
		// dereferences `object_get_node_matrix` unguarded in its fallback path.
		if (interpolation_ran)
		{
			world = &interpolated;
		}

		matrix4x3_multiply(world, &render_model->nodes[node]->default_inverse_matrix,
			&composed_cache[node]);
		node_matrix_cache[node] = &composed_cache[node];
		return &composed_cache[node];
	};

	// it. 660 — the per-phase timer (the it. 553/556 prerequisite, folded into the change it gates).
	LARGE_INTEGER animate_t0, animate_t1, animate_t2, animate_t3;
	QueryPerformanceCounter(&animate_t0);

	for (uint32 welded_index = 0; welded_index < shadow->welded_vertex_count; welded_index++)
	{
		const real_point3d* base = &shadow->base_positions[welded_index];
		real_point3d* world = &shadow->world_positions[welded_index];

		if (shadow->vertex_bone_weights)
		{
			// P1-3: td's blend, section_skin_from_rigid_point_groups (td 0x19EAF0):
			//   P' = sum over bones of w_i * (P . M_i)
			// Dominant-node snapping tore silhouettes exactly at the joints, which is where
			// the silhouette edge usually lives.
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
		// Same composed matrix as the weighted branch above — a bind-pose vertex needs its bind
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

	// it. 660 — when the GPU-skinned draw will pose this section (static skinned VB built AND the
	// skinned shader pair exists), the dynamic VB refresh is dead weight and is skipped. The SKIN
	// ABOVE STAYS either way: the plane recompute below needs world positions, and the facing test
	// needs the planes — GPU skinning moves the vertices, not the silhouette decision.
	const bool gpu_vb_serves = shadow->skinned_vb && stencil_shadow_skinned_ready();
	if (!gpu_vb_serves)
	{
		s_stencil_shadow_vertex* vb_data = NULL;
		if (!shadow->shadow_vb
			|| FAILED(shadow->shadow_vb->Lock(0, 0, (void**)&vb_data, D3DLOCK_DISCARD)))
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

	// plane recompute from world positions (same math as build)
	//
	// it. 621 — WRITES THE SoA LANES DIRECTLY, and the separate transpose pass is gone.
	// `planes_soa` is the only plane array the facing test reads, so building AoS and then
	// transposing it walked plane_count twice to produce one consumed result. Lane addressing is
	// identical to stencil_shadow_planes_fill_soa: block = index/4, [nx x4][ny x4][nz x4][d x4].
	// The pad lanes of a partial tail block keep the zeros the build-time fill gave them.
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

#ifdef LOG_STENCIL
		LOG_INFO_GAME("stencil animtime: animates={} vb_skipped={} avg_us skin={:.1f} vb={:.1f} planes={:.1f} (it. 660 — the it. 553/556 measurement; vb should be ~0 with vb_skipped == animates)",
			g_stencil_shadow_animate_count, g_stencil_shadow_animate_vb_skipped,
			(real64)g_stencil_shadow_animate_skin_ticks * to_microseconds * inv_count,
			(real64)g_stencil_shadow_animate_vb_ticks * to_microseconds * inv_count,
			(real64)g_stencil_shadow_animate_plane_ticks * to_microseconds * inv_count);
#endif
	}

	// world-position verification (user-requested): every transformed vertex must stay
	// near its object; an outlier means a wrong node assignment scattered geometry
	// (streak fins). Logged throttled with the offending node.
	//
	// PLUS a measured INFLATION RATIO (it. 319). The outlier threshold below is
	// `radius * 10 + 5` — deliberately loose, so it only catches geometry flung across the map.
	// It therefore could NOT see the bug that actually shipped: the missing inverse-bind term
	// inflated skinned casters by roughly 2x, which is enormous visually (a biped became a
	// starfish) yet nowhere near a 10x threshold. Nothing numeric contradicted the code for
	// several iterations, and the symptom had to be diagnosed from screenshots instead.
	//
	// `max_dist / object.radius` is the sensitive test the loose bound is not. `object.radius` is
	// the engine's own bounding radius for this object, so a correctly skinned section should
	// measure at or a little under 1.0. A ratio near 2 means every vertex is being displaced
	// outward from the root — i.e. the bind transform is being applied twice. This is logged
	// unconditionally (throttled) rather than only on failure, so there is always a baseline to
	// compare against instead of silence meaning "fine".
	//
	// (The `max_dist / object.radius` paragraph above is SUPERSEDED — it. 331 replaced that ratio
	// with world_extent / bind_extent, for the reason set out at the metric itself below. The
	// paragraph is kept because it records why the loose outlier bound could not see the it. 317
	// starfish, which is still the useful part.)
	//
	// it. 621 — OFF BY DEFAULT. Everything below is diagnostic, and the 1-in-300 / 1-in-600
	// throttles gate only the LOGGING: with this on, the outlier sweep runs on EVERY animate and
	// the inflation metric adds two more full sweeps on its every-600th. Flip
	// k_stencil_shadow_verify_animation to bring all of it back unchanged.
	if constexpr (k_stencil_shadow_verify_animation)
	{
		const object_datum* object = object_try_and_get(object_index);
		if (object)
		{
			real32 sane_radius = object->object.radius * 10.f + 5.f;
			uint32 outliers = 0;
			uint8 outlier_node = 0;
			real32 max_dist_sq = 0.f;
			for (uint32 i = 0; i < shadow->welded_vertex_count; i++)
			{
				real32 dx = shadow->world_positions[i].x - object->object.position.x;
				real32 dy = shadow->world_positions[i].y - object->object.position.y;
				real32 dz = shadow->world_positions[i].z - object->object.position.z;
				real32 dist_sq = dx * dx + dy * dy + dz * dz;
				if (dist_sq > max_dist_sq)
				{
					max_dist_sq = dist_sq;
				}
				if (dist_sq > sane_radius * sane_radius)
				{
					if (!outliers)
					{
						outlier_node = shadow->vertex_nodes[i];
					}
					outliers++;
				}
			}
			static uint32 dbg_outlier_log = 0;
			if (outliers && ++dbg_outlier_log % 300 == 1)
			{
#ifdef LOG_STENCIL
				LOG_INFO_GAME("stencil VERTEX OUTLIERS: {}/{} world verts beyond {:.1f}wu of object (first node={})",
					outliers, shadow->welded_vertex_count, sane_radius, outlier_node);
#endif
			}

			// INFLATION RATIO — normalised against the section's own BIND-POSE extent, not
			// against object.radius (corrected it. 331).
			//
			// The first version divided by `object->object.radius` and asserted "expect ~1.0". That
			// was unverified: `radius` is the object's own bounding/collision radius and there is no
			// established relationship between it and a section's vertex spread, so the baseline
			// could sit anywhere. A diagnostic whose "correct" value is unknown is worse than none —
			// it invites reading a healthy 2.0 as the very bug it was written to find.
			//
			// `world_extent / bind_extent` has a baseline that follows from the geometry instead of
			// from a convention: skinning is a rigid-ish rearrangement of the SAME vertices, so an
			// animated pose moves the extent somewhat but cannot double it. Re-applying each bone's
			// bind translation displaces every vertex outward roughly in proportion to its distance
			// from the root, which IS a doubling. Both extents are measured about their own centroid
			// so a translated object reads the same as one at the origin.
			static uint32 dbg_inflation_log = 0;
			if (shadow->welded_vertex_count > 0 && (dbg_inflation_log++ % 600) == 0)
			{
				real_point3d bind_centre = { 0.f, 0.f, 0.f };
				real_point3d world_centre = { 0.f, 0.f, 0.f };
				const real32 inv_count = 1.f / (real32)shadow->welded_vertex_count;
				for (uint32 i = 0; i < shadow->welded_vertex_count; i++)
				{
					bind_centre.x += shadow->base_positions[i].x * inv_count;
					bind_centre.y += shadow->base_positions[i].y * inv_count;
					bind_centre.z += shadow->base_positions[i].z * inv_count;
					world_centre.x += shadow->world_positions[i].x * inv_count;
					world_centre.y += shadow->world_positions[i].y * inv_count;
					world_centre.z += shadow->world_positions[i].z * inv_count;
				}
				real32 bind_max_sq = 0.f, world_max_sq = 0.f;
				for (uint32 i = 0; i < shadow->welded_vertex_count; i++)
				{
					real32 bx = shadow->base_positions[i].x - bind_centre.x;
					real32 by = shadow->base_positions[i].y - bind_centre.y;
					real32 bz = shadow->base_positions[i].z - bind_centre.z;
					real32 b = bx * bx + by * by + bz * bz;
					if (b > bind_max_sq) { bind_max_sq = b; }
					real32 wx = shadow->world_positions[i].x - world_centre.x;
					real32 wy = shadow->world_positions[i].y - world_centre.y;
					real32 wz = shadow->world_positions[i].z - world_centre.z;
					real32 w = wx * wx + wy * wy + wz * wz;
					if (w > world_max_sq) { world_max_sq = w; }
				}
				real32 bind_extent = (real32)sqrt((real64)bind_max_sq);
				real32 world_extent = (real32)sqrt((real64)world_max_sq);
				// it. 473 CORRECTED THIS LINE'S MEANING. It used to read
				// "expect ~1.0; >=1.8 = bind applied twice", which is unfounded — and "applied
				// twice" is precisely the case this metric CANNOT see.
				//
				// `ratio` compares max-distance-from-centroid before and after transforming. A
				// RIGID transform preserves distances up to its scale factor, and the centroid maps
				// to the centroid, so for a single-node section:
				//
				//     ratio == m->scale     EXACTLY, and identically so whether the inverse bind was
				//                           applied once, not at all, or twice
				//
				// because composing rigid transforms is still one rigid transform. Only the SCALE
				// factors differ between those cases, and the inverse binds measured live are
				// orthonormal with scale 1 (it. 413) — so all three read ~1.0. On the static path
				// this number is a tautology, not a confirmation.
				//
				// It is only informative on the SKINNED branch, where vertices receive DIFFERENT
				// matrices and the cloud genuinely deforms. Even there it does not isolate a defect:
				// a biped mid-animation legitimately has a different extent than its bind pose. What
				// it does bound is the starfish — a missing inverse bind scatters vertices by
				// per-node amounts and inflates the extent far beyond any real pose.
				//
				//   branch=dominant-node  -> ratio is m->scale. Carries NO information about the bind.
				//   branch=weighted, ratio ~1-2   -> ordinary animated deformation
				//   branch=weighted, ratio >>2    -> vertices flying apart: the it. 317 starfish
				//
				// Use `stencil bindprobe:` (bind_offset / bind_rot) to ask whether composing the bind
				// MATTERS, and it. 412's bit-exact check for whether it HAPPENS. This line answers
				// neither; it measures how far the volume is spread.

#ifdef LOG_STENCIL
				LOG_INFO_GAME("stencil inflation: obj={} bind_extent={:.3f}wu world_extent={:.3f}wu ratio={:.2f} (it. 473: on branch=dominant-node this is just m->scale and means NOTHING; on branch=weighted >>2 = starfish) verts={} branch={} obj_radius={:.3f} max_from_origin={:.3f}",
					(uint32)object_index, bind_extent, world_extent,
					bind_extent > 0.0001f ? world_extent / bind_extent : -1.f,
					shadow->welded_vertex_count,
					shadow->vertex_bone_weights ? "weighted" : "dominant-node",
					object->object.radius, (real32)sqrt((real64)max_dist_sq));
#endif
			}
		}
	}

	shadow->last_animated_object = object_index;
	shadow->last_animated_frame = frame;
	return true;
}

void stencil_shadow_skinning_reset_diagnostics(void)
{
	g_stencil_shadow_warned_no_bind = false;
	g_stencil_shadow_probed_interpolation = false;
	g_stencil_shadow_interp_samples = 0;
	g_stencil_shadow_interp_ran = 0;
	g_stencil_shadow_interp_max_pos = 0.f;
	g_stencil_shadow_interp_max_fwd = 0.f;
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


// it. 673 — FOUR PLANES PER ITERATION, LANE-PARALLEL. The user's profiler named `cross_product3d`
// as this function's hotspot, and the diagnosis is structural: that helper is the SHUFFLE-BASED SSE
// cross product, which for a single one-off call spends more work marshalling three scalars into an
// __m128, shuffling, and extracting the result back out than the arithmetic itself costs — per
// call, `plane_count` times per articulated section per frame.
//
// The output layout was already the answer: `planes_soa` stores 4-plane blocks as
// [nx x4][ny x4][nz x4][d x4] — exactly the shape lane-parallel SIMD produces with NO shuffles at
// all. So this computes four triangles' planes at once: gather the twelve vertices' components into
// lane arrays (the indirection through `triangles[]` is the gather-bound part it. 553 measured, and
// it is unavoidable either way), then every cross/dot term is one _mm_mul/_mm_sub across the four
// lanes, stored straight into the block. Same expressions per lane as the scalar math — the values
// feed a facing SIGN test, and the per-lane operation order is unchanged.
//
// _mm_storeu on purpose: `planes_soa` comes from `new real32[]` (8-byte aligned on x86), and the
// facing test already reads these blocks with _mm_loadu_ps — unaligned is this array's convention.
static void stencil_shadow_compute_planes(
	s_stencil_shadow_section const* shadow)
{
	const uint32 plane_count = shadow->plane_count;
	const uint32 block_count = plane_count / 4;

	for (uint32 block = 0; block < block_count; block++)
	{
		// gather: 4 triangles x 3 vertices, component-planar
		float p0x[4], p0y[4], p0z[4];
		float p1x[4], p1y[4], p1z[4];
		float p2x[4], p2y[4], p2z[4];
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

		// edge_1 = p0 - p1, edge_2 = p0 - p2 — the build-time convention (and the pre-helper
		// inline's), normals matching bit-for-bit per lane
		const __m128 e1x = _mm_sub_ps(ax, _mm_loadu_ps(p1x));
		const __m128 e1y = _mm_sub_ps(ay, _mm_loadu_ps(p1y));
		const __m128 e1z = _mm_sub_ps(az, _mm_loadu_ps(p1z));
		const __m128 e2x = _mm_sub_ps(ax, _mm_loadu_ps(p2x));
		const __m128 e2y = _mm_sub_ps(ay, _mm_loadu_ps(p2y));
		const __m128 e2z = _mm_sub_ps(az, _mm_loadu_ps(p2z));

		// n = cross(edge_1, edge_2), d = dot(n, p0) — four lanes at a time, zero shuffles
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

		// Off by default. When on, AoS is kept in step with SoA exactly as before — note this
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

	// scalar tail (0-3 planes) — writes only the real lanes, so the pad lanes of the final partial
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