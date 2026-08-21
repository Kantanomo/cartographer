#pragma once

// This subsystem's telemetry goes through the engine's own event system, under the category
// `rasterizer:dx9:stencil` and its leaves (build, weld, validate, cache, cluster, draw, apply, frame,
// skinning, pool, reach, lod, caster, dynamic, environment, stitch, perf). That nests below the
// `rasterizer:dx9` category the rasterizer already uses, so a line can be enabled or silenced by
// category and level at runtime instead of only by build configuration.
//
// LOG_STENCIL remains for guarding a diagnostic that is NOT a log line - an accumulator, a probe -
// which must not run in a shipping build. `event` itself compiles out when EVENTS_ENABLED is not
// defined, and it discards its arguments when it does: read a member directly in the call rather than
// hoisting it into a local only the call reads, or release_min_msvc turns the C4189 into an error.
#ifndef NDEBUG
#define LOG_STENCIL
#endif

// Every tunable, cap and shader-register assignment for the stencil shadow system. Each value carries
// the measurement or the tag-debug site it came from; if you change one, update its note with what you
// measured. The register constants are not tunable in the same sense, but they are what you need when
// editing the .fx files, so they live here too.

#include "objects/object_types.h"	// _object_mask_* for the caster mask

/* constants */

// what the shadow is

// Tag debug's extrusion distance, read out of the shipped NV2A vertex microcode. The extrusion is a
// FINITE distance held in c[20].w, not a w=0 point at infinity: rasterizer_build_light_constants
// (td 0xAC2B0) writes it from rasterizer_light_begin's last float, which pushes 2.0 for the
// lightmap-tier fake light and 1024.0 for dynamic lights.
static const real32 k_stencil_shadow_extrusion_distance = 2.f;

// Tag debug's global shadow darkness (td flt_53A674). Its apply pass blends ZERO/SRC_COLOR against
// ARGB(255, V, V, V) with V = (1 - flt_53A674) * 255, i.e. dst *= V/255; ours is SRCALPHA/INVSRCALPHA
// against black, dst *= (1 - a), so the alpha equals flt_53A674 exactly.
static const real32 k_stencil_shadow_darkness = 0.4f;

// SELF-SHADOW BIAS. Shifts the whole volume this far AWAY FROM the light so the near cap stops being
// coplanar with the caster's own surface; pushing it toward the light instead replaces the flicker
// with a constant shell of self-shadow. Sized to beat depth quantisation rather than geometry - at the
// measured near/far of 0.0601/1024.8 that is far below a centimetre, and 0.02 wu is ~3% of a biped's
// height. Too large detaches the shadow from the caster's feet; too small and the coplanarity returns.
static const real32 k_stencil_shadow_self_shadow_bias = 0.02f;

// Draw the LIGHTMAP tier (tag debug's layer 6) at all - the shipped system that casts objects against
// the baked world lighting. True is the normal state; it exists as a switch only so the environment
// tier can be looked at on its own, because cluster and object volumes overlap heavily on screen.
static const bool k_stencil_shadow_lightmap_tier_enabled = true;

// WHICH OBJECT TYPES CAST. Tag debug's LIGHTMAP tier casts from bipeds and vehicles only:
// `rasterizer_model_compute_fake_lighting` (td 0x1F4060) gates the pass-6 draw flag on
// `object->type <= 1`, and biped is 0 and vehicle is 1, so the test is exactly `_object_mask_unit`.
// We instead take tag debug's DYNAMIC-tier caster rule, where `render_object_should_cast_shadow`
// (td 0x103740) passes no type mask at all and every object type casts. The cost is accepted rather
// than overlooked - crates and scenery are large, so this adds shadows layer 6 would not have.
// Narrow to `_object_mask_unit` for strict layer-6 parity.
static const int32 k_stencil_shadow_caster_mask = _object_mask_all;

// reach clip - the per-pixel bound that replaced every finite-far-cap experiment

// The only mode that does not put a finite far cap in the scene. Any finite cap sits somewhere a
// receiver can graze, which relocates the artefact rather than removing it; here the volume runs
// effectively infinite (the shader's far-plane clamp collapses the cap onto the far plane) and the
// leak that would otherwise cause is bounded per pixel in shadow_reach_clip.fx.
//
// Structural ceiling, accepted: a per-caster bound cannot distinguish a shadow continuing onto a lower
// surface from one leaking through a floor, since both are "receiver further along the light". Only
// per-pixel occlusion could.
static const bool k_stencil_shadow_reach_enabled = true;

