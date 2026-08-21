#pragma once
#include "geometry/geometry_definitions_new.h"
#include "geometry/geometry_definitions_new_runtime.h"
#include "math/real_math.h"
#include "models/render_model_definitions.h"
#include "rasterizer_dx9_stencil_shadow_tunables.h"

// Stencil shadow volumes, the DRAW side: pipeline configuration, facing test, the volume and seam
// draws, and the apply. The generator that feeds them is geometry/geometry_definitions_new_runtime.h.
// What the system is and why it is a rebuild rather than a port: docs/README.md.

/* prototypes */

// Per-light facing test: bit i set == triangle i faces the light. light_position is in section/model
// space; directional lights pass a direction and point_light == false, which skips the plane's d term
// to match the original.
//
// light_reach clears the bit of any triangle entirely further than that from the light, so it casts
// nothing. 0 = no cull, which is what both model tiers pass, a model being small enough that every
// triangle is within any light that reached it at all. The ENVIRONMENT tier needs it: a BSP cluster
// can span the whole map, and this is what tag debug's SUBCLUSTERS prevent. Correctness rather than
// performance - geometry the light never reaches cannot block it.
void stencil_shadow_build_facing_bitvector(
	const s_stencil_shadow_section* shadow,
	const real_point3d* light_position,
	bool point_light,
	uint32* out_bitvector,
	real32 light_reach = 0.f);

// Generates silhouette indices from the facing bitvector and renders the volume with two-sided stencil
// (z-fail plus caps). Point lights pass their position, directional lights the vector TOWARD the
// light with point_light == false, both in WORLD space - the facing bitvector is model-space and the
// shader extrudes after the node transform, per engine convention. model_matrix is the model->world
// transform (NULL = identity).
//
// opacity is tag debug's stipple-density fade: below ~1.0 the volume fragments are screen-door clipped
// at that fraction on SM3, and fall back to full-strength shadows without it.
//
// self_shadow_bias is per caller because the right value depends on the CASTER'S SCALE. The default is
// the model tier's; the cluster tier's own value and why it is twelve times larger sit beside it in
// render_stencil_shadow_environment.h.
//
// palette_rows/palette_matrix_count enable GPU skinning: when the section has a skinned VB, the
// skinned shader pair exists and the caller supplies the c50 palette (3 rows per node_map slot in
// LOCAL order), the draw poses on the GPU from the static skinned VB with no dynamic VB dependency.
// NULL/0 draws the classic path from shadow_vb unchanged.
void stencil_shadow_section_draw(
	const s_stencil_shadow_section* shadow,
	const uint32* facing_bitvector,
	const real_point3d* light_position,
	bool point_light,
	const real_matrix4x3* model_matrix,
	real32 extrusion_distance,
	real32 opacity = 1.f,
	real32 self_shadow_bias = k_stencil_shadow_self_shadow_bias,
	const real_vector4d* palette_rows = NULL,
	int32 palette_matrix_count = 0);

// The dynamic tier's per-object entry: one object's volumes for a POINT light, and the only user of
// the point_light == true path. Reuses the lightmap caster loop's resolve/region/LOD/section sequences
// so the two tiers cannot drift. out_articulated_skipped (nullable) counts only FAILED articulated
// sections, not unsupported ones. opacity is the light's stipple fade, min(shadow_fade, intensity), so
// the stencil counts and therefore the mask fade with the light. Returns sections drawn; the caller
// owns per-light state (c0-c3 re-upload, scissor).
int32 stencil_shadow_draw_object_volume_point_light(
	datum object_index,
	const real_point3d* light_world_position,
	real32 extrusion_distance,
	int32* out_articulated_skipped,
	real32 opacity = 1.f);

// Mode-1 (red view) tint override for the draws that follow: pass an RGBA float4 in CALLER-OWNED
// storage to recolour, NULL to restore the default red. The dynamic tier brackets its volume draws
// with this so both tiers stay tellable apart in one mode-1 frame.
void stencil_shadow_debug_tint_override_set(const real32* rgba_or_null);

