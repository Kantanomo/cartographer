#pragma once

// ENVIRONMENT TIER — the level's own geometry casting stencil shadow volumes.
//
// tag debug's `rasterizer_draw_environment_shadows` (td 0x9C590), reached from `render_layer_draw`
// (td 0x10DBB0):
//
//     case 13:                                    // the DYNAMIC light layer
//         rasterizer_draw_environment_shadows();  // <- BSP clusters
//         rendered_models_draw(scene_type);       // <- objects
//         break;
//     case 6:                                     // the LIGHTMAP layer
//         rendered_models_draw(scene_type);       // <- objects only. No environment case.
//
// Read straight off the dispatch, and it settles two design questions at once:
//
//   1. This tier belongs to layer 13 ONLY. Pass 6 has no environment case because BSP-vs-world
//      shadowing is baked into the lightmaps — which is why the tier we ship casts from models only
//      and is CORRECT to do so.
//   2. The cluster volumes and the model volumes share ONE stencil buffer and ONE apply, inside the
//      same per-light layer open. So this module does not clear, does not apply, and does not choose
//      lights: it is called from inside the dynamic tier's per-light loop and contributes volumes to
//      the buffer that loop already owns.
//
// WHY IT REUSES THE MODEL GENERATOR
// ---------------------------------
// `rasterizer_stencilshadow_isq_cluster_shadow_draw` (td 0x1A2780) opens with
//
//     rasterizer_stencilshadow_build_bitvector_from_rigid_groups(cached_data + 296, NULL);
//
// — the SAME function on the SAME `geometry_isq_info` as the model tier (td 0x1A1100, our
// `stencil_shadow_build_facing_bitvector`). A cluster's shadow data is not a second silhouette
// format; it is the same format on different geometry. So there is one generator here as there is
// one generator there, reached through `stencil_shadow_cluster_get`.
//
// WHAT IS DELIBERATELY NOT PORTED
// -------------------------------
// td partitions each cluster into SUBCLUSTERS with their own visible / carmack bitvectors and
// separate internal, boundary and shared edge lists. We treat a cluster as one section. That is not
// a shortcut around something unknown: td has a mode that does exactly this — `byte_53A68E`
// (`ssc_subcluster_all_visible`) forces every subcluster's visible and carmack bits — and since we
// always draw z-fail, the per-subcluster carmack choice has nothing to select. It costs culling
// granularity, not correctness.
//
// Cross-cluster shared edges are likewise skipped; each cluster self-closes with boundary sentinels,
// as our model sections do. Cluster boundaries are where the BSP splits space, so a seam artefact
// there would be large and obvious — it is the first thing to look at if walls shadow wrongly.

#include "cseries/cseries.h"
#include "math/real_math.h"

/* tunables — kept here rather than in the shared header, for the same reason the dynamic tier keeps
   its own: switching this tier off should not mean reasoning about a file other tiers read. */

// Compile-time gate. False makes the tier completely inert and F4 refuses to enable it.
static const bool k_stencil_shadow_environment_available = true;

// Whether it starts enabled. ON, because this tier can only ever run inside the dynamic light tier's
// per-light loop, and that tier needs a deliberate F5 press. Someone who presses F5 wants the whole
// layer-13 picture; F4 then removes just this half if it misbehaves.
static const bool k_stencil_shadow_environment_default_on = true;

// Clusters drawn per light.
//
// it. 645 MEASURED the assumption behind this and it was wrong in the reassuring direction. Clusters
// were expected to be one to two orders of magnitude larger than a model section; the first run put
// them at 388-3750 triangles (verts 104-9029), i.e. comparable to a handful of sections:
//
//     stencil cluster: [1]   verts=9029 tris=3750 shadow_parts=34/68 ...
//     stencil cluster: [222] verts=104  tris=36   shadow_parts=5/5   ...
//
// So the cost here is ordinary fill rate, not something exceptional. The cap can be raised if lights
// are seen to reach more clusters than this — the first run showed `in_light_range=3`, under it.
static const int32 k_stencil_shadow_environment_max_clusters_per_light = 4;

