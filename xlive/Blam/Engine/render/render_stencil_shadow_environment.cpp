#include "stdafx.h"
#include "render_stencil_shadow_environment.h"

#include "cache/cache_files.h"
#include "geometry/geometry_definitions_new_runtime.h"
#include "rasterizer/dx9/rasterizer_dx9_stencil_shadows.h"
#include "rasterizer/dx9/rasterizer_dx9_stencil_shadow_tunables.h"
#include "render/render.h"
#include "structures/structure_bsp_definitions.h"
#include "H2MOD/Modules/h2log/h2log.h"

/* globals */

static bool g_stencil_shadow_environment_enabled =
	k_stencil_shadow_environment_available && k_stencil_shadow_environment_default_on;

// Per-map log budgets. LATCHES — file scope, reset per map (the it. 310 rule).
static bool g_env_warned_no_bsp = false;
static uint32 g_env_report_tick = 0;

// Build throttle state. Keyed on the frame index rather than reset by a per-frame hook, because this
// module has no hook of its own — it only exists inside somebody else's loop.
static uint32 g_env_build_frame = 0xFFFFFFFF;
static int32 g_env_builds_this_frame = 0;

/* private code */

// The PVS bit vector the engine fills for the current frame — one bit per cluster.
// `render_visibility_check_location_cluster_active` (halo2.exe 0x59447C) is the whole contract:
//     (render_visibility_collection[cluster >> 5] & (1 << (cluster & 31))) != 0
static const uint32* render_visibility_collection_get(void)
{
	return Memory::GetAddress<const uint32*>(0x4E8F00);
}

static bool stencil_shadow_environment_cluster_visible(int32 cluster_index)
{
	const uint32* visibility = render_visibility_collection_get();
	if (!visibility)
	{
		return true;		// no PVS available — do not cull, the sphere test still bounds us
	}
	return (visibility[cluster_index >> 5] & (1u << (cluster_index & 31))) != 0;
}

// Does the light's sphere touch the cluster's AABB? Closest-point-on-box, the standard test.
//
// This is the ONLY spatial gate that matters for correctness of the SELECTION — the PVS bit above is
// a performance gate and is allowed to be wrong in the permissive direction. Getting this one wrong
// in the RESTRICTIVE direction drops a wall that should have cast, so the comparison is inclusive.
//
// cluster-impl K1: PUBLIC now (was static) — the E11 autopsy's "survives" list; the new tier's
// stage-0 census consumes it from render_lights.cpp.
bool stencil_shadow_environment_light_touches_cluster(
	const real_rectangle3d* bounds,
	const real_point3d* light_position,
	real32 light_radius)
{
	real32 dx = 0.f, dy = 0.f, dz = 0.f;

	if (light_position->x < bounds->x0) { dx = bounds->x0 - light_position->x; }
	else if (light_position->x > bounds->x1) { dx = light_position->x - bounds->x1; }

	if (light_position->y < bounds->y0) { dy = bounds->y0 - light_position->y; }
	else if (light_position->y > bounds->y1) { dy = light_position->y - bounds->y1; }

	if (light_position->z < bounds->z0) { dz = bounds->z0 - light_position->z; }
	else if (light_position->z > bounds->z1) { dz = light_position->z - bounds->z1; }

	return (dx * dx + dy * dy + dz * dz) <= (light_radius * light_radius);
}

/* public code */

bool stencil_shadow_environment_enabled(void)
{
	return g_stencil_shadow_environment_enabled;
}

void stencil_shadow_environment_toggle(void)
{
	// if/else constexpr rather than an early return: an early return under `if constexpr` leaves the
	// remainder provably unreachable, which is C4702, which /WX turns into an error. A discarded
	// else-branch is not unreachable code — it is simply not instantiated.
	if constexpr (!k_stencil_shadow_environment_available)
	{
		LOG_INFO_GAME("stencil env: DISABLED at compile time (k_stencil_shadow_environment_available = false) — F4 ignored");
	}
	else
	{
		g_stencil_shadow_environment_enabled = !g_stencil_shadow_environment_enabled;
		LOG_INFO_GAME("stencil env: enabled={} (F4) — max_clusters/light={} builds/frame={}. NOTE this tier only runs inside the DYNAMIC light tier (F5); with that off it draws nothing either way.",
			g_stencil_shadow_environment_enabled ? 1 : 0,
			(int32)k_stencil_shadow_environment_max_clusters_per_light,
			(int32)k_stencil_shadow_environment_max_builds_per_frame);
	}
}

void stencil_shadow_environment_reset_diagnostics(void)
{
	g_env_warned_no_bsp = false;
	g_env_report_tick = 0;
	g_env_build_frame = 0xFFFFFFFF;
	g_env_builds_this_frame = 0;
}

