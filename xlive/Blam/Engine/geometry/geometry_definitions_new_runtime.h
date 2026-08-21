#pragma once

// RUNTIME generation of the stencil-shadow geometry Vista does not ship - what tool.exe authors
// offline into the tag for tag debug. Welding, edge pairing, planes, seam pairing, validation, and
// the caches that own their lifetime. Why it must run at load time: docs/01-overview.md.

#include "geometry/geometry_definitions_new.h"
#include "math/real_math.h"
#include "models/render_model_definitions.h"
#include "rasterizer/dx9/rasterizer_dx9_stencil_shadow_tunables.h"

#include <vector>

/* structures */

// Shadow VB vertex: doubled per welded vertex (2*i = original, 2*i+1 = extruded). The vertex shader
// extrudes verts with extrude != 0 away from the light.
struct s_stencil_shadow_vertex
{
	real_point3d position;
	real32 extrude;
};
ASSERT_STRUCT_SIZE(s_stencil_shadow_vertex, 16);

// GPU-skinned shadow VB vertex, for articulated sections. STATIC - written once at build in bind
// pose, and the palette at c50 poses it on the GPU, which is what removes the per-frame Lock/rewrite
// and the per-object VB contention. Same doubling convention as above.
struct s_stencil_shadow_skinned_vertex
{
	real_point3d position;		// bind pose, section space
	real32 extrude;
	uint8 indices[4];			// local node_map slot * 3 (the palette row offset added to c50); unused lanes 0
	uint8 weights[4];			// ubyte4n, sum == 255 exactly (the builder distributes the rounding residue)
};
ASSERT_STRUCT_SIZE(s_stencil_shadow_skinned_vertex, 24);

// Mirrors tag debug's render_model_dsq_silhouette_quad_block: emit an extruded quad when
// facing(tri_left) != facing(tri_right). Verts are WELDED indices, doubled at draw time.
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
	// boundary edge matched to another section's boundary at bind pose: the per-section walk skips
	// it, and the model's cross-quad list bridges the seam instead
	k_stencil_shadow_matched_boundary = 0xFFFE
};

// Mirrors tag debug's geometry_rigid_point_group shape for planes.
//
// VESTIGIAL AND CURRENTLY UNREAD: the builder emits one group per part with `node` hardcoded to 0,
// stores them and frees them, and nothing consumes `groups` / `group_count`. That is a design
// divergence rather than an omission. Tag debug uses these groups to partition planes BY NODE so it
// can transform the light into each node's space and reuse the authored planes; we mark a section
// with mixed per-vertex nodes `articulated` and CPU-skin it to world with its planes recomputed
// wholesale, so a per-node light transform has nothing to do, and the facing test reads `planes_soa`
// against a single light position without looking at the partition. Vista strips
// `rigid_point_groups` from the cache anyway, so there is no authored partition to honour.
//
// Keep or delete, but do not assume it works: `node` is not meaningful, and code that starts trusting
// it would silently transform every group by node 0.
struct s_stencil_shadow_rigid_group
{
	uint8 node;
	uint8 part_index;
	uint16 plane_count;
};
ASSERT_STRUCT_SIZE(s_stencil_shadow_rigid_group, 4);

// Cartographer-owned shadow data for one render model section, built once from resident geometry so
// it survives geometry cache eviction.
struct s_stencil_shadow_section
{
	bool valid;

	// facing test (plane per shadow-casting triangle, section space)
	real_plane3d* planes;			// AoS (kept for the scalar fallback + recompute)
	real32* planes_soa;				// SoA 4-blocks: [nx x4][ny x4][nz x4][d x4] per block
	uint32 plane_count;				// == shadow-casting triangle count
	s_stencil_shadow_rigid_group* groups;
	uint32 group_count;

	// triangle list (welded indices, 3 per plane/triangle, matches plane order)
	uint16* triangles;

	// silhouette adjacency
	s_stencil_shadow_quad* quads;
	uint32 quad_count;
	// bit per quad: the source pair shares this edge with the SAME winding (an inconsistently wound
	// mesh) and the emission swap is suppressed - the runtime mirror of the tool's edge-record
	// 'reversed' bit
	uint32* quad_same_winding_bits;

	// draw resources
	struct IDirect3DVertexBuffer9* shadow_vb;	// 2 * welded_vertex_count entries
	uint32 welded_vertex_count;

	// Articulated support (per-vertex node sections: rigid_boned with mixed nodes, and skinned).
	// Positions are CPU-transformed into WORLD space each draw, the dynamic VB is refreshed, and
	// planes are recomputed in place - tag debug's soft-group recompute. NULL/false for static
	// sections.
	//
	// This is exact, not approximate, everywhere it matters:
	// geometry_classification_get_max_nodes_per_vertex (td 0x212A40) gives worldspace 0, rigid 1,
	// rigid_boned 1 and skinned 4, so a rigid_boned vertex binds to exactly one node and its
	// "dominant node" IS its only node, while skinned sections use the full 4-bone payload below. The
	// single lossy case is a skinned section whose weight stream is missing, which falls back to
	// vertex_nodes.
	bool articulated;
	uint8* vertex_nodes;			// per welded vertex: model node index (dominant bone)
	real_point3d* base_positions;	// per welded vertex: section-space base position
	real_point3d* world_positions;	// per welded vertex: scratch for the current draw

