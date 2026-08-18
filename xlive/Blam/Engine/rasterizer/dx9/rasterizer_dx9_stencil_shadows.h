#pragma once
#include "geometry/geometry_definitions_new.h"
#include "math/real_math.h"
#include "models/render_model_definitions.h"

// Stencil shadow volumes (ISQ/DSQ) port from the 2003 Xbox tag-debug build.
//
// Scope as implemented (the old "P1 prototype: rigid sections, single light, z-pass, no
// cross-section stitching" note described none of this and has been removed):
//   - all four geometry classifications -- worldspace/rigid draw statically, rigid_boned with
//     mixed nodes and skinned go through the articulated path (CPU skin + per-frame planes)
//   - Z-FAIL with near and far caps. td's layer 6 forces the carmack flag on
//     (lightmap_shadows_model_begin, td 0x21DAF0), so the lightmap tier is always z-fail;
//     z-pass is the DYNAMIC tier (layer 13), which we do not implement
//   - cross-section seam stitching via s_stencil_shadow_cross_quad -- BUILT BUT DISABLED
//     (see stencil_shadow_model_cross_get, 2026-08-16): dominant-node skinning let seam verts
//     drift apart, so a one-sided bridge left the PARTNER section's volume open (wedge sheets
//     fanning off bipeds). Each section self-closes via its boundary sentinels instead, which
//     counts correctly without bridges. The stated precondition for re-enabling was "until
//     full-weight skinning lands" -- see td-seam-stitching-precondition.md, that may now be met
//   - one synthetic light per caster, built from render_lighting.shadow_direction, matching
//     td's build_fake_light (td 0x21B700)
//
// ISQ planes and DSQ silhouette quads are GENERATED AT RUNTIME because Vista's format has no
// field for them: render_model_section (92 B) carries no isq_info and no quad block, so this is
// not stripped data but absent data. td reads both from the tag.
//
// RE reference: the td-*.md notes in the repo root; td-isq-generation.md is the entry point.

/* constants */

enum
{
	k_stencil_shadow_maximum_planes_per_section = 32767,
	k_stencil_shadow_maximum_quads_per_section = 65535,
	k_stencil_shadow_facing_bitvector_words = (k_stencil_shadow_maximum_planes_per_section + 31) / 32
};

/* structures */

// Shadow VB vertex: doubled per welded vertex (2*i = original, 2*i+1 = extruded).
// The vertex shader extrudes verts with extrude != 0 away from the light.
struct s_stencil_shadow_vertex
{
	real_point3d position;
	real32 extrude;
};
ASSERT_STRUCT_SIZE(s_stencil_shadow_vertex, 16);

// Mirrors tag-debug render_model_dsq_silhouette_quad_block (8 bytes):
// emit an extruded quad when facing(tri_left) != facing(tri_right).
// Verts are WELDED indices (doubled at draw time: 2*v / 2*v+1).
struct s_stencil_shadow_quad
{
	uint16 vert_a;
	uint16 vert_b;
	uint16 tri_left;
	uint16 tri_right;	// == k_stencil_shadow_boundary_triangle for open (boundary) edges
};
ASSERT_STRUCT_SIZE(s_stencil_shadow_quad, 8);

enum
{
	k_stencil_shadow_boundary_triangle = 0xFFFF,
	// boundary edge matched to another section's boundary at bind pose: the per-section
	// walk skips it; the model's cross-quad list bridges the seam instead (td's
	// shared-edge stitching)
	k_stencil_shadow_matched_boundary = 0xFFFE
};

// Mirrors tag-debug geometry_rigid_point_group shape for planes.
//
// VESTIGIAL -- CURRENTLY UNREAD (verified it. 247). The builder emits one group per part with
// `node` hardcoded to 0, stores them, and frees them; **nothing ever consumes `groups` /
// `group_count`**. The "node == 0xFF marks a soft group" behaviour this comment used to describe
// as "P3+" was never implemented -- no group is ever tagged 0xFF.
//
// That is a design divergence, not an omission. td uses these groups to partition planes BY NODE so
// it can transform the light into each node's space and reuse the authored planes
// (rasterizer_stencilshadow_build_bitvector_from_rigid_groups, td 0x1A1100, which tests
// `node == 0xFF` per group and recomputes only the soft ones). We took the other route: a section
// with mixed per-vertex nodes is marked `articulated` and CPU-skinned to world with its planes
// recomputed wholesale, so a per-node light transform has nothing to do. Our facing test reads
// `planes_soa` against a single light position and never looks at the partition.
//
// Vista strips `rigid_point_groups` from the cache anyway (all four `point_data` counts read 0 --
// td-authored-point-data.md, it. 246), so there is no authored partition to honour even if wanted.
//
// Keep or delete, but do not assume it works: the `node` field is not meaningful, and code that
// starts trusting it would silently transform every group by node 0.
struct s_stencil_shadow_rigid_group
{
	uint8 node;
	uint8 part_index;
	uint16 plane_count;
};
ASSERT_STRUCT_SIZE(s_stencil_shadow_rigid_group, 4);

// Cartographer-owned shadow data for one render model section.
// Built once from resident geometry (survives geometry cache eviction).
struct s_stencil_shadow_section
{
	bool valid;

	// facing test (plane per shadow-casting triangle, section space)
	real_plane3d* planes;			// AoS (kept for the scalar fallback + recompute)
	real32* planes_soa;				// td SoA 4-blocks: [nx x4][ny x4][nz x4][d x4] per block
	uint32 plane_count;				// == shadow-casting triangle count
	s_stencil_shadow_rigid_group* groups;
	uint32 group_count;

