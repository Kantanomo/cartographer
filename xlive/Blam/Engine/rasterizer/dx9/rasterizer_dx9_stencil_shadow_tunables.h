#pragma once

// Every tunable, cap and shader-register assignment for the stencil shadow system, in one place.
//
// WHY THIS FILE EXISTS
// --------------------
// These constants were scattered through rasterizer_dx9_stencil_shadows.cpp — some at file scope
// between two D3D helpers, three declared INSIDE the 1400-line render hook as function locals — so
// there was no way to answer "what can I change" without reading the whole file. They are the part
// of the system most often edited and least often read in context, which is the wrong way round.
//
// Nothing here is a bare number. Every value carries the measurement or the tag-debug site it came
// from, because this project has repeatedly lost time to a constant whose provenance was forgotten
// (the 20000 that was a light PLACEMENT being used as an extrusion LENGTH is the canonical case).
// If you change one, update its note with what you measured.
//
// Layout constants that the shaders must agree with (the register enum at the bottom) live here too:
// they are not tunable in the same sense, but they are exactly what you need when editing the .fx.

#include "objects/object_types.h"	// _object_mask_* for the caster mask

/* ---------------------------------------------------------------------------------------------
   WHAT THE SHADOW IS
   --------------------------------------------------------------------------------------------- */

// Tag-debug's extrusion distance, read out of the shipped NV2A vertex microcode (2026-08-16).
// Shader 137 (the simplest cap variant) is the whole algorithm:
//     L    = c[15].xyz - v0.xyz * c[15].w        ; w = 0 directional / 1 point
//     amt  = c[20].w * v11.x                     ; v11.x = 0 near cap, 1 far cap
//     pos' = v0.xyz - normalize(L) * amt
//     oPos = dph(pos', c[0..3])                  ; DPH, so w is implicitly 1
// The extrusion is therefore a FINITE distance held in c[20].w, not a w=0 point at infinity.
// rasterizer_light_submit (td 0x1A5CB0) uploads c[15..22] from rasterizer_build_light_constants
// (td 0xAC2B0), which writes c[20].w (constants+92) from rasterizer_light_begin's last float:
//   lightmap-tier fake light  -> push 40000000h =    2.0   (td 0x21BB3A)
//   dynamic lights            -> push 44800000h = 1024.0   (td 0x11D452)
// So the lightmap stencil shadows extrude exactly 2 world units. The old 20000 here (with the
// shader's far-plane clamp) was the direct cause of shadows far larger than their casters:
// a 0.7wu biped was being given a 20000wu volume.
static const real32 k_stencil_shadow_extrusion_distance = 2.f;

// td's global shadow darkness. render_layer_lightmap_diffuse_setup (td 0x10D740) builds the
// apply-pass constant colour as ARGB(255, V, V, V) with V = (1 - flt_53A674) * 255 and blends
// SRCBLEND=ZERO / DESTBLEND=SRC_COLOR, i.e. dst *= V/255. Ours is SRCALPHA/INVSRCALPHA against
// black, dst *= (1 - a), so a == flt_53A674 exactly.
// flt_53A674 read from the shipped image (td 0x53A674) = 0x3ECCCCCD = 0.4.
static const real32 k_stencil_shadow_darkness = 0.4f;			// == td flt_53A674

// it. 615/616 — SELF-SHADOW BIAS. Shifts the whole shadow volume this far AWAY FROM the light so the
// near cap stops being coplanar with the caster's own surface. it. 615 pushed it TOWARD the light and
// was WRONG — the user saw the flicker replaced by a constant shell of self-shadow. The derivation is
// in shadow_extrude.fx beside the code that applies it.
//
// Sized to beat depth quantisation, not geometry: it only has to exceed the depth-buffer resolution at
// the ranges casters are seen, which at the measured near/far of 0.0601/1024.8 (it. 550) is far below a
// centimetre. 0.02 wu is ~3% of a biped's 0.7 wu height — invisible as a shadow offset, and two orders
// of magnitude above the precision it fights.
//
// TOO LARGE would detach the shadow from the caster's feet (a visible gap where the volume starts short).
// TOO SMALL and the coplanarity returns. If the flicker persists, raise it before suspecting anything
// else; if a gap appears at contact points, lower it.
static const real32 k_stencil_shadow_self_shadow_bias = 0.02f;

