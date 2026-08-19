#pragma once

// RUNTIME generation of the geometry definitions Vista does not ship.
//
// WHY THIS MODULE EXISTS
// ----------------------
// tag debug loads its stencil-shadow geometry — the ISQ per-triangle planes and the DSQ silhouette
// quads — straight out of the tag. `tool.exe` authors them offline (`connected_geometry_*`:
// the point welder, the edge builder, the plane writer) and the runtime only looks them up.
//
// Vista has neither. Two different failures, worth keeping apart (it. 627):
//   - the ISQ/DSQ blocks are ABSENT FROM THE FORMAT. `render_model_section_data` is 112 bytes
//     (geometry_section 68 + point_data 32 + node_map 8 + pad 4) against tag debug's 524, and the
//     difference is exactly that payload (it. 518). The fields do not exist, so no cache tooling
//     could recover them.
//   - `geometry_point_data` DOES exist in those 112 bytes but arrives EMPTY — all four counts read 0
//     (td-authored-point-data.md, it. 246). That one is stripped rather than absent.
// Either way there is nothing to read, so the whole of tool.exe's offline pass has to run at load
// time instead. That is this module, and it is the project's founding premise (see CLAUDE.md).
//
// THE SPLIT AGAINST rasterizer_dx9_stencil_shadows.cpp — DRAWN FROM THE REFERENCE, NOT INVENTED
// --------------------------------------------------------------------------------------------
// The line follows tag debug's own division of labour:
//
//   * what tool.exe does offline  ->  HERE. Vertex access across every position format, welding,
//     edge pairing, plane construction, cross-section seam pairing, validation, and the cache that
//     owns their lifetime.
//   * what td's RASTERIZER does per frame  ->  stays in rasterizer_dx9_stencil_shadows.cpp. The
//     facing bitvector is `rasterizer_stencilshadow_build_bitvector_from_rigid_groups` (td
//     0x1A1100) and the per-frame skinning is `section_skin_from_rigid_point_groups` (td 0x19EAF0)
//     — both rasterizer-side there, so both stay rasterizer-side here.
//
// So `stencil_shadow_section_animate` and `stencil_shadow_build_facing_bitvector` deliberately did
// NOT move: they consume generated data every frame rather than producing the definition.
//
// it. 629: the generated STRUCTURES now live here too, with the code that produces them. The
// include direction reversed at the same time — rasterizer_dx9_stencil_shadows.h includes THIS
// header for them, not the other way round.

#include "geometry/geometry_definitions_new.h"
#include "math/real_math.h"
#include "models/render_model_definitions.h"
#include "rasterizer/dx9/rasterizer_dx9_stencil_shadow_tunables.h"

#include <vector>

/* structures */

// Shadow VB vertex: doubled per welded vertex (2*i = original, 2*i+1 = extruded).
// The vertex shader extrudes verts with extrude != 0 away from the light.
struct s_stencil_shadow_vertex
{
	real_point3d position;
	real32 extrude;
};
ASSERT_STRUCT_SIZE(s_stencil_shadow_vertex, 16);

// it. 660 — GPU-SKINNED shadow VB vertex, for articulated sections. STATIC (written once at build,
// bind pose): the palette at c50 poses it on the GPU, which is what removes the per-frame
// Lock/rewrite and the per-object VB contention. Same doubling convention as above.
//
// `indices` are the section's LOCAL node_map slots PRE-MULTIPLIED BY 3 — the palette row offset the
// shader adds to c50 directly (shadow_extrude_skinned.fx bakes out the reference shader's c[7].w
// index scale). `weights` are ubyte4n (each /255, summing 255 — the builder distributes the
// rounding residue so the sum is exact).
struct s_stencil_shadow_skinned_vertex
{
	real_point3d position;		// bind pose, section space
	real32 extrude;
	uint8 indices[4];			// local_slot * 3; unused lanes 0
	uint8 weights[4];			// normalized, sum == 255; unused lanes 0
};
ASSERT_STRUCT_SIZE(s_stencil_shadow_skinned_vertex, 24);

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

	// it. 658: the section's node_map (local slot -> model node), cached at build for the
	// skinning-pool PALETTE path. Vista content is 100% `force_node_maps` (measured it. 657:
	// flags=0xc, pool word+0 == 1 on 6-node models), so the pool stores per-REGION palettes in
	// LOCAL node_map order rather than a per-node array — and translating our model-node indices
	// into palette slots needs this map at draw time, when the tag's own copy may not be resident.
	// NULL for sections without a map (clusters, plain rigid on non-force content).
	uint8* pool_node_map;
	int32 pool_node_map_count;

	// it. 660 — the GPU-skinning static VB (s_stencil_shadow_skinned_vertex, doubled), built for
	// articulated sections whose every bound node is in `pool_node_map` and whose map fits the
	// palette window (count <= 67, so c50 + 3N + 2 stays inside vs_2_0's constant file). NULL means
	// the section poses on the CPU exactly as before it. 660.
	//
	// The DYNAMIC `shadow_vb` above is still created and still written whenever the skinned draw
	// cannot run (resources failed, or a caller without a palette) — the two coexist per section so
	// the CPU path stays whole, at the cost of one extra static buffer.
	struct IDirect3DVertexBuffer9* skinned_vb;
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