	// triangle list (welded indices, 3 per plane/triangle, matches plane order)
	uint16* triangles;

	// silhouette adjacency
	s_stencil_shadow_quad* quads;
	uint32 quad_count;
	// bit per quad: the source pair shares this edge with the SAME winding (inconsistently
	// wound mesh); the emission swap is suppressed — the runtime mirror of the tool's
	// edge-record 'reversed' bit
	uint32* quad_same_winding_bits;

	// draw resources
	struct IDirect3DVertexBuffer9* shadow_vb;	// 2 * welded_vertex_count entries
	uint32 welded_vertex_count;

	// it. 526: largest axis extent of this section's welded points, in the space its vertices live in.
	// Filled at build time from the bound already computed for the size diagnostic. Used only by the
	// PER-SECTION dynamic-extrusion experiment (F6 mode) — a caster's sections sit at different heights,
	// so a single per-caster extrusion cannot place every section's far cap clear of the floor.
	real32 extent_max;

	// Articulated support (per-vertex node sections: rigid_boned with mixed nodes, and skinned).
	// Positions are CPU-transformed into WORLD space each draw, the dynamic VB is refreshed, and
	// planes are recomputed in place — tag-debug's soft-group recompute. NULL/false for static
	// sections.
	//
	// PRECISION: this is exact, not approximate, everywhere it matters.
	// geometry_classification_get_max_nodes_per_vertex (td 0x212A40) gives worldspace 0, rigid 1,
	// rigid_boned 1, skinned 4 — so a rigid_boned vertex binds to exactly ONE node and the
	// "dominant node" for it IS its only node. Skinned sections use the full 4-bone payload below
	// (P' = sum(w_i * (P . M_i))). The single lossy case is a skinned section whose weight stream
	// is missing, which falls back to vertex_nodes; the old comment here read as though every
	// animated model were approximated, which was never true.
	bool articulated;
	uint8* vertex_nodes;			// per welded vertex: model node index (dominant bone)
	real_point3d* base_positions;	// per welded vertex: section-space base position
	real_point3d* world_positions;	// per welded vertex: scratch for the current draw

	// Full 4-bone skinning for position-stream declaration 4 (stride 20:
	// float3 + uint8 node_index[4] + uint8 node_weight[4], weights summing to 255,
	// indices LOCAL to the section's node_map). td skins the same way in
	// section_skin_from_rigid_point_groups (td 0x19EAF0): P' = sum(w_i * (P . M_i)).
	// NULL when the section has no weight data, in which case vertex_nodes drives a
	// single-bone transform.
	uint8* vertex_bone_indices;		// 4 per welded vertex: model node indices
	real32* vertex_bone_weights;	// 4 per welded vertex: normalised weights (sum 1)

	// animate guard: render_scene runs several times per frame; skip re-skinning when the
	// same object was already animated this frame (re-animates when another object of the
	// same model interleaves — sections are shared per render model)
	datum last_animated_object;
	uint32 last_animated_frame;
};

// One bridged seam edge between two sections of a model (td shared-edge stitch): drawn
// from the OWNER section's welded verts when the two triangles' facing disagrees.
//
// Both endpoints come from the OWNER on purpose. Sections do not share a coordinate space at
// draw time -- articulated ones are CPU-skinned to WORLD and draw with a NULL node matrix, while
// static ones stay in model space and draw with their node matrix. A quad taking one vertex from
// each side of a seam would therefore mix spaces and come out malformed whenever the two sections
// differ in kind. Sourcing the geometry entirely from the owner sidesteps that; the partner
// contributes only `partner_triangle`, whose FACING BIT is compared against the owner's.
//
// That comparison is space-independent: each section's bit is computed in its own space against
// the correspondingly transformed light, and "does this triangle face the light" is a boolean
// about the same physical relationship either way.
struct s_stencil_shadow_cross_quad
{
	uint16 vert_a;				// owner welded indices
	uint16 vert_b;
	uint16 owner_triangle;
	uint8 owner_section;
	uint8 partner_section;
	uint16 partner_triangle;
};

/* prototypes */

// Engine accessor (halo2.exe 0x675DD9): reads all vertex position formats, dequantizing
// compressed sections against position_bounds; out_detail receives the per-vertex local
// node index where the format carries one (declared in the .cpp, used by the builder).

// Build shadow data for a resident section (blocking-preload the geometry block first).
// position_bounds = the model's compression_info position bounds (6 floats, NULL when the
// model is uncompressed). Returns false when the section has no shadow-casting parts or
// data is unavailable.
bool stencil_shadow_section_build(
	const render_model_section* section,
	render_model_section_data* resident_data,
	const real32* position_bounds,
	s_stencil_shadow_section* out_shadow);

void stencil_shadow_section_destroy(s_stencil_shadow_section* shadow);

// Per-light facing test: bit i set == triangle i faces the light.
// light_position is in section/model space; directional lights pass a direction and
// point_light == false (plane d term is skipped, matching the original).
void stencil_shadow_build_facing_bitvector(
	const s_stencil_shadow_section* shadow,
	const real_point3d* light_position,
	bool point_light,
	uint32* out_bitvector);

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
	real32 opacity = 1.f);

// Get-or-build cached shadow data for a render model section (blocking preload inside).
s_stencil_shadow_section* stencil_shadow_section_get(datum render_model_index, int32 section_index);

// Darken where the stencil count differs from the 128 midpoint, optionally scissored,
// with the given darkness (0..1). Callers manage stencil clears per volume.
void stencil_shadow_apply_and_clear(real32 darkness, const RECT* scissor);

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
