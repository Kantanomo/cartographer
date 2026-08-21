#pragma once
#include "geometry/geometry_definitions_new.h"
#include "math/real_math.h"
#include "models/render_model_definitions.h"
// it. 624: every tunable, cap and shader-register assignment.
#include "rasterizer_dx9_stencil_shadow_tunables.h"

// Stencil shadow volumes (ISQ/DSQ) port from the 2003 Xbox tag-debug build.
//
// Scope as implemented:
//   - all four geometry classifications -- worldspace/rigid draw statically, rigid_boned with
//     mixed nodes and skinned go through the articulated path (CPU skin + per-frame planes)
//   - Z-FAIL with near and far caps. td's layer 6 forces the carmack flag on
//     (lightmap_shadows_model_begin, td 0x21DAF0), so the lightmap tier is always z-fail;
//     z-pass is the DYNAMIC tier (layer 13), which we do not implement
//   - cross-section seam stitching via s_stencil_shadow_cross_quad -- BUILT BUT DISABLED, and
//     the REASON CHANGED (corrected it. 627; the old note here still gave the superseded one).
//     It is no longer blocked on skinning: full-weight skinning landed, the pairing was restored
//     (it. 542/543), and it. 544 then MEASURED that enabling it changes nothing visible. It stays
//     off because it can DELETE real edges -- pairing is a position hash, and a false match
//     retags two genuine silhouette edges as matched and bridges an edge that is not a seam,
//     which the user observed as spikes. Gate: k_stencil_shadow_stitch_seams. Do not re-enable
//     without rejecting false pairs by NORMAL as well as position
//   - one synthetic light per caster, built from render_lighting.shadow_direction, matching
//     td's build_fake_light (td 0x21B700)
//
// ISQ planes and DSQ silhouette quads are GENERATED AT RUNTIME. Be precise about why, because the
// two halves differ (sharpened it. 627):
//   - the ISQ/DSQ blocks are ABSENT FROM VISTA'S FORMAT, not stripped from it.
//     `render_model_section_data` is 112 B (geometry_section 68 + point_data 32 + node_map 8 + pad 4)
//     against td's 524, and the difference is exactly the ISQ/DSQ payload (it. 518). No cache tooling
//     could recover them; the fields do not exist.
//   - `geometry_point_data` DOES exist in the 112 B struct but arrives EMPTY -- all four counts read 0
//     (it. 246). That one is genuinely stripped, and is why we re-weld rather than read the welded set.
// td reads both from the tag.
//
// RE reference: the td-*.md notes in the repo root; td-isq-generation.md is the entry point.


// it. 629 — THE GENERATED STRUCTURES MOVED to geometry/geometry_definitions_new_runtime.h, with the
// code that produces them. This header is now the DRAW side only. The include below brings them
// back in, so every existing consumer of this header still sees them unchanged.
//
// The include direction REVERSED with the move: the geometry header used to include this one for the
// structs; now this one includes the geometry header. Nothing includes both directions.
#include "geometry/geometry_definitions_new_runtime.h"

/* prototypes */

// it. 623 — GENERATION MOVED. `stencil_shadow_section_build`, `stencil_shadow_section_destroy`,
// `stencil_shadow_section_get`, `stencil_shadow_model_cross_get` and
// `geometry_section_get_compressed_vertex` now live in
// geometry/geometry_definitions_new_runtime.h, with the welder, edge pairer and plane builder.
// The STRUCTURES they produce are still declared above; moving those is a follow-up.
//
// What remains below is the draw side — the per-frame facing test, the volume draw, the apply, and
// the hooks. That division follows tag debug's own: the facing bitvector and the per-frame skinning
// are rasterizer functions there too (td 0x1A1100 and 0x19EAF0).

// Per-light facing test: bit i set == triangle i faces the light.
// light_position is in section/model space; directional lights pass a direction and
// point_light == false (plane d term is skipped, matching the original).
void stencil_shadow_build_facing_bitvector(
	const s_stencil_shadow_section* shadow,
	const real_point3d* light_position,
	bool point_light,
	uint32* out_bitvector,
	// it. 651: clear the bit of any triangle entirely further than this from the light, so it casts
	// nothing. 0 = no cull, which is what both model tiers pass (a model is small enough that every
	// triangle is within any light that reached it at all).
	//
	// The ENVIRONMENT tier needs it: a BSP cluster can span the whole map, and without this every one
	// of its triangles casts regardless of distance from the light. That is what td's SUBCLUSTERS
	// prevent, and it is correctness rather than performance — geometry the light never reaches
	// cannot block it.
	real32 light_reach = 0.f);