// Draw the LIGHTMAP tier (td layer 6) at all. This is the shipped, working shadow system — the one
// that casts objects against the baked world lighting.
//
// TRUE is the normal state and setting it false costs every object shadow in the game. It is not a
// feature; it exists so the ENVIRONMENT tier (BSP clusters) can be looked at on its own, because
// cluster and object volumes overlap heavily on screen.
//
// it. 652: restored to true after the cluster-tier investigation was parked.
static const bool k_stencil_shadow_lightmap_tier_enabled = true;

// WHICH OBJECT TYPES CAST.
//
// td's set is known exactly. `rasterizer_model_compute_fake_lighting` (td 0x1F4060) gates the
// pass-6 draw flag on `*(uint16*)(object + 104) <= 1`, and `object_get_and_verify_type`
// (td 0x85170) proves object+104 is e_object_type:
//     v3 = *(int16*)(object + 104);
//     if (((1 << v3) & type_mask) == 0) -> "got an object type we didn't expect"
// With _object_type_biped = 0 and _object_type_vehicle = 1, `type <= 1` is precisely
// `_object_mask_unit`. **tag debug's LIGHTMAP tier casts from BIPEDS AND VEHICLES ONLY.**
//
// it. 613: this comment previously argued FOR that restriction and described casting from items,
// projectiles and crates as a past mistake — while the code said `_object_mask_all` and did
// exactly that. Code and comment had disagreed for an unknown number of iterations. The code is
// authoritative and the user confirmed the intent: **all object types cast.**
//
// it. 620 supplies the missing justification from the reference. `_object_mask_all` IS a tag-debug
// rule — it is the DYNAMIC tier's. `render_object_should_cast_shadow` (td 0x103740) calls
// `object_get_and_verify_type(object, 0xFFFFFFFF)` — no type mask at all — so layer 13 casts from
// every object type. We are on td's layer-13 caster rule while otherwise mirroring layer 6.
//
// The cost is real and is accepted, not overlooked: crates and scenery are large, so this adds
// shadows the LIGHTMAP tier would not have. Narrow it to `_object_mask_unit` for strict layer-6 parity.
static const int32 k_stencil_shadow_caster_mask = _object_mask_all;

/* ---------------------------------------------------------------------------------------------
   REACH CLIP — the per-pixel bound that replaced every finite-far-cap experiment
   --------------------------------------------------------------------------------------------- */

// it. 557 — REACH-CLIP. This is the ONLY mode that does not put a finite far cap in the scene.
// DYNAMIC and CLIPPED both chose *where* to put the cap; any finite choice leaves it somewhere a
// receiver can graze, which is why both relocated the artefact instead of removing it. Here the
// volume runs effectively INFINITE — the shader's far-plane clamp collapses the cap onto the far
// plane, MEASURED it. 550 — and the leak that would otherwise cause is bounded PER PIXEL in
// shadow_reach_clip.fx.
//
// it. 569 parked it, and it. 573 re-enabled it. Two things had changed:
//   1. The release now goes through `rasterizer_dx9_device_set_texture` (the SAME cached setter the
//      bind uses, IDB 0x66EBC7), so the engine's texture cache cannot desync. it. 568's raw
//      `device->SetTexture(0, NULL)` bypassed that cache, leaving it believing the depth target was
//      still bound: every draw after the first sampled NOTHING (`texture=1` on the first, `texture=0`
//      on the rest) and wall decals stayed corrupted.
//   2. it. 571 found the "no shadows" seen during EVERY reach trial was a `volumes_drawn` scope bug
//      of mine, not reach. Reach had therefore never actually been evaluated.
//
// it. 600 records its structural ceiling: a per-caster bound cannot distinguish "shadow continuing
// onto a lower surface" from "shadow leaking through a floor" — both are "receiver further along the
// light". Only per-pixel occlusion could. Shipping at that level is a deliberate choice.
static const bool k_stencil_shadow_reach_enabled = true;

// Sentinel selecting reach mode in the F6 extrusion-override cycle, continuing the negative-sentinel
// scheme (-1 was DYNAMIC, -2 CLIPPED; both deleted in it. 604/605 — see td-do-not-fix.md entry 15).
static const real32 k_stencil_shadow_reach_extrusion = -3.f;