int32 stencil_shadow_environment_draw_for_light(
	const real_point3d* light_position,
	real32 light_radius)
{
	if (!g_stencil_shadow_environment_enabled)
	{
		return 0;
	}

	// Derived here rather than taken from the caller: the dynamic tier passes its own flat 1024,
	// which is correct for a model caster and catastrophic for a cluster one. See the constant.
	real32 extrusion = light_radius * k_stencil_shadow_environment_extrusion_scale;
	if (extrusion < k_stencil_shadow_environment_extrusion_minimum)
	{
		extrusion = k_stencil_shadow_environment_extrusion_minimum;
	}
	// The caller has already established the device, the camera and the shared resources — this
	// runs inside its per-light loop. The cache-file check is the one thing worth repeating, because
	// it guards the tag reads below rather than any resource the caller touched.
	if (!cache_file_is_loaded())
	{
		return 0;
	}

	structure_bsp* bsp = global_structure_bsp_get();
	if (!bsp || bsp->clusters.count <= 0)
	{
		if (!g_env_warned_no_bsp)
		{
			g_env_warned_no_bsp = true;
			LOG_INFO_GAME("stencil env: no structure bsp (bsp={} clusters={}) — environment shadows skipped",
				bsp ? 1 : 0, bsp ? bsp->clusters.count : -1);
		}
		return 0;
	}
	const int16 bsp_index = get_global_structure_bsp_index();

	// Refresh the per-frame build budget. Several lights run this function within one frame and they
	// share one budget — the hitch is per BUILD, not per light.
	const uint32* frame_index_address = global_frame_index_get();
	const uint32 frame_index = frame_index_address ? *frame_index_address : 0;
	if (frame_index != g_env_build_frame)
	{
		g_env_build_frame = frame_index;
		g_env_builds_this_frame = 0;
	}

	// it. 648: every exit from the per-cluster loop is counted. The previous version counted only
	// what SUCCEEDED, so "nothing drew" produced no line at all — the one case the telemetry exists
	// for was the one case it was silent in. A pass that draws nothing must say which gate ate it.
	int32 considered = 0;
	int32 in_range = 0;
	int32 pvs_culled = 0;
	int32 rejected = 0;			// cached negative — built once and found unusable
	int32 drawn = 0;
	int32 deferred_builds = 0;

	for (int32 cluster_index = 0;
		cluster_index < bsp->clusters.count && drawn < k_stencil_shadow_environment_max_clusters_per_light;
		cluster_index++)
	{
		considered++;

		structure_cluster* cluster =
			(structure_cluster*)TAG_BLOCK_GET_ELEMENT(&bsp->clusters, cluster_index, structure_cluster);
		if (!cluster)
		{
			continue;
		}
		if (!stencil_shadow_environment_light_touches_cluster(
			&cluster->bounds, light_position, light_radius))
		{
			continue;
		}
		in_range++;

		// PVS last of the three: it is the cheapest to evaluate but the most likely to be
		// permissive, so ordering it after the sphere test keeps the counts meaningful — `in_range`
		// then means "the light reaches it", independent of what the camera can see.
		if (!stencil_shadow_environment_cluster_visible(cluster_index))
		{
			pvs_culled++;
			continue;
		}

		// THE BUILD THROTTLE. Peek first, and charge the budget ONLY for a genuine build.
		//
		// The obvious version — "if the budget is spent, skip" — is wrong in two ways that both
		// present as flickering environment shadows rather than as a stall:
		//
		//   * charging for a CACHE HIT starves the rest. In steady state with four cached clusters
		//     and a budget of one, the first cluster spends the budget and the other three defer,
		//     every frame, forever. Only one wall would ever shadow.
		//   * charging for a NEGATIVE entry starves them the same way, permanently, because that
		//     cluster can never succeed and so never stops consuming the budget.
		//
		// Hence the tri-state peek: `cached` separates "known" from "unknown", and `shadow`
		// separates "usable" from "known bad".
		s_stencil_shadow_section* shadow = NULL;
		const bool cached = stencil_shadow_cluster_peek(bsp_index, cluster_index, &shadow);
		if (!cached)
		{
			if (g_env_builds_this_frame >= k_stencil_shadow_environment_max_builds_per_frame)
			{
				deferred_builds++;
				continue;		// next frame gets it
			}
			// The attempt is what costs, so charge before it rather than after: a cluster that fails
			// to build has still done the welding work that the throttle exists to spread out.
			g_env_builds_this_frame++;
			shadow = stencil_shadow_cluster_get(bsp, bsp_index, cluster_index);
		}
		if (!shadow || !shadow->valid)
		{
			rejected++;		// cached negative, or this frame's build failed. `stencil cluster:` says why
			continue;
		}

		// Cluster geometry is WORLDSPACE, so the facing test's "model space" IS world space and the
		// draw takes a NULL node matrix. No transform anywhere in this tier — the single largest
		// difference from the model tiers, and the reason this path is the simplest one the
		// generator has.
		static uint32 facing_bitvector[k_stencil_shadow_facing_bitvector_words];
		// The reach cull is what keeps a map-spanning cluster from casting map-spanning volumes.
		// Passed the light's own radius: beyond it there is no light to block.
		stencil_shadow_build_facing_bitvector(shadow, light_position, true, facing_bitvector,
			light_radius);
		stencil_shadow_section_draw(shadow, facing_bitvector, light_position, true,
			NULL, extrusion, 1.f, k_stencil_shadow_environment_self_shadow_bias);
		drawn++;
	}

	// THROTTLED, NOT LATCHED (it. 648). A latch answers "did this ever work"; a throttle answers
	// "is it working now", which is the question actually being asked while a tier is brought up.
	// Every counter is printed whether or not anything drew, so a silent pass is impossible:
	//   in_light_range == 0  -> no cluster's bounds met the light sphere
	//   pvs_culled          -> the camera cannot see them
	//   rejected            -> built and found unusable; the `stencil cluster:` line gives the reason
	//   deferred            -> the build throttle, should reach 0 within a few frames
	if ((++g_env_report_tick % 120) == 0)
	{
		LOG_INFO_GAME("stencil env: bsp={} clusters={} in_light_range={} pvs_culled={} rejected={} deferred={} DRAWN={} light=({:.2f},{:.2f},{:.2f}) r={:.2f} extrude={:.1f}",
			bsp_index, considered, in_range, pvs_culled, rejected, deferred_builds, drawn,
			light_position->x, light_position->y, light_position->z, light_radius, extrusion);
	}
	return drawn;
}