// Is the GPU-skinned pair usable? Declaration plus vs_2_0 shader, plus the vs_3_0 twin on SM3 hardware,
// where stipple or reach can bind a ps_3_0 and a vs_2_0 beside it would be illegal. The animate
// consults this to decide whether the dynamic VB refresh is still needed.
bool stencil_shadow_skinned_ready(void);

// Darken where the stencil count differs from the 128 midpoint, optionally scissored, with the given
// darkness (0..1). Callers manage stencil clears per volume.
void stencil_shadow_apply_and_clear(real32 darkness, const RECT* scissor);

// Emit one silhouette sheet as two triangles into `out_six`, from a welded edge's doubled vertices
// (2v = the vertex where it is, 2v+1 = its extruded copy). Both emitters use this - the per-section
// silhouette build and the seam-bridge build - because the vertex ORDER is the winding convention the
// z-fail ops are matched to, and a change there has to reach both or the seams stop cancelling.
// Callers decide the swap; this only lays out the result.
void stencil_shadow_emit_silhouette_sheet(uint16 vert_a, uint16 vert_b, uint16* out_six);

// Draw a prepared list of cross-section seam quads for one owner section - the seam-stitch pass's
// draw. Same pipeline as stencil_shadow_section_draw, minus the silhouette index build, since the
// caller has already decided which edges bridge.
void stencil_shadow_draw_cross_indices(
	const s_stencil_shadow_section* shadow,
	const std::vector<uint16>& indices,
	const real_point3d* light_position,
	const real_matrix4x3* model_matrix,
	real32 extrusion_distance,
	real32 opacity);

// Is the SM3 vertex shader available? Both the reach module and the debug view need this fact and
// neither owns the shader handles, which stay here with the other D3D resources. D3D9 forbids mixing
// shader models, so any ps_3_0 path (reach clip, stipple) also needs this vs_3_0.
bool stencil_shadow_sm3_vertex_shader_ready(void);

// Frees all cached shadow data. TWO call sites, both required: map unload, and BEFORE a device Reset
// (rasterizer_dx9_main.cpp), since every shadow VB is D3DPOOL_DEFAULT and so is lost on reset and must
// be released first. The cache rebuilds lazily afterwards. Dropping the reset call site would leave
// stale VB pointers after any alt-tab, resolution change or fullscreen toggle.
void stencil_shadow_cache_clear(void);

// True while the tag-debug-style stencil shadow system is running. Callers use this to suppress the
// Vista shadow systems it REPLACES, notably render_cinematic_lightmap_shadows, whose projected-quad
// shadows would otherwise draw on top of our volumes.
bool stencil_shadow_active(void);

// UI-phase hook (call once per frame from the main render hook): F8 toggles shadows, F7 cycles draw
// modes. Keys only, no drawing.
void stencil_shadow_debug_update(void);

// Late render-phase hook (after render_lights_new in the native render_scene): red-volume and
// diagnostic modes only. Real shadows come from the pair below.
void __cdecl stencil_shadow_render_layer_hook(void);

// Lay the volumes, called from the native render_scene between the lightmap_indirect and SH-PRT
// layers - tag debug's passes 6/7 position. SH-PRT then draws UNMASKED: it completes the lightmap
// term that the single darken attenuates, and masking it as well attenuates those pixels twice.
void stencil_shadow_lightmap_volumes_pass(void);

// World-layer application, called around _render_layer_lightmap_indirect: mode 3 probes a faithful
// stencil mask on the world layer, mode 0 darkens where the counts mark shadow (Vista draws the full
// world lightmap in one pass).
void stencil_shadow_world_darken(void);

// Registration hook, called from the central patch registration - but it installs NO patch. The
// draw is invoked directly from Cartographer's own render_scene, so this only announces that the
// subsystem is live. Kept because the registration list is where a reader looks to find out whether
// stencil shadows are wired in at all.
void rasterizer_dx9_stencil_shadows_apply_patches(void);

// Releases the module's device objects (call on device loss/shutdown).
void stencil_shadow_shaders_dispose(void);