// The extrusion handed to the shader in reach mode. Must be long enough that the far-plane clamp
// engages, which it. 550 measured as ~379 wu of camera depth; 500 is the value the user already
// A/B-tested as visually clean, so reuse it rather than introduce a second unvalidated number.
static const real32 k_stencil_shadow_reach_extrusion_distance = 500.f;

// How far BEHIND the caster (in view depth, world units) a receiver may still be shadowed.
// Deliberately generous to start: too small truncates legitimate ground shadows, which is the
// failure mode the user rejected in the CLIPPED experiments. Tune DOWN from here, not up.
// it. 575 measured that 1.5 wu visibly reduces the leak — which CONFIRMED the depth sample is live and
// the whole reach mechanism works. it. 578 then made the reach PER-CASTER (traced to the caster's own
// receiver), so this constant is now only the FALLBACK used before the trace runs.
static const real32 k_stencil_shadow_reach_distance = 1.5f;

// it. 578: added past the traced hit so the bound sits just BELOW the receiving surface rather than
// exactly on it. Unlike the CLIPPED margin (it. 533-539) this is not fighting coplanarity — there is no
// far cap here — it only absorbs trace-vs-render surface disagreement, so it can stay small.
// it. 587: 0.25 -> 0.05. Since it. 586 removed the `+ radius` term, the leak past a receiving surface is
// EXACTLY this margin — no other term contributes.
//
// If SLOPED ground starts showing truncated shadows, that is this value being too small — points further
// out along a slope have a larger `along` than the traced hit. Raise it; do NOT reinstate `+ radius`,
// which scales with the caster and reopens the leak (it. 586).
static const real32 k_stencil_shadow_reach_margin = 0.05f;

// Max search distance along the light for the per-caster receiver trace. Also the value the reach
// FAILS OPEN to when the trace misses entirely — a caster over a void keeps its full shadow rather
// than losing it.
static const real32 k_stencil_shadow_reach_probe_length = 50.f;

// it. 590 added c7 (shadow dir + slope), so the block is 8 float4s. Must match shadow_reach_clip.fx's
// c216..c223 declarations exactly.
static const uint32 k_stencil_shadow_reach_constant_count = 8;

/* ---------------------------------------------------------------------------------------------
   DISTANCE FADE
   --------------------------------------------------------------------------------------------- */

// Reach used when a model's LOD block does not validate -- the behaviour this replaced.
static const real32 k_stencil_shadow_fallback_cull_distance = 70.f;
// td's fade band past the cutoff (render_lod_compute_model_alpha: 1 - (d - cutoff) * 0.1).
static const real32 k_stencil_shadow_fade_band = 10.f;

/* ---------------------------------------------------------------------------------------------
   EXPERIMENTS — all default OFF, all documented as to what turning them on would prove
   --------------------------------------------------------------------------------------------- */

// it. 561 — CROSS-SECTION SEAM STITCHING, OFF by default.
//
// Restored it. 542, dense-slot bug fixed it. 543. it. 544 then MEASURED that it changes nothing
// visible: 52 seams paired and drawn on a 4-section caster, fragmentation identical. So it carries
// no demonstrated benefit.
//
// It does carry a specific risk. Pairing is a POSITION HASH quantised to 1/4096 wu, and a matched
// pair is retagged `k_stencil_shadow_matched_boundary` on BOTH sides so the per-section walk SKIPS
// them and one bridge quad replaces two sentinel closures. A FALSE match therefore deletes two
// genuine silhouette edges and bridges an edge that is not a seam — which draws as a spike reaching
// to an unrelated vertex. The user reported exactly that ("some edges seem broken now ... caused when
// the cross-stitching runtime was built"), on a build where it was live.
//
// Zero measured upside against a mechanism that can delete real edges: default OFF. Flip to true to
// A/B it. Do NOT re-enable without a way to reject false pairs (matching the endpoints' NORMALS as
// well as positions would be the obvious guard).
static const bool k_stencil_shadow_stitch_seams = false;