// Sentinel selecting reach mode in the F6 extrusion-override cycle (negative values are modes).
static const real32 k_stencil_shadow_reach_extrusion = -3.f;

// The extrusion handed to the shader in reach mode. Must be long enough that the far-plane clamp
// engages, measured at ~379 wu of camera depth.
static const real32 k_stencil_shadow_reach_extrusion_distance = 500.f;

// How far BEHIND the caster (in view depth, world units) a receiver may still be shadowed. Only the
// FALLBACK, used before the per-caster receiver trace runs. Too small truncates legitimate ground
// shadows, which is the failure mode the finite-cap experiments were rejected for; tune DOWN from here.
static const real32 k_stencil_shadow_reach_distance = 1.5f;

// Added past the traced hit so the bound sits just below the receiving surface rather than exactly on
// it. It absorbs trace-vs-render surface disagreement only - there is no far cap here to fight - and
// since the `+ radius` term was removed the leak past a receiving surface is exactly this margin. If
// SLOPED ground shows truncated shadows this is too small; raise it, and do NOT reinstate `+ radius`,
// which scales with the caster and reopens the leak.
static const real32 k_stencil_shadow_reach_margin = 0.05f;

// Max search distance along the light for the per-caster receiver trace, and the value the reach FAILS
// OPEN to when the trace misses - a caster over a void keeps its full shadow rather than losing it.
static const real32 k_stencil_shadow_reach_probe_length = 50.f;

// Must match shadow_reach_clip.fx's c216..c223 declarations exactly.
static const uint32 k_stencil_shadow_reach_constant_count = 8;

// distance fade

// Reach used when a model's LOD block does not validate.
static const real32 k_stencil_shadow_fallback_cull_distance = 70.f;
// Tag debug's fade band past the cutoff (render_lod_compute_model_alpha: 1 - (d - cutoff) * 0.1).
static const real32 k_stencil_shadow_fade_band = 10.f;

// experiments - all default OFF

// CROSS-SECTION SEAM STITCHING. Measured to change nothing visible (52 seams paired and drawn on a
// 4-section caster, fragmentation identical), and it carries a specific risk: pairing is a position
// hash quantised to 1/4096 wu, and a matched pair is retagged on BOTH sides so the per-section walk
// skips them. A FALSE match therefore deletes two genuine silhouette edges and bridges an edge that is
// not a seam, which draws as a spike reaching to an unrelated vertex - observed on a build where this
// was live. Do not re-enable without a way to reject false pairs; matching endpoint NORMALS as well as
// positions is the obvious guard.
static const bool k_stencil_shadow_stitch_seams = false;

// A per-frame pass over the geometry that carries no shipping value, gated rather than deleted
// because it earned its place diagnosing a real defect. It built the AoS `planes[]` array during
// animate, but the facing test reads only `planes_soa`, so a second pass transposed it into the array
// that IS read; the recompute now writes SoA lanes directly. Leaving it off also removes a latent
// inconsistency, since `stencil_shadow_section_validate` checks `planes[]` against bind-pose positions
// while the animate write was overwriting them with world-space planes.
static const bool k_stencil_shadow_write_aos_planes = false;

// BUILD CASTER POSES FROM THE ENGINE'S SKINNING POOL. The engine composes
// interpolated_node_world x inverse_bind once per visible object per frame in
// `render_model_build_skinning` (halo2.exe 0x77DEBD), before either volume pass, and both of Vista's own shadow
// systems read their caster poses from it - as does tag debug's. Reading the same pool also gets us
// render-only node composition and eye tracking, interpolation, and compound-node references
// (node_map values >= nodes.count) that our own composition cannot resolve. A caster with no valid
// pool entry this frame - off-screen, cinematic-lit, first-person, or a force-node-maps model -
// silently keeps the old composition.
static const bool k_stencil_shadow_use_skinning_pool = true;

// capacity caps - each has a warn-once latch behind it, because a silent cap is the failure mode

enum
{
	k_stencil_shadow_maximum_planes_per_section = 32767,
	k_stencil_shadow_maximum_quads_per_section = 65535,
	k_stencil_shadow_facing_bitvector_words = (k_stencil_shadow_maximum_planes_per_section + 31) / 32,