// EXTRUSION, as a multiple of the light's own radius — NOT the dynamic tier's flat 1024.
//
// it. 645, from the first run. The dynamic tier's 1024 produced volumes the size of the map:
//
//     stencil env: ... light=(298.55,140.84,-20.87) r=6.00 extrude=1024.0
//
// A flashlight reaching 6 wu, extruded 170x further than it lights.
//
// WHY THE TWO TIERS DIFFER, AND WHY THIS IS NOT it. 640 REPEATED
// -------------------------------------------------------------
// it. 640 found that tying the MODEL tier's extrusion to the light radius truncated tall casters —
// a biped's head is far from the floor its shadow lands on, and the volume stopped short. That
// finding stands and the model tier keeps its flat 1024.
//
// The environment tier's geometry is different in kind: the caster IS the receiver set. A wall's
// shadow lands on the floor beside it, not a hundred metres away, and anything past the light's
// radius is unlit and therefore has nothing to shadow. Concretely: a caster point sits at distance
// d <= R from the light, and the shadow ray leaves the lit sphere at distance R, so an extrusion of
// R past the caster always suffices. The 2x below is margin on that bound, not a guess.
//
// The cost of getting it wrong here is also different in kind. For a model, over-extruding wastes
// fill on a thin prism. For a cluster, EVERY silhouette edge of an entire room sprays a quad that
// long, which is what filled the screen.
static const real32 k_stencil_shadow_environment_extrusion_scale = 2.f;

// Floor under the above, so a very small light still yields a usable volume rather than a sliver.
static const real32 k_stencil_shadow_environment_extrusion_minimum = 2.f;

// SELF-SHADOW BIAS for cluster volumes — 12x the model tier's 0.02.
//
// it. 649, and it is THE defect behind the shattered triangular shards, not the weld.
//
// The near cap is the caster's own light-facing triangles at their original positions. For a model
// that surface is a biped seen from a few wu away and 0.02 wu of separation is plenty. For a cluster
// the caster IS the visible world, seen from 5 to 100+ wu, where depth-buffer precision is coarser by
// orders of magnitude — 0.02 wu disappears into it, the cap stays coplanar with the surface that
// generated it, and each cap triangle flips marked/unmarked as a unit. That is why the artefact is
// large, triangular, and aligned with the world's own triangulation rather than looking like noise.
//
// shadow_extrude.fx (it. 615/616) has the derivation, including WHY the bias must push AWAY from the
// light rather than toward it. This changes only the magnitude, and only for this tier.
//
// TOO SMALL and the shards return. TOO LARGE and a shadow visibly starts off its wall — but at world
// scale that costs 0.25 wu against surfaces metres across, so there is a lot of room here.
static const real32 k_stencil_shadow_environment_self_shadow_bias = 0.25f;

// New cluster builds allowed per frame, across all lights.
//
// Written against a feared cost that it. 645 measured and did not find: clusters are hundreds to a
// few thousand triangles, not tens of thousands, and the first run built them several per second
// with no visible stall. Kept at 1 anyway — it converges in ~3 frames (`deferred=2 -> 1 -> 0` in the
// log) and costs nothing observable, so there is no reason to spend the risk. Raise it if a room
// ever takes visibly long to acquire its environment shadows.
static const int32 k_stencil_shadow_environment_max_builds_per_frame = 1;

/* prototypes */

// Contribute this light's environment volumes to the CURRENT stencil buffer. Called from inside the
// dynamic tier's per-light loop, after that light's clear and before its apply — never on its own.
// `light_position` is world space; cluster geometry is worldspace too, so no transform is involved
// anywhere in this tier. Returns the number of clusters drawn.
//
// it. 645: takes no extrusion. It derives its own from the radius, because the caller's is the MODEL
// tier's flat 1024 and that is 170x too long for a cluster — see the scale constant above.
int32 stencil_shadow_environment_draw_for_light(
	const real_point3d* light_position,
	real32 light_radius);

// Runtime on/off, F4.
bool stencil_shadow_environment_enabled(void);
void stencil_shadow_environment_toggle(void);

// Reset this module's per-map log budgets. Called by stencil_shadow_cache_clear.
void stencil_shadow_environment_reset_diagnostics(void);