// Draw: generates silhouette indices from the facing bitvector and renders the
// volume with two-sided stencil (z-fail + caps).
// point lights pass their position; directional lights pass the vector TOWARD the
// light with point_light == false — both in WORLD space (the facing bitvector is
// model-space; the shader extrudes after the node transform, engine convention).
// model_matrix: model->world transform (e.g. object node 0 matrix; NULL = identity).
// opacity: td's stipple-density fade — below ~1.0 the volume fragments are screen-door
// clipped at that fraction (SM3; falls back to full-strength shadows without it).
void stencil_shadow_section_draw(
	const s_stencil_shadow_section* shadow,
	const uint32* facing_bitvector,
	const real_point3d* light_position,
	bool point_light,
	const real_matrix4x3* model_matrix,
	real32 extrusion_distance,
	real32 opacity = 1.f,
	// it. 649: PER CALLER, because the right value depends on the CASTER'S SCALE.
	//
	// The default is the model tier's, whose own note sizes it as "~3% of a biped's 0.7 wu height".
	// That is the wrong order of magnitude for BSP clusters: world geometry is seen at 5-100 wu where
	// depth precision is far coarser, and 0.02 wu vanishes into it. The near cap then stays coplanar
	// with the surface that GENERATED it and flips marked/unmarked per triangle — large, world-aligned
	// triangular shards, which is exactly what the cluster tier's first unblocked run showed.
	//
	// shadow_extrude.fx documents the mechanism at it. 615/616; this only makes its magnitude a
	// property of the caller rather than of the whole system.
	real32 self_shadow_bias = k_stencil_shadow_self_shadow_bias,
	// it. 660 — GPU skinning. When the section has a skinned VB, the skinned shader pair exists,
	// and the caller supplies the c50 palette (stencil_shadow_pool_build_palette, 3 rows per
	// node_map slot in LOCAL order), the draw poses on the GPU from the STATIC skinned VB — no
	// dynamic VB dependency. NULL/0 draws the classic path from shadow_vb unchanged.
	const real_vector4d* palette_rows = NULL,
	int32 palette_matrix_count = 0);

// impl2 J12/J15 — the DYNAMIC tier's per-object entry: one object's volumes for a POINT
// light (the first user of the point_light=true path). Reuses the lightmap caster loop's
// resolve/region/LOD/section sequences so the tiers cannot drift; J15 wired the articulated
// path (CPU-skin to world + the GPU palette + the it. 660 stale-pose guard), so skinned
// casters — bipeds — draw too. out_articulated_skipped (nullable) now counts only FAILED
// articulated sections (animate/palette failure), not unsupported ones.
// J26: `opacity` = td's stipple shadow fade (min(shadow_fade, intensity), R46's terms) —
// below ~0.995 the volume fragments screen-door out through the shipped stipple path, so the
// stencil counts (and therefore the mask) fade with the light. 1.f = full-strength.
// Returns sections drawn. Caller owns per-light state (c0–c3 re-upload, scissor).
int32 stencil_shadow_draw_object_volume_point_light(
	datum object_index,
	const real_point3d* light_world_position,
	real32 extrusion_distance,
	int32* out_articulated_skipped,
	real32 opacity = 1.f);

// impl2 J13 — mode-1 (red view) tint override for the draws that follow: pass an RGBA float4
// in CALLER-OWNED storage to recolour, NULL to restore the default red. The dynamic tier
// brackets its volume draws with this so both tiers stay tellable in one mode-1 frame.
void stencil_shadow_debug_tint_override_set(const real32* rgba_or_null);

// it. 660: is the GPU-skinned pair usable? Declaration + vs_2_0 shader, plus the vs_3_0 twin on
// SM3 hardware (where stipple/reach can bind a ps_3_0 and a vs_2_0 beside it would be illegal).
// The animate consults this to decide whether the dynamic VB refresh is still needed.
bool stencil_shadow_skinned_ready(void);

// Darken where the stencil count differs from the 128 midpoint, optionally scissored,
// with the given darkness (0..1). Callers manage stencil clears per volume.
void stencil_shadow_apply_and_clear(real32 darkness, const RECT* scissor);

// it. 630: is the SM3 vertex shader available? Both the reach module and the debug view need this
// fact and neither owns the shader handles, which stay here with the other D3D resources. D3D9
// forbids mixing shader models, so any ps_3_0 path (reach clip, stipple) also needs this vs_3_0.
bool stencil_shadow_sm3_vertex_shader_ready(void);

// Frees all cached shadow data. TWO call sites, both required:
//   - map unload
//   - BEFORE a device Reset (rasterizer_dx9_main.cpp) -- every shadow VB is D3DPOOL_DEFAULT
//     (dynamic ones must be; static ones are for uniformity), so they are lost on reset and
//     must be released first. The cache rebuilds lazily afterwards.
// Dropping the reset call site would leave stale VB pointers after any alt-tab, resolution
// change or fullscreen toggle.
void stencil_shadow_cache_clear(void);

// True while the td-style stencil shadow system is running. Callers use this to suppress the
// Vista shadow systems it REPLACES -- notably render_cinematic_lightmap_shadows, whose
// projected-quad shadows would otherwise draw on top of our volumes.
bool stencil_shadow_active(void);

// UI-phase hook (call once per frame from the main render hook): F8 toggles shadows,
// F7 cycles draw modes. Keys only — no drawing.
void stencil_shadow_debug_update(void);

// Late render-phase hook (after render_lights_new in the native render_scene): red-volume
// and diagnostic modes only. Real shadows use the masking trio below.
void __cdecl stencil_shadow_render_layer_hook(void);

// Masking architecture (tag-debug passes 6/7), called from the native render_scene:
// volumes pass between lightmap_indirect and SH-PRT; mask_begin/end bracket the SH-PRT
// layer draw so shadowed pixels receive no direct lightmap light.
void stencil_shadow_lightmap_volumes_pass(void);

// World-layer application (called around _render_layer_lightmap_indirect):
// mode 3 probes a faithful stencil mask on the world layer; mode 0 darkens where the
// counts mark shadow (Vista draws the full world lightmap in one pass — design map 6.6).
void stencil_shadow_world_darken(void);

// Installs the render-layer detour. Call from the central patch registration.
void rasterizer_dx9_stencil_shadows_apply_patches(void);

// Releases the module's device objects (call on device loss/shutdown).
void stencil_shadow_shaders_dispose(void);