	// silhouette quads expand to 6 indices each; sized for the worst case section
	k_stencil_shadow_index_buffer_capacity = k_stencil_shadow_maximum_quads_per_section * 6,

	// How many BSP clusters may hold generated shadow data at once. A MEMORY bound: a shadow VB costs
	// 32 bytes per welded vertex, a model section carries hundreds of vertices and a cluster carries
	// tens of thousands, so one cluster can outweigh every model on the map. Without a bound, a large
	// map walked end to end accumulates cluster VBs until an allocation fails in this 32-bit process -
	// a crash, not a missing shadow. A hard cap rather than an LRU, because eviction needs use-tracking
	// and a policy and both can go wrong invisibly.
	k_stencil_shadow_environment_max_cached_clusters = 64,

	// Bounds only the caster loop and the over-cap warning; it sizes no array. Measured not to be the
	// current limiter - a full-mask run reached 224 candidates and drew 3, the rest rejected by the
	// lighting gate, flagged shadowless or distance-culled. A safety rail, not a tuning knob: at the
	// measured ~0.13 ms per drawn section a frame that genuinely reached 256 casters would be expensive.
	k_stencil_shadow_max_casters_per_frame = 256,

	// Per-section state retained for the cross-quad (seam stitch) pass. Unlike the caster cap this one
	// DOES size arrays - every member of s_stencil_shadow_caster_sections - so raising it costs static
	// storage, and its `facing` alone is this count x 1024 words.
	k_stencil_shadow_max_cross_sections = 64,

	// The GPU-skinning palette window. shadow_extrude_skinned.fx reads rows c50 + 3S, +1 and +2 for
	// palette slot S, so the highest register a slot touches is 50 + 3S + 2. Staying clear of our own
	// c254/c255 therefore allows S up to 67 - that is 68 slots, and exactly the `palette[204]` the
	// shader declares.
	//
	// THIS CAP IS ONE SLOT BELOW THAT WINDOW, so a 68-node section falls back to the CPU path even
	// though its palette would fit. Whether claiming the last slot is safe has never been measured, so
	// the conservative bound stands; raising it is a behaviour change, not a typo fix. A section over
	// the cap draws on the CPU path with its dynamic VB, as one over 68 would have to anyway.
	k_stencil_shadow_max_palette_nodes = 67,
	k_stencil_shadow_palette_row_count = k_stencil_shadow_max_palette_nodes * 3,

	// Index spaces the port stores in a byte, and therefore the sizes of the arrays keyed by them:
	// a SECTION index inside s_stencil_shadow_cross_quad, and a model NODE index inside
	// s_stencil_shadow_section::vertex_nodes.
	k_stencil_shadow_section_index_count = 256,
	k_stencil_shadow_node_index_count = 256
};

// shader constant registers - must agree with the .fx files

// High registers on purpose, so the engine's own constants - the 2D text transform in c0-c3, say - are
// never clobbered; state blocks proved unreliable at restoring constants on this device.
//
// Every one of these is checkable without a rebuild: fxc records the register a name binds to in the
// constant table of the compiled header, and the .h files under vertex_shaders_dx9 carry that table
// as a comment. If an upload ever stops taking effect, read the table before reading the code.
enum
{
	k_stencil_shadow_node_constant = 50,			// shadow_extrude.fx `node` c50-c52 / `palette` c50..
	k_stencil_shadow_light_constant = 254,			// shadow_extrude.fx c254: xyz = light, w = 1 point / 0 dir
	k_stencil_shadow_extrusion_distance_constant = 255,	// c255: .x distance, .y self-shadow bias
	k_stencil_shadow_tint_constant = 31,			// shadow_solid.fx `solid_color`; ps_2_0's highest register
	k_stencil_shadow_stipple_constant = 30,			// shadow_stipple.fx `stipple_c`

	// Reach-clip's 8 constants. MUST stay clear of c0-c7, which the engine's own pixel shaders use for
	// tints (weather_plate, fog_atmospheric_apply, bloom_simple) - writing there leaked our camera
	// vectors into their colours. c216-c223 is the top of ps_3_0's c0-c223 range.
	k_stencil_shadow_reach_constant_base = 216
};