// One model's cross-section seam bridges, built once from its cached sections.
struct s_stencil_shadow_model_cross
{
	bool built;
	std::vector<s_stencil_shadow_cross_quad> quads;
};

/* globals */

// it. 510: the CURRENT caster's render-only node flags, published by the rasterizer's caster loop so
// the section builder can intersect them with the nodes the shadow geometry actually references.
// NOT OWNED — points into tag data and is valid only for the caster iteration that set it, which is
// why it is republished every iteration rather than probed once. Cleared per map by
// stencil_shadow_generation_cache_clear.
extern const int8* g_stencil_shadow_render_only_flags;
extern int32 g_stencil_shadow_render_only_node_count;

/* prototypes */

// Engine accessor (halo2.exe 0x675DD9): reads ALL vertex position formats — 12B float3,
// 16B float3+detail, and 8B compressed int16 normalized against the model's compression_info
// position bounds (6 floats: x lo/hi, y lo/hi, z lo/hi).
int32 __cdecl geometry_section_get_compressed_vertex(
	geometry_section* section, const real32* position_bounds, int32 index,
	real_point3d* out_position, int32* out_detail);

// EVERYTHING THE BUILDER READS (it. 643).
//
// The builder used to take a `render_model_section` + `render_model_section_data`. It never needed
// either type — only the eight things below — and a BSP cluster supplies all eight just as a render
// model section does. `structure_cluster` in fact BEGINS with the same two members a
// `render_model_section` does (`geometry_section_info`, `geometry_block_info`), which is what makes
// the cluster adapter thin rather than a port.
//
// That is not a convenience refactor: tag debug runs the environment tier's facing test through the
// SAME function on the SAME `geometry_isq_info` as the model tier
// (`rasterizer_stencilshadow_isq_cluster_shadow_draw`, td 0x1A2780, calling td 0x1A1100 on
// `cluster_data + 296`). One generator for both tiers is the reference's own design, not ours.
struct s_stencil_shadow_build_input
{
	const geometry_section_info* section_info;
	geometry_section* geometry;			// RESIDENT geometry — preload the block before filling this
	// NULL when the source format has no point data at all. Clusters pass NULL; render model
	// sections pass theirs, which Vista ships EMPTY but declared (it. 246) — a different thing, and
	// the diagnostic that measures it only runs for the sources that have the block.
	const geometry_point_data* point_data;
	const uint8* node_map;				// NULL for sources with no node map (clusters)
	int32 node_map_count;
	int32 global_geometry_classification;
	int32 rigid_node;					// NONE when the source has no node
	const real32* position_bounds;		// compression_info position bounds; NULL when uncompressed
	// Reject the section when unpaired (boundary) edges exceed this percentage of all edges.
	// 10 for models, 50 for clusters. 0 disables the gate entirely.
	//
	// The gate is OURS, not td's (it. 422 established the function it claimed to mirror contains no
	// geometry test at all), and it exists to catch a mesh whose topology did not come out right.
	//
	// it. 643 gave clusters a full BYPASS, reasoning that a BSP partition is cut at its portals so
	// open edges are its normal condition — td's own cluster path BRIDGES those edges
	// (`_submit_shared_edges`) rather than rejecting on them. That reasoning holds, but the bypass
	// was still wrong, and it. 646 measured why: the first run built clusters at **70% boundary
	// edges** (1131/1621). Portal cuts are a small fraction of a room's edges; 70% is a broken weld,
	// not an open partition. Removing the gate removed the one signal that would have said so, and
	// the tier drew garbage instead of reporting it.
	//
	// So: relaxed, not absent. A cluster legitimately runs far more open than a model and must not be
	// held to 10%, but one running over half unpaired has a topology problem worth hearing about.
	int32 boundary_reject_percent;