	// Full 4-bone skinning for position-stream declaration 4 (stride 20: float3 + uint8
	// node_index[4] + uint8 node_weight[4], weights summing to 255, indices LOCAL to the section's
	// node_map). Tag debug skins the same way in section_skin_from_rigid_point_groups (td 0x19EAF0):
	// P' = sum(w_i * (P . M_i)). NULL when the section has no weight data, in which case
	// vertex_nodes drives a single-bone transform.
	uint8* vertex_bone_indices;		// 4 per welded vertex: model node indices
	real32* vertex_bone_weights;	// 4 per welded vertex: normalised weights (sum 1)

	// animate guard: render_scene runs several times per frame, so skip re-skinning when the same
	// object was already animated this frame (and re-animate when another object of the same model
	// interleaves, since sections are shared per render model)
	datum last_animated_object;
	uint32 last_animated_frame;

	// The section's node_map (local slot -> model node), cached at build for the skinning-pool
	// palette path. Vista content is entirely `force_node_maps`, so the pool stores per-REGION
	// palettes in local node_map order rather than a per-node array, and translating our model-node
	// indices into palette slots needs this map at draw time, when the tag's own copy may not be
	// resident. NULL for sections without a map (clusters, plain rigid on non-force content).
	uint8* pool_node_map;
	int32 pool_node_map_count;

	// The GPU-skinning static VB (s_stencil_shadow_skinned_vertex, doubled), built for articulated
	// sections whose every bound node is in `pool_node_map` and whose map fits the palette window
	// (count <= 67, so c50 + 3N + 2 stays inside vs_2_0's constant file). NULL means the section
	// poses on the CPU. The dynamic `shadow_vb` above is still created and still written whenever the
	// skinned draw cannot run - resources failed, or a caller without a palette - so the two coexist
	// per section at the cost of one extra static buffer.
	struct IDirect3DVertexBuffer9* skinned_vb;
};

// One bridged seam edge between two sections of a model, drawn from the OWNER section's welded verts
// when the two triangles' facing disagrees.
//
// Both endpoints come from the owner on purpose. Sections do not share a coordinate space at draw
// time - articulated ones are CPU-skinned to WORLD and draw with a NULL node matrix, while static
// ones stay in model space and draw with their node matrix - so a quad taking one vertex from each
// side of a seam would mix spaces whenever the two sections differ in kind. The partner contributes
// only `partner_triangle`, whose facing BIT is compared against the owner's, and that comparison is
// space-independent.
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

/* prototypes */

// Engine accessor (halo2.exe 0x675DD9): reads 12B float3, 16B float3+detail, and 8B compressed int16
// normalized against the model's compression_info position bounds (6 floats: x lo/hi, y lo/hi,
// z lo/hi).
int32 __cdecl geometry_section_get_compressed_vertex(
	geometry_section* section, const real32* position_bounds, int32 index,
	real_point3d* out_position, int32* out_detail);

void stencil_shadow_section_destroy(s_stencil_shadow_section* shadow);

// Get-or-build cached shadow data for a render model section (blocking preload inside). Keyed by
// render model datum plus section index, NOT per object, so every instance of a model shares one entry.
s_stencil_shadow_section* stencil_shadow_section_get(datum render_model_index, int32 section_index);

// Get-or-build cached shadow data for a BSP cluster - tag debug's environment tier. Keyed by bsp index
// plus cluster index; cluster geometry is worldspace, so one entry serves every viewer and never
// animates. Same transient/permanent caching contract as stencil_shadow_section_get: a non-resident
// cluster erases its slot and retries next frame, a malformed one stays negatively cached.
s_stencil_shadow_section* stencil_shadow_cluster_get(
	struct structure_bsp* bsp, int16 bsp_index, int32 cluster_index);

// Look up WITHOUT building. Returns true when the cluster has a cache entry at all; `*out_shadow` is
// the entry when it is usable and NULL when the entry is a negative one. Callers that throttle builds
// need the difference: "cached and rejected" must not be retried, and "never built" must not be
// treated as a permanent failure.
bool stencil_shadow_cluster_peek(
	int16 bsp_index, int32 cluster_index, s_stencil_shadow_section** out_shadow);

// Build (once per model) the cross-quad list bridging boundary edges whose bind-pose endpoints
// coincide across two sections. Cached alongside the section data.
s_stencil_shadow_model_cross* stencil_shadow_model_cross_get(
	datum render_model_index, const render_model_definition* render_model,
	s_stencil_shadow_section* const* drawn_sections, const int16* section_of_dense,
	int32 drawn_count);

// Free all generated data and reset this module's per-map diagnostic budgets. Called by
// stencil_shadow_cache_clear, which additionally resets the draw-side latches - the two halves are
// separate because the diagnostics are owned by whichever module emits them.
void stencil_shadow_generation_cache_clear(void);