// it. 621 — TWO REDUNDANT PER-FRAME PASSES OVER THE GEOMETRY, switched OFF.
//
// `stencil_shadow_section_animate` ran FIVE full passes per articulated section per object per
// frame: skin -> VB write -> plane recompute -> SoA transpose -> world-position verification. Two of
// those five carried no shipping value. Both are gated rather than deleted, because each one earned
// its place diagnosing a real defect and either could be wanted again.
//
// 1. VERIFY_ANIMATION — the outlier scan and the inflation-ratio metric. Written for it. 317's
//    "starfish" (a missing inverse-bind term inflating skinned casters ~2x). The *logging* was
//    throttled to 1-in-300 / 1-in-600; the *computation* was not, so every animate paid a full
//    sweep over welded_vertex_count plus, on its 1-in-600 frames, two more.
//
// 2. WRITE_AOS_PLANES — the AoS `planes[]` array during animate. The facing test reads ONLY
//    `planes_soa`; the scalar AoS path is documented unreachable, so the recompute was building an
//    array nothing then read, and a second pass transposed it into the array that IS read. The
//    recompute now writes SoA lanes directly.
//
//    NOTE this also removes a latent inconsistency: `stencil_shadow_section_validate` checks
//    `planes[]` against `base_positions` (bind pose), and the animate write was overwriting them with
//    WORLD-space planes — so with the write on, any validation after an animate flags every plane bad.
static const bool k_stencil_shadow_verify_animation = false;
static const bool k_stencil_shadow_write_aos_planes = false;

// it. 655 — BUILD CASTER POSES FROM THE ENGINE'S SKINNING POOL (default ON).
//
// The engine composes `interpolated_node_world x inverse_bind` once per visible object per frame —
// `render_model_build_skinning` (0x77DEBD), filled during create_visible_render_primitives, BEFORE
// either volume pass — and both of Vista's own shadow systems read their caster poses from it. So
// does td's (`model_skinning_get_node_matrix` from ONE pool shared with the drawn model, it. 488).
// With this on, our static and articulated paths read the same pool instead of re-deriving the
// product per caster per node.
//
// What changes beyond saving the arithmetic (full verification: it. 653/654):
//   * render-only node composition and eye tracking are IN the pool matrices (0x53599B runs inside
//     the builder) — closing the it. 487 fidelity gap our own composition cannot close;
//   * interpolation is in them too (render_objects.cpp:56 tries the interpolator first), so the
//     it. 500/617 fixes are preserved, not reimplemented;
//   * compound-node references (node_map values >= nodes.count) resolve correctly — those slots
//     exist only in the pool, and the old composition rejected them.
//
// A caster WITHOUT a valid pool entry this frame — off-screen, cinematic-lit, first-person, or a
// `_render_model_definition_force_node_maps` model (no per-node array) — silently keeps the old
// composition. `stencil poolprobe:` reports the measured pool-vs-composed delta; large deltas on an
// object with no render-only nodes would mean a layout assumption is wrong, and this switch turns
// the whole adoption off in one place.
static const bool k_stencil_shadow_use_skinning_pool = true;

// A/B TOGGLE for the silhouette quad winding (it. 341). Default false == CURRENT behaviour, so this
// changes nothing until it is set. It exists because the question it settles is a one-bit experiment
// that is otherwise a rebuild: **set it to true from the debugger** and compare interior parity of the
// same shadow on the same scene.
//
// What it decides: td emits `(2b, 2a, 2a+1, 2b+1)` when the LEFT triangle faces
// (`rasterizer_stencilshadow_shadows_model_section_draw`, td 0x1A16B0); we emit the reverse cycle, so
// our side sheets are wound INWARD where td's are OUTWARD (derivation in td-caps-draw.md it. 340/341).
// Our caps are NOT inverted, and our z-fail ops are inverted GLOBALLY -- so sheets and caps plausibly
// disagree in sign.
//
// Expected signature if the flag FIXES something: interior holes / patchy parity inside an otherwise
// correctly shaped and sized shadow disappear.
//
// LATENT INTERACTION with `quad_same_winding_bits`, harmless today: the swap is suppressed for
// same-winding source pairs (`!same_winding`), and that suppression was defined against the
// `right_faces` condition. `quad_same_winding_bits` is never written (see td-do-not-fix.md), so
// `same_winding` is always false and the two features cannot currently interact. **If same-winding
// pairing is ever implemented (td-same-winding-pairs.md), re-derive which side the suppression belongs
// on for the flipped case** -- it does not follow automatically.
//
// Kept as a mutable global rather than a constant ON PURPOSE: the point is to flip it live.
// (definition stays in rasterizer_dx9_stencil_shadows.cpp — it is state, not a tunable)