	// Select shadow-casting parts BY TYPE, walking every part, instead of by
	// `shadow_casting_part_count`. TRUE FOR CLUSTERS, false for models — because tag debug uses a
	// DIFFERENT RULE FOR EACH, and it. 643 wrongly applied the model rule to both.
	//
	// Models (`rasterizer_stencilshadow_shadows_model_section_draw`): walk parts
	// [0, shadow_casting_part_count) and never inspect the type. The tool sorts casting parts to the
	// front and records how many. That is the "D9" finding and it is correct — for models.
	//
	// Clusters (`cluster_generate_shadow_strip`, td 0x19FB20) do the opposite:
	//     for (i = 0; i < part_count; i++)
	//         if (geometry_part_type_is_shadow_casting(part->type))   // <- EVERY part, BY TYPE
	//             ...enumerate strips...
	// It never reads shadow_casting_part_count at all.
	//
	// The difference is not cosmetic. A cluster's first N parts are NOT its casters, so the count rule
	// takes an arbitrary N — pulling in transparent and non-shadowing geometry (glass, decals, fx
	// sheets), which are large, flat, two-sided and open. That casts volumes from surfaces that must
	// not cast, inflates the unpaired-edge fraction, and makes the generated plane count exceed the
	// tag's own shadow_casting_triangle_count.
	//
	// td's predicate is `geometry_part_type_is_shadow_casting` (td 0x212BC0): types 1
	// (opaque_shadow_only) and 2 (opaque_shadow_casting) cast, 3 (opaque_nonshadowing) and 4
	// (transparent) do not. `stencil_shadow_part_casts` is already byte-identical to it.
	bool select_parts_by_type;
};

// The generator proper. Both tiers enter here.
bool stencil_shadow_build_from_geometry(
	const s_stencil_shadow_build_input* input,
	s_stencil_shadow_section* out_shadow);

// Build shadow data for a resident section (blocking-preload the geometry block first).
// position_bounds = the model's compression_info position bounds, NULL when uncompressed.
// Returns false when the section has no shadow-casting parts or data is unavailable.
bool stencil_shadow_section_build(
	const render_model_section* section,
	render_model_section_data* resident_data,
	const real32* position_bounds,
	s_stencil_shadow_section* out_shadow);

void stencil_shadow_section_destroy(s_stencil_shadow_section* shadow);

// Get-or-build cached shadow data for a render model section (blocking preload inside).
// Keyed by render model datum + section index — NOT per object, so every instance of a model
// shares one entry.
s_stencil_shadow_section* stencil_shadow_section_get(datum render_model_index, int32 section_index);

// Get-or-build cached shadow data for a BSP CLUSTER — td's environment tier
// (rasterizer_stencilshadow_isq_cluster_shadow_draw, td 0x1A2780). Keyed by bsp index + cluster
// index; cluster geometry is worldspace, so one entry serves every viewer and never animates.
//
// Same transient/permanent caching contract as stencil_shadow_section_get: a non-resident cluster
// erases its slot and retries next frame, a malformed one stays negatively cached.
s_stencil_shadow_section* stencil_shadow_cluster_get(
	struct structure_bsp* bsp, int16 bsp_index, int32 cluster_index);

// Look up WITHOUT building. Returns true when the cluster has a cache entry at all — `*out_shadow`
// is the entry when it is usable and NULL when the entry is a negative one. Callers that throttle
// builds need the difference: "cached and rejected" must not be retried, and "never built" must not
// be treated as a permanent failure.
bool stencil_shadow_cluster_peek(
	int16 bsp_index, int32 cluster_index, s_stencil_shadow_section** out_shadow);

// Build (once per model) the cross-quad list bridging boundary edges whose bind-pose endpoints
// coincide across two sections. Cached alongside the section data.
s_stencil_shadow_model_cross* stencil_shadow_model_cross_get(
	datum render_model_index, const render_model_definition* render_model,
	s_stencil_shadow_section* const* drawn_sections, const int16* section_of_dense,
	int32 drawn_count);

// Free all generated data and reset this module's per-map diagnostic budgets.
// Called by stencil_shadow_cache_clear, which additionally resets the draw-side latches —
// the two halves are separate because the diagnostics are owned by whichever module emits them.
void stencil_shadow_generation_cache_clear(void);