/* ---------------------------------------------------------------------------------------------
   CAPACITY CAPS
   td-INDEX.md lists "did a capacity cap ever bite?" as a question each run should answer, so every
   one of these has a warn-once latch behind it. A silent cap is the failure mode to avoid.
   --------------------------------------------------------------------------------------------- */

enum
{
	k_stencil_shadow_maximum_planes_per_section = 32767,
	k_stencil_shadow_maximum_quads_per_section = 65535,
	k_stencil_shadow_facing_bitvector_words = (k_stencil_shadow_maximum_planes_per_section + 31) / 32,

	// silhouette quads expand to 6 indices each; sized for the worst case section
	k_stencil_shadow_index_buffer_capacity = k_stencil_shadow_maximum_quads_per_section * 6,

	// it. 643 — how many BSP CLUSTERS may hold generated shadow data at once (the environment tier).
	//
	// This is a MEMORY bound, not a performance one, and the model cache needs no equivalent because
	// the scale is different by two orders of magnitude: a shadow VB costs 32 bytes per welded vertex
	// (doubled, 16 bytes each), a model section carries hundreds of vertices, and a cluster carries
	// tens of thousands. One cluster can outweigh every model on the map.
	//
	// Cartographer is 32-bit. Without a bound, a large map walked end to end with dynamic lights on
	// accumulates cluster VBs until an allocation fails — a crash, not a missing shadow. 64 clusters
	// at a generous ~1 MB each is ~64 MB worst case, and far less in practice.
	//
	// A hard cap rather than an LRU: eviction needs use-tracking and a policy, and both are ways to
	// get this wrong invisibly. Hitting the cap loses environment shadows in the areas visited last
	// and says so once.
	k_stencil_shadow_environment_max_cached_clusters = 64,

	// it. 614: 64 -> 256 (x4). Safe to raise: this bounds ONLY the caster `while` loop and the
	// over-cap warning — it sizes no array, so there is nothing to overflow.
	//
	// MEASURED NOT TO BE THE CURRENT LIMITER (it. 613): with `_object_mask_all` the run showed
	// `iter=224 drawn=3`, nowhere near even the old 64. 160 of those 224 are rejected by the
	// lighting gate (`render_object_cache_get_lighting` returning NULL), 30 are flagged
	// shadowless and 13 are distance-culled. Raising this removes a future ceiling; it does not
	// add casters today.
	//
	// The per-caster cost is real if it ever does bind: it. 554 measured the volumes pass at
	// ~0.768 ms and it. 555 at ~0.13 ms per drawn SECTION, so a frame that genuinely reached 256
	// casters would be expensive. The cap is a safety rail, not a tuning knob.
	k_stencil_shadow_max_casters_per_frame = 256,

	// Per-section facing bits retained for the cross-quad (seam stitch) pass. UNLIKE the caster cap
	// this one DOES size arrays (facing_scratch, cross_shadows, cross_matrices), so raising it costs
	// static storage — facing_scratch alone is this count x 1024 words.
	k_stencil_shadow_max_cross_sections = 64
};

/* ---------------------------------------------------------------------------------------------
   SHADER CONSTANT REGISTERS — must agree with the .fx files
   --------------------------------------------------------------------------------------------- */

// High registers on purpose, so the engine's own constants — e.g. the 2D text transform in c0-c3 —
// are never clobbered (state blocks proved unreliable at restoring constants on this device).
enum
{
	k_stencil_shadow_light_constant = 254,			// shadow_extrude.fx c254: xyz = light, w = 1 point / 0 dir
	k_stencil_shadow_extrusion_distance_constant = 255,	// c255: .x distance, .y self-shadow bias
	k_stencil_shadow_tint_constant = 31,			// ps_2_0 highest constant register

	// it. 618: reach-clip's 8 constants. MUST stay clear of c0-c7 — the engine's own pixel shaders
	// use that range for tints (weather_plate, fog_atmospheric_apply, bloom_simple), and writing it
	// leaked our camera vectors into their colours, which the user saw as a wall decal changing
	// colour shot to shot. c216-c223 is the top of ps_3_0's c0-c223 range.
	k_stencil_shadow_reach_constant_base = 216
};
