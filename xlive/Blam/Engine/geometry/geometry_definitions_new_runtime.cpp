#include "stdafx.h"
#include "geometry_definitions_new_runtime.h"

#include "rasterizer/dx9/rasterizer_dx9.h"
#include "rasterizer/dx9/rasterizer_dx9_errors.h"					// rasterizer_dx9_log_hr
#include "rasterizer/dx9/rasterizer_dx9_main.h"
#include "rasterizer/dx9/rasterizer_dx9_stencil_shadow_tunables.h"
#include "cache/cache_files.h"
#include "cache/pc_geometry_cache.h"
#include "models/models.h"
#include "structures/structure_bsp_definitions.h"
#include "networking/network_event.h"

#include <unordered_map>
#include <vector>

/* structures */

struct s_position_key
{
	uint32 x, y, z, skin_a, skin_b;
	bool operator==(const s_position_key& other) const
	{
		return x == other.x && y == other.y && z == other.z
			&& skin_a == other.skin_a && skin_b == other.skin_b;
	}
};

struct s_position_key_hasher
{
	size_t operator()(const s_position_key& key) const
	{
		size_t hash = (size_t)key.x * 73856093u;
		hash ^= (size_t)key.y * 19349663u;
		hash ^= (size_t)key.z * 83492791u;
		hash ^= (size_t)key.skin_a * 2654435761u;
		hash ^= (size_t)key.skin_b * 40503u;
		return hash;
	}
};

// Everything the builder reads. It takes these rather than a `render_model_section` because a BSP
// cluster supplies all of them just as a render model section does - `structure_cluster` in fact
// begins with the same two members, which is what makes the cluster adapter thin rather than a port.
// One generator for both tiers is the reference's own design: tag debug runs the environment tier's
// facing test through the same function on the same `geometry_isq_info` as the model tier
// (rasterizer_stencilshadow_isq_cluster_shadow_draw, td 0x1A2780).
struct s_stencil_shadow_build_input
{
	const geometry_section_info* section_info;
	geometry_section* geometry;			// RESIDENT geometry - preload the block before filling this
	// NULL when the source format has no point data at all. Clusters pass NULL; render model sections
	// pass theirs, which Vista ships empty but declared - a different thing, and the diagnostic that
	// measures it only runs for the sources that have the block.
	const geometry_point_data* point_data;
	const uint8* node_map;				// NULL for sources with no node map (clusters)
	int32 node_map_count;
	int32 global_geometry_classification;
	int32 rigid_node;					// NONE when the source has no node
	const real32* position_bounds;		// compression_info position bounds; NULL when uncompressed

	// Reject the section when unpaired (boundary) edges exceed this percentage of all edges. 10 for
	// models; **0, i.e. off, for clusters**. The gate is ours, not tag debug's, and it exists to catch
	// a mesh whose topology did not come out right.
	//
	// Clusters need a looser limit than models because a BSP partition is cut at its portals, so open
	// edges are its normal condition - tag debug's own cluster path bridges those edges rather than
	// rejecting on them. But every threshold tried so far rejected every cluster on every map, which
	// leaves the tier drawing nothing at all, so it is off there. That is a known hole rather than a
	// decision: a measured run built clusters at 70% boundary edges, and since portal cuts are a small
	// fraction of a room's edges that is a broken weld, not an open partition - exactly what a gate
	// should catch. Restoring one is part of the parked cluster tier's revival (docs/15).
	int32 boundary_reject_percent;

	// Select shadow-casting parts BY TYPE, walking every part, instead of by
	// `shadow_casting_part_count`. True for clusters, false for models, because tag debug uses a
	// different rule for each: `rasterizer_stencilshadow_shadows_model_section_draw` walks parts
	// [0, shadow_casting_part_count) and never inspects the type, since the tool sorts casting parts
	// to the front and records how many, while `cluster_generate_shadow_strip` (td 0x19FB20) walks
	// every part and tests `geometry_part_type_is_shadow_casting` (td 0x212BC0), never reading the
	// count at all.
	//
	// The difference is not cosmetic. A cluster's first N parts are NOT its casters, so the count rule
	// takes an arbitrary N and pulls in transparent and non-shadowing geometry - glass, decals, fx
	// sheets - which are large, flat, two-sided and open. That casts volumes from surfaces that must
	// not cast, inflates the unpaired-edge fraction, and makes the generated plane count exceed the
	// tag's own shadow_casting_triangle_count.
	bool select_parts_by_type;
};

// The builder's working set, threaded through its five stages. Vectors rather than arrays because
// every count is only known as the walk proceeds; stage 5 copies them into the section's own
// allocations and this whole struct dies with the call.
struct s_stencil_shadow_build_state
{
	// stage 1, the weld
	uint16 weld_vertex_count;					// opaque vertices considered for welding
	std::vector<uint16> weld_map;				// raw vertex index -> welded index, or k_unwelded
	std::vector<real_point3d> welded_positions;
	std::vector<uint8> welded_nodes;			// dominant model node per welded vertex
	std::vector<uint8> welded_bone_indices;		// 4 per welded vertex, when the stream carries weights
	std::vector<real32> welded_bone_weights;	// 4 per welded vertex, normalised
	bool has_bone_weights;
	bool mixed_nodes;							// per-vertex nodes differ, so the section is articulated
	bool use_authored_weld;						// the tag carried vertex_point_indices

	// stage 2, the shadow-casting triangles
	std::vector<uint16> triangles;				// welded indices, 3 per triangle
	std::vector<real_plane3d> planes;			// one per triangle, in the same order
	std::vector<s_stencil_shadow_rigid_group> groups;
	std::vector<uint16> triangle_parts;			// owning part per triangle
	int32 casting_part_count;					// how many parts the selection rule accepted

	// stage 3, the silhouette adjacency
	std::vector<s_stencil_shadow_quad> quads;
	std::vector<uint32> quad_same_winding;		// bit per quad; the emission swap is suppressed for same-winding pairs
	uint32 boundary_edge_count;					// unpaired edges, and the counters the build report reads
	uint32 total_edge_refs;
	uint32 same_winding_candidates;

	// stage 4, the shadow vertex buffer
	bool articulated;							// draws from a per-frame dynamic VB rather than a static one
};

/* globals */

// Per-map diagnostic budgets. FILE SCOPE on purpose, reset by stencil_shadow_generation_cache_clear:
// as function-local statics these cap per PROCESS, so only the first map of a session would ever be
// described. A latch that reports a condition once belongs here and in that reset.
static uint8 g_stencil_shadow_logged_classification[8] = {};	// caps `stencil vbuf:` at 4 per class
static bool g_stencil_shadow_warned_degenerate_weld = false;
static bool g_stencil_shadow_warned_no_definition = false;
static bool g_stencil_shadow_warned_no_section_data = false;
static bool g_stencil_shadow_warned_no_bounds = false;
static bool g_stencil_shadow_warned_section_bounds = false;
static bool g_stencil_shadow_logged_authored_weld = false;
static bool g_stencil_shadow_warned_no_shadow_parts = false;
static bool g_stencil_shadow_warned_unwelded = false;
static bool g_stencil_shadow_warned_plane_cap = false;
static bool g_stencil_shadow_warned_no_node_map = false;
// The 4-bone payload's node_map fallback is a separate latch from the single-node one: they cover
// different branches of the same defect and both should be able to report.
static bool g_stencil_shadow_warned_bone_no_map = false;
static bool g_stencil_shadow_warned_normalized_no_bounds = false;
static bool g_stencil_shadow_warned_bad_strip = false;
// Why the last section build failed: set on every failure path and printed by the BUILD FAILED
// line, so the tally can be filtered by cause rather than being eight silent returns.
static const char* g_stencil_shadow_build_fail = "unset";

// weld_map entry for a vertex the weld never visited - past weld_vertex_count, or transparent.
static const uint16 k_unwelded = 0xFFFF;

/* prototypes */

static void stencil_shadow_planes_fill_soa(s_stencil_shadow_section* shadow);

static bool stencil_shadow_declaration_is_skinned(const rasterizer_vertex_buffer* buffer);

static const uint8* stencil_shadow_get_skinned_vertex(geometry_section* geometry, int32 vertex_index);

static void stencil_shadow_decompress_position(const real32* position_bounds, real_point3d* position);

static void stencil_shadow_get_vertex(
	geometry_section* geometry,
	const real32* position_bounds,
	uint16 compression_flags,
	int32 vertex_index,
	real_point3d* out_position,
	int32* out_node);

static bool stencil_shadow_part_casts(const geometry_part* part);

static s_position_key stencil_shadow_position_key(
	const real_point3d* position, uint8 node, const uint8* skin_payload);

static void stencil_shadow_section_validate(const s_stencil_shadow_section* shadow);

static bool stencil_shadow_build_weld_vertices(
	const s_stencil_shadow_build_input* input,
	s_stencil_shadow_build_state* state);

static bool stencil_shadow_build_enumerate_triangles(
	const s_stencil_shadow_build_input* input,
	s_stencil_shadow_build_state* state);

static bool stencil_shadow_build_pair_edges(
	const s_stencil_shadow_build_input* input,
	s_stencil_shadow_build_state* state);

static IDirect3DVertexBuffer9* stencil_shadow_build_vertex_buffer(
	const s_stencil_shadow_build_input* input,
	s_stencil_shadow_build_state* state);

static bool stencil_shadow_build_commit(
	const s_stencil_shadow_build_input* input,
	s_stencil_shadow_build_state* state,
	IDirect3DVertexBuffer9* shadow_vb,
	s_stencil_shadow_section* out_shadow);

static bool stencil_shadow_build_from_geometry(
	const s_stencil_shadow_build_input* input,
	s_stencil_shadow_section* out_shadow);

static bool stencil_shadow_section_build(
	const render_model_section* section,
	render_model_section_data* resident_data,
	const real32* position_bounds,
	s_stencil_shadow_section* out_shadow);

static uint64 stencil_shadow_seam_key(const real_point3d* a, const real_point3d* b);

static uint32 stencil_shadow_cluster_key(int16 bsp_index, int32 cluster_index);

/* public code */


// Engine accessor (halo2.exe 0x675DD9): reads every vertex position format - 12B float3, 16B
// float3+detail, and 8B compressed int16 normalized against the model's compression_info
// position_bounds (6 floats: x lo/hi, y lo/hi, z lo/hi).
int32 __cdecl geometry_section_get_compressed_vertex(
	geometry_section* section, const real32* position_bounds, int32 index,
	real_point3d* out_position, int32* out_detail)
{
	return INVOKE(0x275DD9, 0, geometry_section_get_compressed_vertex,
		section, position_bounds, index, out_position, out_detail);
}


void stencil_shadow_section_destroy(s_stencil_shadow_section* shadow)
{
	if (shadow->shadow_vb)
	{
		shadow->shadow_vb->Release();
	}
	delete[] shadow->planes;
	delete[] shadow->planes_soa;
	delete[] shadow->groups;
	delete[] shadow->triangles;
	delete[] shadow->quads;
	delete[] shadow->quad_same_winding_bits;
	delete[] shadow->vertex_nodes;
	delete[] shadow->vertex_bone_indices;
	delete[] shadow->vertex_bone_weights;
	delete[] shadow->base_positions;
	delete[] shadow->pool_node_map;
	if (shadow->skinned_vb)
	{
		shadow->skinned_vb->Release();
	}
	delete[] shadow->world_positions;
	memset(shadow, 0, sizeof(*shadow));
}
// the per-section shadow cache: keyed by render model datum + section index, freed on map unload

static std::unordered_map<uint32, s_stencil_shadow_section> g_stencil_shadow_cache;

s_stencil_shadow_section* stencil_shadow_section_get(datum render_model_index, int32 section_index)
{
	// The 8-bit section field is sufficient, not a truncation: MAXIMUM_SECTIONS_PER_RENDER_MODEL
	// is 255 (render_models.h), so section_index is always <= 254. The datum's low 16 bits are its
	// table index, unique among live datums; salt reuse cannot alias because the whole cache is
	// cleared on map unload (and before device reset).
	uint32 key = (((uint32)render_model_index & 0xFFFF) << 8) | ((uint32)section_index & 0xFF);
	auto found = g_stencil_shadow_cache.find(key);
	if (found != g_stencil_shadow_cache.end())
	{
		return found->second.valid ? &found->second : NULL;
	}

	s_stencil_shadow_section& slot = g_stencil_shadow_cache[key];
	memset(&slot, 0, sizeof(slot));

	// NOTE on the negative cache: the slot was inserted above, so every `return NULL` below leaves a
	// `valid == 0` entry that the early-out at the top of this function turns into "NULL forever".
	// That is correct for PERMANENT failures (bad tag data, unbuildable geometry) - it stops us
	// retrying hopeless work every frame - but it is WRONG for a transient one. See the preload
	// below.
	render_model_definition* definition = (render_model_definition*)tag_get_fast(render_model_index);
	if (!definition || !VALID_INDEX(section_index, definition->sections.count))
	{
		// permanent: tag data. Negative cache is correct.
		if (!g_stencil_shadow_warned_no_definition)
		{
			g_stencil_shadow_warned_no_definition = true;
			event(_event_warning, "rasterizer:dx9:stencil:cache: section_get failed - definition=%d section_index=%d sections=%d",
				definition ? 1 : 0, section_index, definition ? definition->sections.count : NONE);
		}
		return NULL;
	}
	render_model_section* section = definition->sections[section_index];

	// TRANSIENT FAILURE - must NOT be negatively cached. `pc_geometry_cache_preload_geometry`
	// (halo2.exe 0x6652BC) is a streaming call that fails for reasons which resolve on their own: the
	// engine can strip the blocking flag we pass, and the LRU cache can fail to allocate under
	// pressure. A negative entry here meant a section that merely was not resident YET never cast a
	// shadow again for the rest of the map. The engine itself just retries, and so must we - erase the
	// slot so the next frame rebuilds. `slot` DANGLES after the erase; do not touch it below.
	if (!pc_geometry_cache_preload_geometry(
		&section->geometry_block_info,
		(e_pc_geometry_cache_preload_flags)(_pc_geometry_cache_preload_blocking | _pc_geometry_cache_preload_flag_2)))
	{
		g_stencil_shadow_cache.erase(key);
		static uint32 preload_retry_log = 0;
		if ((preload_retry_log++ % 300) == 0)
		{
			event(_event_verbose, "rasterizer:dx9:stencil:cache: geometry not resident - model=%u section=%d, retrying next frame (count %u)",
				(uint32)render_model_index, section_index, preload_retry_log);
		}
		return NULL;
	}
	if (section->section_data.count <= 0)
	{
		// permanent: `section_data` is a tag block, its count does not depend on residency.
		if (!g_stencil_shadow_warned_no_section_data)
		{
			g_stencil_shadow_warned_no_section_data = true;
			event(_event_warning, "rasterizer:dx9:stencil:cache: section %d has no section_data - cannot build a volume", section_index);
		}
		return NULL;
	}

	// Compressed sections dequantize against their OWN section-level bounds where they have them; the
	// model-level block is the fallback. This DIVERGES from the engine, whose two consumers of position
	// bounds (`render_visible_section_set_vertex_compression` at halo2.exe 0x6809C4 and
	// `lightmap_raycast_resolve_object_hit` at 0x4B2CD4) read the model-level block only. Our
	// decompression has to reproduce the GPU's reconstruction exactly, so a section carrying bounds that
	// differ from the model's would decompress to positions the renderer does not draw, giving that one
	// section a mis-sized volume. On measured content no section carries its own block, so in practice
	// this takes the same branch the engine does; the log below fires only if that ever stops holding.
	const real32* position_bounds = NULL;
	if (section->section_info.compression_info.count > 0)
	{
		position_bounds = (const real32*)&section->section_info.compression_info[0]->position_bounds;
		if (!g_stencil_shadow_warned_section_bounds)
		{
			g_stencil_shadow_warned_section_bounds = true;
			event(_event_verbose, "rasterizer:dx9:stencil:cache: section %d has its own compression_info (%d entries) - we use it, the engine uses the model-level block; compare them if volumes are mis-sized",
				section_index, section->section_info.compression_info.count);
		}
	}
	else if (definition->compression_info.count > 0)
	{
		position_bounds = (const real32*)&definition->compression_info[0]->position_bounds;
	}

	// CRASH GUARD. `geometry_section_get_compressed_vertex` (halo2.exe 0x675DD9) case 3 dereferences
	// its `bounds` argument unconditionally (`bounds[1] - *bounds`, no null check), so a declaration-3
	// section with no compression_info at either level is a null-deref rather than a degenerate decode.
	// The engine's own caller has the same latent deref; shipped maps just never exercise it, and
	// Cartographer runs user-modified maps. Rejecting is a property of the tag data rather than of
	// timing, so unlike the residency case above the negative cache entry is correct here.
	{
		render_model_section_data* resident_data = section->section_data[0];
		geometry_section* resident = resident_data ? &resident_data->section : NULL;
		const rasterizer_vertex_buffer* position_buffer =
			(resident && resident->vertex_buffers.count > 0) ? resident->vertex_buffers[0] : NULL;
		if (position_buffer && (int32)position_buffer->declaration == 3 && !position_bounds)
		{
			if (!g_stencil_shadow_warned_no_bounds)
			{
				g_stencil_shadow_warned_no_bounds = true;
				event(_event_warning, "rasterizer:dx9:stencil:cache: section %d is declaration 3 (compressed) with NO compression_info - skipped to avoid a null bounds deref in the engine decoder",
					section_index);
			}
			return NULL;
		}
	}

	if (!stencil_shadow_section_build(section, section->section_data[0], position_bounds, &slot))
	{
		// Deliberately not latched: the point is a complete inventory of the sections that were lost,
		// not one sample. A permanent failure cannot spam, because the invalid cache entry it leaves
		// behind is never rebuilt; the residency case erases its slot on purpose so the next frame
		// retries, and a steady stream of THAT means the geometry cache is thrashing.
		event(_event_warning, "rasterizer:dx9:stencil:cache: section %d BUILD FAILED - casts no shadow (reason=%s)",
			section_index, g_stencil_shadow_build_fail);
		return NULL;
	}
	return &slot;
}

// cross-section stitching: tag debug's shared-edge stitches, seams bridged between sections

// s_stencil_shadow_model_cross is declared in this module's header - the render hook holds one.

static std::unordered_map<uint32, s_stencil_shadow_model_cross> g_stencil_shadow_cross_cache;



// Build the model's cross-quad list once all casting sections are cached: boundary edges
// whose bind-pose endpoints coincide across two sections are retagged matched (skipped by
// the per-section walk) and bridged by ONE owner-side cross quad.
s_stencil_shadow_model_cross* stencil_shadow_model_cross_get(
	datum render_model_index, const render_model_definition* render_model,
	s_stencil_shadow_section* const* drawn_sections, const int16* section_of_dense,
	int32 drawn_count)
{
	uint32 key = (uint32)render_model_index & 0xFFFF;
	auto found = g_stencil_shadow_cross_cache.find(key);
	if (found != g_stencil_shadow_cross_cache.end())
	{
		return &found->second;
	}

	s_stencil_shadow_model_cross& cross = g_stencil_shadow_cross_cache[key];
	cross.built = true;

	// Runtime seam pairing: the substitute for a TOOL-time step, not a port of a runtime one. tool.exe
	// pairs edges at tag build time and writes four shared-edge tag blocks that tag debug's runtime only
	// looks up; Vista strips them (`render_model_section_data` is 112 B against tag debug's 524, and the
	// difference is the ISQ/DSQ payload), so the pairing has to be derived here.
	//
	// A boundary quad is one the per-section walk could not pair (`tri_right == 0xFFFF`). Its endpoints
	// are in BIND-POSE model space, which is the space seams coincide in - that is why this can be built
	// once per model and cached, and why it holds for skinned sections whose world positions differ
	// every frame. Hash each boundary edge by its endpoint pair and match entries from DIFFERENT
	// sections.
	//
	// Each matched edge is retagged `k_stencil_shadow_matched_boundary` on BOTH sides so the per-section
	// draw skips it and the bridge closes it instead. Without the retag a seam gets a bridge quad AND
	// two sentinel closures, double-counting stencil along every seam.
	struct s_seam_ref
	{
		int32 section;
		uint32 quad_index;
		uint16 vert_a;
		uint16 vert_b;
		uint16 triangle;
	};
	std::unordered_map<uint64, std::vector<s_seam_ref>> seams;

	// Iterate DENSE slots, but emit the STABLE section index that `section_of_dense` recovers, so the
	// per-model cache stays valid when a different LOD changes which sections draw.
	for (int32 slot = 0; slot < drawn_count; slot++)
	{
		s_stencil_shadow_section* shadow = drawn_sections[slot];
		if (!shadow || !shadow->valid || !shadow->base_positions)
		{
			continue;
		}
		for (uint32 quad_index = 0; quad_index < shadow->quad_count; quad_index++)
		{
			const s_stencil_shadow_quad* quad = &shadow->quads[quad_index];
			if (quad->tri_right != k_stencil_shadow_boundary_triangle)
			{
				continue;	// already paired inside its own section
			}
			if (quad->vert_a >= shadow->welded_vertex_count
				|| quad->vert_b >= shadow->welded_vertex_count)
			{
				continue;
			}
			const uint64 seam = stencil_shadow_seam_key(
				&shadow->base_positions[quad->vert_a], &shadow->base_positions[quad->vert_b]);
			s_seam_ref ref;
			ref.section = slot;			// dense slot: used for pairing and for the retag
			ref.quad_index = quad_index;
			ref.vert_a = quad->vert_a;
			ref.vert_b = quad->vert_b;
			ref.triangle = quad->tri_left;
			seams[seam].push_back(ref);
		}
	}

	int32 paired = 0;
	for (auto& entry : seams)
	{
		std::vector<s_seam_ref>& refs = entry.second;
		for (uint32 i = 0; i < refs.size(); i++)
		{
			for (uint32 j = i + 1; j < refs.size(); j++)
			{
				if (refs[i].section == refs[j].section)
				{
					continue;	// a hole inside one section, not a seam between two
				}
				// OWNER is the lower section index, so the pair is emitted exactly ONCE and the
				// choice is stable across frames.
				const s_seam_ref& owner = refs[i].section < refs[j].section ? refs[i] : refs[j];
				const s_seam_ref& partner = refs[i].section < refs[j].section ? refs[j] : refs[i];
				const int16 owner_section = section_of_dense[owner.section];
				const int16 partner_section = section_of_dense[partner.section];
				if (owner_section < 0 || owner_section > 255
					|| partner_section < 0 || partner_section > 255)
				{
					continue;	// must fit the quad's uint8 fields
				}

				s_stencil_shadow_cross_quad bridge;
				bridge.vert_a = owner.vert_a;
				bridge.vert_b = owner.vert_b;
				bridge.owner_triangle = owner.triangle;
				bridge.owner_section = (uint8)owner_section;
				bridge.partner_section = (uint8)partner_section;
				bridge.partner_triangle = partner.triangle;
				cross.quads.push_back(bridge);

				// retag BOTH sides so neither is sentinel-closed as well as bridged
				drawn_sections[owner.section]->quads[owner.quad_index].tri_right =
					k_stencil_shadow_matched_boundary;
				drawn_sections[partner.section]->quads[partner.quad_index].tri_right =
					k_stencil_shadow_matched_boundary;

				// each boundary edge participates in at most one bridge
				refs.erase(refs.begin() + j);
				refs.erase(refs.begin() + i);
				i--;
				paired++;
				break;
			}
		}
	}

	event(_event_verbose, "rasterizer:dx9:stencil:stitch: model %u paired %d cross-section seams from %u distinct boundary keys",
		(uint32)render_model_index, paired, (uint32)seams.size());
	return &cross;
}

// BSP clusters: tag debug's environment tier

// Separate from the model cache on purpose rather than sharing a key space: the two are keyed by
// different things (render model datum vs bsp+cluster index) and a shared map would need a
// discriminator bit whose only job is to prevent a collision that separate maps cannot have.
static std::unordered_map<uint32, s_stencil_shadow_section> g_stencil_shadow_cluster_cache;

static bool g_stencil_shadow_warned_no_cluster_geometry = false;
static bool g_stencil_shadow_warned_cluster_bounds = false;
static bool g_stencil_shadow_warned_cluster_cap = false;
static int32 g_stencil_shadow_logged_clusters = 0;


bool stencil_shadow_cluster_peek(
	int16 bsp_index, int32 cluster_index, s_stencil_shadow_section** out_shadow)
{
	*out_shadow = NULL;
	auto found = g_stencil_shadow_cluster_cache.find(
		stencil_shadow_cluster_key(bsp_index, cluster_index));
	if (found == g_stencil_shadow_cluster_cache.end())
	{
		return false;
	}
	// TRI-STATE, and the caller depends on all three. "cached and rejected" must be distinguishable
	// from "never built": a caller throttling builds would otherwise re-charge its budget for a
	// cluster that will never succeed, starving the ones that would.
	if (found->second.valid)
	{
		*out_shadow = &found->second;
	}
	return true;
}

s_stencil_shadow_section* stencil_shadow_cluster_get(
	structure_bsp* bsp, int16 bsp_index, int32 cluster_index)
{
	if (!bsp || !VALID_INDEX(cluster_index, bsp->clusters.count))
	{
		return NULL;
	}

	const uint32 key = stencil_shadow_cluster_key(bsp_index, cluster_index);
	auto found = g_stencil_shadow_cluster_cache.find(key);
	if (found != g_stencil_shadow_cluster_cache.end())
	{
		return found->second.valid ? &found->second : NULL;
	}

	// MEMORY BOUND - needed here but not on the model cache. A shadow VB costs 32 bytes per welded
	// vertex; a model section is hundreds of vertices but a BSP cluster is tens of thousands, so one
	// cluster can outweigh every model on the map. In a 32-bit process a large map walked end to end
	// would accumulate cluster VBs until an allocation fails, which presents as a crash rather than as
	// a missing shadow. A hard count cap rather than LRU eviction: eviction needs use-tracking and a
	// policy, both of which can go wrong silently, whereas refusing to build past the cap only loses
	// environment shadows in the rooms visited last, and says so once.
	if ((int32)g_stencil_shadow_cluster_cache.size() >= k_stencil_shadow_environment_max_cached_clusters)
	{
		if (!g_stencil_shadow_warned_cluster_cap)
		{
			g_stencil_shadow_warned_cluster_cap = true;
			event(_event_warning, "rasterizer:dx9:stencil:cluster: cache holds %d clusters (the cap) - no further clusters will be built this map. Raise k_stencil_shadow_environment_max_cached_clusters if environment shadows are missing in later areas.",
				(int32)k_stencil_shadow_environment_max_cached_clusters);
		}
		return NULL;
	}

	s_stencil_shadow_section& slot = g_stencil_shadow_cluster_cache[key];
	memset(&slot, 0, sizeof(slot));

	structure_cluster* cluster =
		(structure_cluster*)TAG_BLOCK_GET_ELEMENT(&bsp->clusters, cluster_index, structure_cluster);
	if (!cluster)
	{
		return NULL;
	}

	// Same transient-vs-permanent split as the model path: a cluster whose geometry is not
	// resident YET must not be negatively cached, or it never casts again for the rest of the map.
	// The engine's own accessor (structure_cluster_get_geometry_section, halo2.exe 0x450B41) does
	// exactly this - preload, and just return NULL on failure, fresh every call.
	if (!pc_geometry_cache_preload_geometry(
		&cluster->section_block_info,
		(e_pc_geometry_cache_preload_flags)(_pc_geometry_cache_preload_blocking | _pc_geometry_cache_preload_flag_2)))
	{
		g_stencil_shadow_cluster_cache.erase(key);
		return NULL;
	}
	if (cluster->cluster_data.count <= 0)
	{
		// permanent: a tag block's count does not depend on residency
		if (!g_stencil_shadow_warned_no_cluster_geometry)
		{
			g_stencil_shadow_warned_no_cluster_geometry = true;
			event(_event_warning, "rasterizer:dx9:stencil:cluster: cluster %d has no cluster_data - casts nothing", cluster_index);
		}
		return NULL;
	}

	// Clusters carry their own compression_info; there is no model-level block to fall back to, so
	// unlike the model path this is the only source. A declaration-3 (compressed) cluster with no
	// bounds would null-deref inside the engine decoder, so it is rejected below rather than decoded.
	const real32* position_bounds = NULL;
	if (cluster->geometry_section_info.compression_info.count > 0)
	{
		position_bounds =
			(const real32*)&cluster->geometry_section_info.compression_info[0]->position_bounds;
	}

	geometry_section* geometry = cluster->cluster_data[0];
	if (!geometry)
	{
		return NULL;
	}
	{
		const rasterizer_vertex_buffer* position_buffer =
			geometry->vertex_buffers.count > 0 ? geometry->vertex_buffers[0] : NULL;
		if (position_buffer && (int32)position_buffer->declaration == 3 && !position_bounds)
		{
			if (!g_stencil_shadow_warned_cluster_bounds)
			{
				g_stencil_shadow_warned_cluster_bounds = true;
				event(_event_warning, "rasterizer:dx9:stencil:cluster: cluster %d is declaration 3 (compressed) with NO compression_info - skipped to avoid a null bounds deref in the engine decoder",
					cluster_index);
			}
			return NULL;
		}
	}

	// MEASUREMENT, not decoration - the three unknowns docs/13 listed before this tier could start.
	// A cluster is far larger than a model section, so the caps and the declaration are the things
	// that can make this tier silently wrong, and they are cheap to state.
	if (g_stencil_shadow_logged_clusters < 8)
	{
		g_stencil_shadow_logged_clusters++;
		const rasterizer_vertex_buffer* position_buffer = geometry->vertex_buffers.count > 0 ? geometry->vertex_buffers[0] : NULL;

		UNREFERENCED_PARAMETER(position_buffer);

		event(_event_verbose, "rasterizer:dx9:stencil:cluster: [%d] verts=%u tris=%u shadow_parts=%u/%u shadow_tris=%u class=%d decl=%d bounds=%d (caps: %d planes / %d quads)",
			cluster_index,
			cluster->geometry_section_info.total_vertex_count,
			cluster->geometry_section_info.total_triangle_count,
			cluster->geometry_section_info.shadow_casting_part_count,
			cluster->geometry_section_info.total_part_count,
			cluster->geometry_section_info.shadow_casting_triangle_count,
			(int32)cluster->geometry_section_info.geometry_classification,
			position_buffer ? (int32)position_buffer->declaration : -1,
			position_bounds ? 1 : 0,
			(int32)k_stencil_shadow_maximum_planes_per_section,
			(int32)k_stencil_shadow_maximum_quads_per_section);
	}

	// Cluster geometry is WORLDSPACE - no nodes, no skinning, the simplest path the builder has.
	// Forced rather than read: a cluster's own geometry_classification describes its VERTEX FORMAT,
	// while the draw-time question is whether a node matrix applies, and for structure it never does.
	s_stencil_shadow_build_input input = {};
	input.section_info = &cluster->geometry_section_info;
	input.geometry = geometry;
	input.point_data = NULL;			// absent from the format entirely - see the struct's note
	input.node_map = NULL;
	input.node_map_count = 0;
	input.global_geometry_classification = _geometry_classification_worldspace;
	input.rigid_node = NONE;
	input.position_bounds = position_bounds;
	// The boundary gate is OFF for clusters. A 50% threshold was tried and rejected every cluster on
	// every map tested, leaving the tier drawing nothing at all, so it is off until the real unpaired
	// fraction of a cluster is understood. See the field's note for why a gate is still wanted here.
	input.boundary_reject_percent = 0;
	input.select_parts_by_type = true;		// tag debug's CLUSTER rule - see the field's note

	if (!stencil_shadow_build_from_geometry(&input, &slot))
	{
		event(_event_warning, "rasterizer:dx9:stencil:cluster: [%d] BUILD FAILED - casts no shadow (reason=%s)",
			cluster_index, g_stencil_shadow_build_fail);
		return NULL;
	}
	return &slot;
}

void stencil_shadow_generation_cache_clear(void)
{
	for (auto& entry : g_stencil_shadow_cache)
	{
		stencil_shadow_section_destroy(&entry.second);
	}
	g_stencil_shadow_cache.clear();
	g_stencil_shadow_cross_cache.clear();

	for (auto& entry : g_stencil_shadow_cluster_cache)
	{
		stencil_shadow_section_destroy(&entry.second);
	}
	g_stencil_shadow_cluster_cache.clear();
	g_stencil_shadow_warned_no_cluster_geometry = false;
	g_stencil_shadow_warned_cluster_bounds = false;
	g_stencil_shadow_warned_cluster_cap = false;
	g_stencil_shadow_logged_clusters = 0;

	// Refresh this module's diagnostic budgets so each map gets its own samples. Without
	// this they are process-lifetime caps and only the first map loaded is ever described.
	//
	// The draw-side latches are reset by stencil_shadow_cache_clear, which calls this. Split by
	// OWNERSHIP: a latch belongs to whichever module emits it, so a new latch added here must be
	// reset here, and one added to the rasterizer must be reset there.
	memset(g_stencil_shadow_logged_classification, 0, sizeof(g_stencil_shadow_logged_classification));
	g_stencil_shadow_warned_degenerate_weld = false;
	g_stencil_shadow_warned_no_definition = false;
	g_stencil_shadow_warned_no_section_data = false;
	g_stencil_shadow_warned_no_bounds = false;
	g_stencil_shadow_warned_section_bounds = false;
	g_stencil_shadow_logged_authored_weld = false;
	g_stencil_shadow_warned_no_shadow_parts = false;
	g_stencil_shadow_warned_unwelded = false;
	g_stencil_shadow_warned_plane_cap = false;
	g_stencil_shadow_warned_no_node_map = false;
	g_stencil_shadow_warned_bone_no_map = false;
	g_stencil_shadow_warned_normalized_no_bounds = false;
	g_stencil_shadow_warned_bad_strip = false;
}

/* private code */

// Fill the tag-debug SoA 4-block plane layout ([nx x4][ny x4][nz x4][d x4] per block) from
// the AoS planes - the movemask facing fast path consumes this. Blocks pad to 4 with zero
// planes; consumers never index past plane_count.
static void stencil_shadow_planes_fill_soa(s_stencil_shadow_section* shadow)
{
	uint32 block_count = (shadow->plane_count + 3) / 4;
	for (uint32 block = 0; block < block_count; block++)
	{
		real32* out = &shadow->planes_soa[block * 16];
		for (uint32 lane = 0; lane < 4; lane++)
		{
			uint32 plane_index = block * 4 + lane;
			if (plane_index < shadow->plane_count)
			{
				const real_plane3d* plane = &shadow->planes[plane_index];
				out[lane] = plane->n.i;
				out[4 + lane] = plane->n.j;
				out[8 + lane] = plane->n.k;
				out[12 + lane] = plane->d;
			}
			else
			{
				out[lane] = 0.f;
				out[4 + lane] = 0.f;
				out[8 + lane] = 0.f;
				out[12 + lane] = 0.f;
			}
		}
	}
}

// Position-stream declarations seen in Vista caches for vertex_buffers[0]:
//   1 -> float3                       (stride 12)  rigid
//   2 -> float3 + node byte + 3 pad   (stride 16)  rigid_boned
//   3 -> 3x int16 + node + pad        (stride 8)   compressed
//   4 -> float3 + 8 bytes skinning    (stride 20)  SKINNED
// The engine's decoder handles only 1/2/3 and returns the origin for anything else, so every
// skinned section read (0,0,0) for all its vertices and cast nothing - the long-standing "player
// shadow is only the head". Decode declaration 4 ourselves; fall through to the engine for the rest.
static const int32 k_vertex_declaration_skinned = 4;
// Declarations 4 through 9 are byte-identical in the engine's declaration table (halo2.exe 0x7E49F8),
// so they describe the same vertex layout, and 6 is live on real sections. Do NOT widen the range by
// reading more of that table: it provably does not encode STORAGE (2 and 3 are identical to each other
// yet store stride 16 and stride 8). The stride check below is the real guard.
static const int32 k_vertex_declaration_skinned_last = 9;
static const uint32 k_vertex_declaration_skinned_stride = 20;

static bool stencil_shadow_declaration_is_skinned(const rasterizer_vertex_buffer* buffer)
{
	const int32 declaration = (int32)buffer->declaration;
	return declaration >= k_vertex_declaration_skinned
		&& declaration <= k_vertex_declaration_skinned_last
		&& (uint32)buffer->stride == k_vertex_declaration_skinned_stride;
}

static const uint8* stencil_shadow_get_skinned_vertex(geometry_section* geometry, int32 vertex_index)
{
	if (geometry->vertex_buffers.count <= 0)
	{
		return NULL;
	}
	const rasterizer_vertex_buffer* buffer = geometry->vertex_buffers[0];
	if (!buffer || !stencil_shadow_declaration_is_skinned(buffer) || !buffer->vertex_data)
	{
		return NULL;
	}
	// Bound the index against the BUFFER's own count, not the caller's: callers iterate to
	// `weld_vertex_count` (from `section_info`) while the vertex buffer carries an independent count,
	// and nothing in the tag format enforces that the two agree. NULL is this function's existing
	// "not a skinned vertex" answer, so the caller falls through to the engine decoder.
	if (!VALID_INDEX(vertex_index, (int32)buffer->count))
	{
		return NULL;
	}
	// base = vertex_data + default_vertex_offset_bytes (the engine's own addressing)
	const uint8* base = (const uint8*)buffer->vertex_data + buffer->default_vertex_offset_bytes;
	return base + (uint32)buffer->stride * (uint32)vertex_index;
}

// Declaration 4's 20-byte layout, confirmed from live data (2026-08-16):
//     float x, y, z;  uint8 node_index[4];  uint8 node_weight[4];
// Weights are ubyte4 summing to 255 (observed sum[16..19] = 254..256 across a whole section);
// indices are LOCAL to the section node_map (observed max 18 against node_map_count 19).
static const int32 k_skinned_index_offset = 12;
static const int32 k_skinned_weight_offset = 16;

// Halo 2 stores model positions normalized to [-1, +1] whenever `geometry_compression_flags & 1` is
// set and reconstructs them ON THE GPU, from constants that
// `rasterizer_dx9_set_vertex_compression_constants` (halo2.exe 0x66F551) uploads as a half-extent and
// a centre. We build volumes on the CPU, so we do it here instead: the engine's own
// `geometry_section_get_compressed_vertex` applies bounds in its case-3 arm only, and cases 1/2 and
// our stride-20 skinned decode all hand back raw normalized floats. Left undone, volumes come out
// anisotropically 3-6x oversized.
//
// Tag debug needs no equivalent - its shadow path skins an authored, uncompressed point array, never
// the compressed vertex stream.
static void stencil_shadow_decompress_position(const real32* position_bounds, real_point3d* position)
{
	// bounds layout is [x_lo, x_hi, y_lo, y_hi, z_lo, z_hi]
	position->x = position->x * ((position_bounds[1] - position_bounds[0]) * 0.5f)
		+ ((position_bounds[0] + position_bounds[1]) * 0.5f);
	position->y = position->y * ((position_bounds[3] - position_bounds[2]) * 0.5f)
		+ ((position_bounds[2] + position_bounds[3]) * 0.5f);
	position->z = position->z * ((position_bounds[5] - position_bounds[4]) * 0.5f)
		+ ((position_bounds[4] + position_bounds[5]) * 0.5f);
}

// Reads any buffer-0 position format. out_node is the engine's detail byte for declarations 1/2/3,
// and the dominant (highest-weight) local node for declaration 4.
static void stencil_shadow_get_vertex(
	geometry_section* geometry,
	const real32* position_bounds,
	uint16 compression_flags,
	int32 vertex_index,
	real_point3d* out_position,
	int32* out_node)
{
	// bit 0 == positions are normalized (bit 1 is texcoords, which we never read)
	const bool decompress = (compression_flags & 1) != 0 && position_bounds != NULL;

	// The `position_bounds != NULL` term above is a guard whose fallback silently reproduces the exact
	// defect decompression exists to remove - raw [-1, 1] values used as model space, roughly 6x
	// oversized. Diagnostic only, deliberately: without bounds there is no correct answer, and
	// rejecting the section would trade a visibly wrong shadow for an invisibly missing one. The
	// declaration-3 crash guard does not cover this; 1, 2 and 4-9 would fall through unreported.
	if ((compression_flags & 1) != 0 && position_bounds == NULL && !g_stencil_shadow_warned_normalized_no_bounds)
	{
		g_stencil_shadow_warned_normalized_no_bounds = true;
		event(_event_warning, "rasterizer:dx9:stencil:build: compression_flags=%u says positions are NORMALIZED but no position_bounds are available - decompression skipped, this section's volume will be grossly oversized (~6x)",
			(uint32)compression_flags);
	}

	const uint8* skinned = stencil_shadow_get_skinned_vertex(geometry, vertex_index);
	if (skinned)
	{
		const real32* position = (const real32*)skinned;
		out_position->x = position[0];
		out_position->y = position[1];
		out_position->z = position[2];
		if (decompress)
		{
			stencil_shadow_decompress_position(position_bounds, out_position);
		}

		int32 dominant = 0;
		uint8 best = 0;
		for (int32 i = 0; i < 4; i++)
		{
			uint8 weight = skinned[k_skinned_weight_offset + i];
			if (weight > best)
			{
				best = weight;
				dominant = skinned[k_skinned_index_offset + i];
			}
		}
		*out_node = dominant;
		return;
	}
	geometry_section_get_compressed_vertex(geometry, position_bounds, vertex_index, out_position, out_node);

	// DECLARATION 3 MUST BE EXCLUDED. Case 3 of the engine decoder already dequantises its int16s
	// into [lo, hi] via the same bounds, so applying the transform again would collapse the geometry
	// toward the bounds centre -- a shadow far SMALLER than its caster, which would read as a
	// completely different defect. Cases 1 and 2 return raw normalized floats and do need it.
	if (decompress)
	{
		const rasterizer_vertex_buffer* position_buffer =
			geometry->vertex_buffers.count > 0 ? geometry->vertex_buffers[0] : NULL;
		if (position_buffer && (int32)position_buffer->declaration != 3)
		{
			stencil_shadow_decompress_position(position_bounds, out_position);
		}
	}
}

static bool stencil_shadow_part_casts(const geometry_part* part)
{
	return part->type == _geometry_part_type_opaque_shadow_only
		|| part->type == _geometry_part_type_opaque_shadow_casting;
}

// The weld key matches tool.exe's own rule: it groups points spatially, snaps each group to its
// centroid, then splits by bit-exact comparison of position + node indices + node weights. Vista's
// exported vertices are already that post-weld output, so exact position bits are the right spatial
// test, and skin_a/skin_b carry the skinning payload verbatim so the comparison is bit-exact too.
// Keying on the dominant node alone would fuse vertices the tool kept apart. Formats with no weight
// payload degrade to the single node index, which is correct for them.
static s_position_key stencil_shadow_position_key(
	const real_point3d* position, uint8 node, const uint8* skin_payload)
{
	s_position_key key;
	memcpy(&key.x, &position->x, 4);
	memcpy(&key.y, &position->y, 4);
	memcpy(&key.z, &position->z, 4);
	if (skin_payload)
	{
		memcpy(&key.skin_a, skin_payload + k_skinned_index_offset, 4);	// 4 node indices
		memcpy(&key.skin_b, skin_payload + k_skinned_weight_offset, 4);	// 4 node weights
	}
	else
	{
		key.skin_a = node;
		key.skin_b = 0;
	}
	return key;
}
// Checks every invariant the draw path depends on: index ranges, finite positions, planes
// consistent with their triangle, and edge membership + WINDING - an interior quad's (vert_a ->
// vert_b) must run against tri_left's traversal and with tri_right's, since a violation flips the
// sheet's INCR/DECR and streaks.
static void stencil_shadow_section_validate(const s_stencil_shadow_section* shadow)
{
	uint32 bad_triangle_indices = 0, bad_quad_indices = 0, bad_positions = 0;
	uint32 bad_planes = 0, bad_edge_membership = 0, bad_winding = 0;

	auto triangle_has_ordered_edge = [shadow](uint16 triangle, uint16 edge_a, uint16 edge_b) -> int32
	{
		// returns 1 = ordered (a->b in winding), -1 = reversed, 0 = absent
		const uint16* tri = &shadow->triangles[triangle * 3];
		for (int32 corner = 0; corner < 3; corner++)
		{
			uint16 v0 = tri[corner];
			uint16 v1 = tri[(corner + 1) % 3];
			if (v0 == edge_a && v1 == edge_b)
			{
				return 1;
			}
			if (v0 == edge_b && v1 == edge_a)
			{
				return -1;
			}
		}
		return 0;
	};

	for (uint32 i = 0; i < shadow->plane_count * 3; i++)
	{
		if (shadow->triangles[i] >= shadow->welded_vertex_count)
		{
			bad_triangle_indices++;
		}
	}
	for (uint32 i = 0; i < shadow->welded_vertex_count; i++)
	{
		const real_point3d* p = &shadow->base_positions[i];
		if (!(p->x == p->x) || !(p->y == p->y) || !(p->z == p->z)
			|| fabsf(p->x) > 10000.f || fabsf(p->y) > 10000.f || fabsf(p->z) > 10000.f)
		{
			bad_positions++;
		}
	}
	for (uint32 i = 0; i < shadow->plane_count; i++)
	{
		const uint16* tri = &shadow->triangles[i * 3];
		if (tri[0] >= shadow->welded_vertex_count || tri[1] >= shadow->welded_vertex_count
			|| tri[2] >= shadow->welded_vertex_count)
		{
			continue;
		}
		const real_plane3d* plane = &shadow->planes[i];
		const real_point3d* p0 = &shadow->base_positions[tri[0]];
		real32 d_check = plane->n.i * p0->x + plane->n.j * p0->y + plane->n.k * p0->z;
		real32 n_len_sq = plane->n.i * plane->n.i + plane->n.j * plane->n.j + plane->n.k * plane->n.k;
		if (fabsf(d_check - plane->d) > 0.01f * (1.f + fabsf(plane->d)) || n_len_sq < 1e-12f)
		{
			bad_planes++;
		}
	}
	for (uint32 i = 0; i < shadow->quad_count; i++)
	{
		const s_stencil_shadow_quad* quad = &shadow->quads[i];
		bool boundary = quad->tri_right == k_stencil_shadow_boundary_triangle
			|| quad->tri_right == k_stencil_shadow_matched_boundary;
		if (quad->vert_a >= shadow->welded_vertex_count
			|| quad->vert_b >= shadow->welded_vertex_count
			|| quad->tri_left >= shadow->plane_count
			|| (!boundary && quad->tri_right >= shadow->plane_count))
		{
			bad_quad_indices++;
			continue;
		}
		// emission invariant: stored (a->b) runs AGAINST tri_left's traversal, WITH
		// tri_right's (normal interior); flagged same-winding pairs run AGAINST both;
		// boundary sentinels AGAINST their only triangle
		int32 left_order = triangle_has_ordered_edge(quad->tri_left, quad->vert_a, quad->vert_b);
		if (left_order == 0)
		{
			bad_edge_membership++;
		}
		else if (left_order != -1)
		{
			bad_winding++;
		}
		if (!boundary)
		{
			bool same_winding = shadow->quad_same_winding_bits
				&& ((shadow->quad_same_winding_bits[i >> 5] >> (i & 31)) & 1);
			int32 right_order = triangle_has_ordered_edge(quad->tri_right, quad->vert_a, quad->vert_b);
			if (right_order == 0)
			{
				bad_edge_membership++;
			}
			else if (right_order != (same_winding ? -1 : 1))
			{
				bad_winding++;
			}
		}
	}

	if (bad_triangle_indices || bad_quad_indices || bad_positions
		|| bad_planes || bad_edge_membership || bad_winding)
	{
		event(_event_warning, "rasterizer:dx9:stencil:validate: FAILED tri_idx=%u quad_idx=%u pos=%u planes=%u edges=%u winding=%u",
			bad_triangle_indices, bad_quad_indices, bad_positions,
			bad_planes, bad_edge_membership, bad_winding);
		// dump the first winding offender in full for diagnosis
		for (uint32 i = 0; i < shadow->quad_count; i++)
		{
			const s_stencil_shadow_quad* quad = &shadow->quads[i];
			bool boundary = quad->tri_right == k_stencil_shadow_boundary_triangle
				|| quad->tri_right == k_stencil_shadow_matched_boundary;
			if (boundary || quad->tri_left >= shadow->plane_count
				|| quad->tri_right >= shadow->plane_count)
			{
				continue;
			}
			bool flagged = shadow->quad_same_winding_bits
				&& ((shadow->quad_same_winding_bits[i >> 5] >> (i & 31)) & 1);
			int32 left_order = triangle_has_ordered_edge(quad->tri_left, quad->vert_a, quad->vert_b);
			int32 right_order = triangle_has_ordered_edge(quad->tri_right, quad->vert_a, quad->vert_b);
			if (left_order != -1 || right_order != (flagged ? -1 : 1))
			{
				const uint16* lt = &shadow->triangles[quad->tri_left * 3];
				const uint16* rt = &shadow->triangles[quad->tri_right * 3];

				UNREFERENCED_PARAMETER(lt);
				UNREFERENCED_PARAMETER(rt);

				event(_event_warning, "rasterizer:dx9:stencil:validate: offender quad %u edge (%u,%u) L=%u (%u,%u,%u) order=%d R=%u (%u,%u,%u) order=%d flagged=%d self=%d",
					i, quad->vert_a, quad->vert_b,
					quad->tri_left, lt[0], lt[1], lt[2], left_order,
					quad->tri_right, rt[0], rt[1], rt[2], right_order,
					flagged, quad->tri_left == quad->tri_right);
				break;
			}
		}
	}
	else
	{
		event(_event_verbose, "rasterizer:dx9:stencil:validate: ok (%u verts, %u planes, %u quads)",
			shadow->welded_vertex_count, shadow->plane_count, shadow->quad_count);
	}
}

// Every stage below takes the input and the state and returns false with
// g_stencil_shadow_build_fail set. Each opens by aliasing the state's members to the names the
// body uses, which is what lets the five read as one continuous walk.

static bool stencil_shadow_build_weld_vertices(
	const s_stencil_shadow_build_input* input,
	s_stencil_shadow_build_state* state)
{
	const geometry_section_info* info = input->section_info;
	const int32 classification = input->global_geometry_classification;
	const geometry_point_data* point_data = input->point_data;
	geometry_section* geometry = input->geometry;
	uint16& weld_vertex_count = state->weld_vertex_count;
	std::vector<uint16>& weld_map = state->weld_map;
	std::vector<real_point3d>& welded_positions = state->welded_positions;
	std::vector<uint8>& welded_nodes = state->welded_nodes;
	std::vector<uint8>& welded_bone_indices = state->welded_bone_indices;
	std::vector<real32>& welded_bone_weights = state->welded_bone_weights;
	bool& has_bone_weights = state->has_bone_weights;
	bool& mixed_nodes = state->mixed_nodes;
	bool& use_authored_weld = state->use_authored_weld;

	// 1. Weld section vertices by exact position + skinning payload (the tool welds by proximity
	// and centroid snap, and exported data is already centroid-snapped, so an exact match suffices).
	//
	// Only the OPAQUE vertices: shadow-casting part types are both opaque, so no shadow triangle can
	// reference a transparent vertex, and welding them would only inflate the shadow VB. The tool
	// sorts opaque vertices first, so they keep the same welded indices a full walk would give them.
	// A cache leaving opaque_vertex_count at 0 or above the total falls back to the total rather
	// than welding nothing.
	weld_vertex_count = info->opaque_vertex_count;
	if (weld_vertex_count == 0 || weld_vertex_count > info->total_vertex_count)
	{
		weld_vertex_count = info->total_vertex_count;
	}

	std::unordered_map<s_position_key, uint16, s_position_key_hasher> weld_lookup;
	// sized by the TOTAL so any vertex index a triangle names is in range; entries past
	// weld_vertex_count stay k_unwelded and are rejected at triangle emission
	weld_map.assign(info->total_vertex_count, k_unwelded);
	welded_positions.reserve(info->total_vertex_count);
	welded_nodes.reserve(info->total_vertex_count);

	const uint8* node_map = input->node_map;
	int32 node_map_count = input->node_map_count;
	mixed_nodes = false;

	// The full 4-bone skinning payload, captured per welded vertex when the position stream carries
	// one. tag debug blends the same way: P' = sum(w_i * (P . M_i)).
	has_bone_weights = stencil_shadow_get_skinned_vertex(geometry, 0) != NULL;
	if (has_bone_weights)
	{
		welded_bone_indices.reserve(info->total_vertex_count * 4);
		welded_bone_weights.reserve(info->total_vertex_count * 4);
	}

	// AUTHORED WELD, preferred when present: vertex_point_indices is tag debug's own
	// vertex_to_point_map, so the tool's vertex->point grouping can be used verbatim instead of
	// reconstructed. That removes an inference - the tool merges on position and weight TOLERANCES,
	// which our exact-match key can only recover because those tolerances were baked in upstream.
	// Self-gating: absent or wrong-sized data leaves the heuristic running unchanged.
	const uint16* authored_point_indices = NULL;
	if (point_data && point_data->vertex_point_indices.count == info->total_vertex_count
		&& info->total_vertex_count > 0)
	{
		authored_point_indices = point_data->vertex_point_indices[0];
	}
	use_authored_weld = authored_point_indices != NULL;
	std::unordered_map<uint16, uint16> authored_lookup;
	{
		if (!g_stencil_shadow_logged_authored_weld)
		{
			g_stencil_shadow_logged_authored_weld = true;
			event(_event_verbose, "rasterizer:dx9:stencil:weld: authored vertex_point_indices %s (count=%d verts=%u)",
				use_authored_weld ? "IN USE" : "absent - using exact-match heuristic",
				point_data ? point_data->vertex_point_indices.count : 0,
				info->total_vertex_count);
		}
	}

	for (uint16 vertex_index = 0; vertex_index < weld_vertex_count; vertex_index++)
	{
		real_point3d position;
		int32 local_node = 0;
		stencil_shadow_get_vertex(geometry, input->position_bounds,
			(uint16)info->geometry_compression_flags, vertex_index, &position, &local_node);

		// The per-vertex detail byte is a NODE index only on rigid_boned/skinned sections; on plain
		// rigid ones it can carry unrelated data, and trusting it there transforms vertices by
		// garbage bone matrices.
		uint8 model_node = 0;
		if (classification >= _geometry_classification_rigid_boned)
		{
			model_node = (uint8)local_node;
			if (node_map && VALID_INDEX(local_node, node_map_count))
			{
				model_node = node_map[local_node];
			}
			else if (!g_stencil_shadow_warned_no_node_map)
			{
				// The fallback binds every affected vertex to a DIFFERENT bone, since the default
				// above is the unmapped local index and the map is genuinely non-identity (a
				// measured example: locals 0-8 map to model nodes 0,1,2,3,6,10,14,15,16). It must
				// not be silent - the symptom reads as mistranslated animation.
				g_stencil_shadow_warned_no_node_map = true;
				event(_event_warning, "rasterizer:dx9:stencil:weld: class %d section has no usable node_map (count=%d) for local node %d - falling back to the LOCAL index, vertices will bind to the wrong nodes",
					(int32)classification, node_map_count, local_node);
			}
		}

		// tool.exe compares the full skinning payload, not just the dominant bone
		const uint8* weld_payload = has_bone_weights
			? stencil_shadow_get_skinned_vertex(geometry, vertex_index) : NULL;
		s_position_key key = stencil_shadow_position_key(&position, model_node, weld_payload);

		// group by the authored point index when available, else by the exact-match key
		uint16 existing_welded = 0xFFFF;
		if (use_authored_weld)
		{
			auto found_point = authored_lookup.find(authored_point_indices[vertex_index]);
			if (found_point != authored_lookup.end())
			{
				existing_welded = found_point->second;
			}
		}
		else
		{
			auto found = weld_lookup.find(key);
			if (found != weld_lookup.end())
			{
				existing_welded = found->second;
			}
		}

		if (existing_welded != 0xFFFF)
		{
			weld_map[vertex_index] = existing_welded;
		}
		else
		{
			uint16 welded_index = (uint16)welded_positions.size();
			if (use_authored_weld)
			{
				authored_lookup[authored_point_indices[vertex_index]] = welded_index;
			}
			else
			{
				weld_lookup[key] = welded_index;
			}
			weld_map[vertex_index] = welded_index;
			welded_positions.push_back(position);
			welded_nodes.push_back(model_node);
			if (model_node != welded_nodes[0])
			{
				mixed_nodes = true;
			}
			if (has_bone_weights)
			{
				const uint8* payload = stencil_shadow_get_skinned_vertex(geometry, vertex_index);
				real32 weight_total = 0.f;
				for (int32 i = 0; i < 4; i++)
				{
					int32 bone_local = payload[k_skinned_index_offset + i];
					uint8 bone_node = (uint8)bone_local;
					if (node_map && VALID_INDEX(bone_local, node_map_count))
					{
						bone_node = node_map[bone_local];
					}
					// Falling back to the unmapped local index binds the vertex to a different bone,
					// which reads as mistranslated animation. The two causes are reported apart because
					// they differ in scope: no node_map at all mis-binds every bone of every weighted
					// vertex here, while an index past the map's end mis-binds only that one.
					else if (!g_stencil_shadow_warned_bone_no_map)
					{
						g_stencil_shadow_warned_bone_no_map = true;
						event(_event_warning, "rasterizer:dx9:stencil:weld: 4-bone payload could not map local node %d (%s) - falling back to the LOCAL index, this vertex binds to the WRONG bone (class %d, node_map_count %d)",
							bone_local,
							node_map ? "index >= node_map_count" : "section has NO node_map",
							(int32)classification,
							node_map_count);
					}
					real32 weight = payload[k_skinned_weight_offset + i] * (1.f / 255.f);
					welded_bone_indices.push_back(bone_node);
					welded_bone_weights.push_back(weight);
					weight_total += weight;
				}
				// renormalise: the stored bytes sum to 255 +/- 1, and an exactly-1 total keeps
				// the blended point from creeping toward the origin
				if (weight_total > 0.f)
				{
					real32 scale = 1.f / weight_total;
					for (int32 i = 4; i > 0; i--)
					{
						welded_bone_weights[welded_bone_weights.size() - i] *= scale;
					}
				}
				else
				{
					welded_bone_weights[welded_bone_weights.size() - 4] = 1.f;
				}
			}
		}
	}

	// DEGENERATE-WELD GUARD. `geometry_section_get_compressed_vertex` (halo2.exe 0x675DD9) decodes
	// declarations 1, 2 and 3 and falls through to `return global_origin3d` for anything else, so a
	// section in a format we do not handle yields (0,0,0) for every vertex; they share one weld key
	// and collapse. Building from that is strictly worse than skipping it - the planes carry zero
	// normals, so the facing bitvector is meaningless and the volume is a degenerate fan at the
	// origin. Reject, and report the declaration so the unhandled format can be identified.
	if (welded_positions.size() <= 1 && info->shadow_casting_triangle_count > 0)
	{
		if (!g_stencil_shadow_warned_degenerate_weld)
		{
			g_stencil_shadow_warned_degenerate_weld = true;
			int32 decl = NONE, stride = NONE;
			if (geometry->vertex_buffers.count > 0 && geometry->vertex_buffers[0])
			{
				decl = (int32)geometry->vertex_buffers[0]->declaration;
				stride = (int32)geometry->vertex_buffers[0]->stride;
			}
			event(_event_warning, "rasterizer:dx9:stencil:weld: section welded to %u vertices from %u verts (class=%d declaration=%d stride=%d) - position decode failed, section skipped",
				(uint32)welded_positions.size(), info->total_vertex_count,
				(int32)classification, decl, stride);
		}
		g_stencil_shadow_build_fail = "position-decode";
		return false;
	}
	return true;
}

static bool stencil_shadow_build_enumerate_triangles(
	const s_stencil_shadow_build_input* input,
	s_stencil_shadow_build_state* state)
{
	const geometry_section_info* info = input->section_info;
	geometry_section* geometry = input->geometry;
	const uint16* strip_indices = geometry->strip_indices[0];
	const std::vector<uint16>& weld_map = state->weld_map;
	const std::vector<real_point3d>& welded_positions = state->welded_positions;
	std::vector<uint16>& triangles = state->triangles;
	std::vector<real_plane3d>& planes = state->planes;
	std::vector<s_stencil_shadow_rigid_group>& groups = state->groups;
	std::vector<uint16>& triangle_parts = state->triangle_parts;
	int32& casting_part_count = state->casting_part_count;

	// 2. Enumerate shadow-casting triangles part by part, walking each triangle strip with
	// degenerate skip and parity winding (matches tag-debug strip walk + tool plane order).
	// owning part per triangle - the pass-0 pairing below requires partners to share a part
	// (our analogue of td's "same material" constraint)

	// Tag debug selects casting parts BY COUNT, not by type: its cap loop walks
	// [0, shadow_casting_part_count) and never inspects part->type, because the tool sorts casting
	// parts to the front and records how many there are. Testing part->type instead is unsafe -
	// geometry_postprocess remaps part types {0,1,2,3} -> {2,1,3,4} exactly once, guarded by a flag
	// written back into the tag, so a part still in the old numbering reads as not_drawn (we skip a
	// caster) or opaque_nonshadowing (we sweep in a non-caster, widening every silhouette).
	casting_part_count = info->shadow_casting_part_count;
	if (casting_part_count > geometry->parts.count)
	{
		casting_part_count = geometry->parts.count;
	}
	// That rests on Vista's cache populating shadow_casting_part_count and on the tool having sorted
	// casting parts to the front. If the count reads 0 while type-based selection can still find
	// casters, the by-count route silently builds nothing at all, so fall back to the type test and
	// say so. Clusters take tag debug's CLUSTER rule instead - every part, filtered by type.
	bool use_type_filter = false;
	if (input->select_parts_by_type)
	{
		use_type_filter = true;
		casting_part_count = geometry->parts.count;
	}
	else if (casting_part_count <= 0)
	{
		int32 type_selected = 0;
		for (int32 i = 0; i < geometry->parts.count; i++)
		{
			if (stencil_shadow_part_casts(geometry->parts[i]))
			{
				type_selected++;
			}
		}
		if (type_selected > 0)
		{
			if (!g_stencil_shadow_warned_no_shadow_parts)
			{
				g_stencil_shadow_warned_no_shadow_parts = true;
				event(_event_status, "rasterizer:dx9:stencil:build: shadow_casting_part_count is 0 but %d parts pass the type test - falling back to type selection (the by-count rule does not hold on this cache)",
					type_selected);
			}
			use_type_filter = true;
			casting_part_count = geometry->parts.count;
		}
	}
	for (int32 part_index = 0; part_index < casting_part_count; part_index++)
	{
		const geometry_part* part = geometry->parts[part_index];
		if (use_type_filter && !stencil_shadow_part_casts(part))
		{
			continue;		// fallback path only - td selects purely by count
		}

		uint32 part_triangle_start = (uint32)planes.size();
		// the FIRST-ORDER guard. `strip_start_index` and `strip_length` are both tag
		// data and neither was validated against the strip block, so a corrupt part reads past
		// the end of `strip_indices` before its garbage values ever reach the weld_map check
		// below. One test per PART, outside the triangle loop, so the cost is nil.
		if ((int32)part->strip_start_index < 0
			|| (int32)part->strip_length < 0
			|| (int32)part->strip_start_index + (int32)part->strip_length > geometry->strip_indices.count)
		{
			if (!g_stencil_shadow_warned_bad_strip)
			{
				g_stencil_shadow_warned_bad_strip = true;
				event(_event_warning, "rasterizer:dx9:stencil:build: part strip range %d..%d exceeds the %d-entry strip block - corrupt tag data, part skipped",
					(int32)part->strip_start_index,
					(int32)part->strip_start_index + (int32)part->strip_length,
					geometry->strip_indices.count);
			}
			continue;
		}
		for (int32 strip_position = 0; strip_position + 2 < part->strip_length; strip_position++)
		{
			uint16 index_0 = strip_indices[part->strip_start_index + strip_position];
			uint16 index_1 = strip_indices[part->strip_start_index + strip_position + 1];
			uint16 index_2 = strip_indices[part->strip_start_index + strip_position + 2];

			// degenerate strip stitching triangles
			if (index_0 == index_1 || index_1 == index_2 || index_0 == index_2)
			{
				continue;
			}

			// odd strip positions flip winding
			if (strip_position & 1)
			{
				uint16 swap = index_0;
				index_0 = index_1;
				index_1 = swap;
			}

			// `index_N` is an arbitrary uint16 straight out of tag strip data while `weld_map` holds
			// only `total_vertex_count` entries, so a corrupt strip indexes far off the end. The
			// `k_unwelded` test below catches a bad result, but only after the read has happened.
			if ((size_t)index_0 >= weld_map.size()
				|| (size_t)index_1 >= weld_map.size()
				|| (size_t)index_2 >= weld_map.size())
			{
				if (!g_stencil_shadow_warned_bad_strip)
				{
					g_stencil_shadow_warned_bad_strip = true;
					event(_event_warning, "rasterizer:dx9:stencil:build: strip index out of range (%u/%u/%u vs %u vertices) - corrupt strip data, triangle dropped",
						index_0, index_1, index_2, (uint32)weld_map.size());
				}
				continue;
			}
			uint16 welded_0 = weld_map[index_0];
			uint16 welded_1 = weld_map[index_1];
			uint16 welded_2 = weld_map[index_2];
			// A shadow triangle naming a vertex outside the opaque range contradicts the
			// part-type argument above (casting parts are opaque_*), so report it rather than
			// emitting a triangle against an unwelded index.
			if (welded_0 == k_unwelded || welded_1 == k_unwelded || welded_2 == k_unwelded)
			{
				if (!g_stencil_shadow_warned_unwelded)
				{
					g_stencil_shadow_warned_unwelded = true;
					event(_event_warning, "rasterizer:dx9:stencil:build: shadow triangle references a non-opaque vertex (opaque=%u total=%u) - triangle dropped",
						state->weld_vertex_count, info->total_vertex_count);
				}
				continue;
			}

			// DEGENERATE AFTER WELDING. The strip-stitch test above compares RAW indices, which is all
			// that exists at that point, but `weld_map` can send two distinct raw indices to the same
			// welded vertex - and then a triangle that was fine in the strip has zero area here. That
			// is not merely wasteful: `plane.n` below is a cross product of two parallel edges, so the
			// normal is zero, the facing test takes the sign of 0, and the bit flips frame to frame,
			// making every edge the triangle touches an intermittent silhouette. Models barely weld
			// (their coincident vertices are split by node or skinning payload), but BSP clusters weld
			// purely by position and world geometry is full of coincident vertices.
			if (welded_0 == welded_1 || welded_1 == welded_2 || welded_0 == welded_2)
			{
				continue;
			}

			const real_point3d* p0 = &welded_positions[welded_0];
			const real_point3d* p1 = &welded_positions[welded_1];
			const real_point3d* p2 = &welded_positions[welded_2];

			// plane: N = cross(p0 - p1, p0 - p2), d = dot(N, p0)
			// (identical math to tag-debug isq_object_do_skinning_work soft planes)
			real_vector3d edge_1 = { p0->x - p1->x, p0->y - p1->y, p0->z - p1->z };
			real_vector3d edge_2 = { p0->x - p2->x, p0->y - p2->y, p0->z - p2->z };

			real_plane3d plane;
			plane.n.i = edge_2.k * edge_1.j - edge_1.k * edge_2.j;
			plane.n.j = edge_1.k * edge_2.i - edge_2.k * edge_1.i;
			plane.n.k = edge_2.j * edge_1.i - edge_1.j * edge_2.i;
			plane.d = plane.n.i * p0->x + plane.n.j * p0->y + plane.n.k * p0->z;

			planes.push_back(plane);
			triangles.push_back(welded_0);
			triangles.push_back(welded_1);
			triangles.push_back(welded_2);
			triangle_parts.push_back((uint16)part_index);

			if (planes.size() >= k_stencil_shadow_maximum_planes_per_section)
			{
				// Bounds the facing bitvector (k_stencil_shadow_facing_bitvector_words is
				// sized from this constant), so the cap is required -- but dropping the
				// remaining triangles silently yields a volume with a hole in it, which is
				// indistinguishable from a topology bug. Say it once.
				if (!g_stencil_shadow_warned_plane_cap)
				{
					g_stencil_shadow_warned_plane_cap = true;
					event(_event_warning, "rasterizer:dx9:stencil:build: section exceeded %u planes - remaining shadow triangles dropped",
						(uint32)k_stencil_shadow_maximum_planes_per_section);
				}
				break;
			}
		}

		uint32 part_triangle_count = (uint32)planes.size() - part_triangle_start;
		if (part_triangle_count > 0)
		{
			// P1: rigid sections only -> one hard group per part, node 0
			// (P3: skinned sections emit soft groups {0xFF, part_index} instead)
			s_stencil_shadow_rigid_group group;
			group.node = 0;
			group.part_index = (uint8)part_index;
			group.plane_count = (uint16)part_triangle_count;
			groups.push_back(group);
		}
	}

	if (planes.empty())
	{
		// split by whether the section DECLARES any shadow-casting triangles. A section
		// with shadow_casting_triangle_count == 0 producing no planes is entirely benign and is
		// expected to be common - it must not be counted against the boundary gate. Zero planes
		// despite a non-zero declared count is a real strip-walk failure and a genuine defect.
		g_stencil_shadow_build_fail = info->shadow_casting_triangle_count == 0
			? "no-shadow-tris-declared"
			: "no-planes-DESPITE-declared-tris";
		return false;
	}
	return true;
}

static bool stencil_shadow_build_pair_edges(
	const s_stencil_shadow_build_input* input,
	s_stencil_shadow_build_state* state)
{
	const std::vector<uint16>& triangles = state->triangles;
	const std::vector<real_plane3d>& planes = state->planes;
	const std::vector<uint16>& triangle_parts = state->triangle_parts;
	std::vector<s_stencil_shadow_quad>& quads = state->quads;
	uint32& boundary_edge_count = state->boundary_edge_count;
	uint32& total_edge_refs = state->total_edge_refs;
	uint32& same_winding_candidates = state->same_winding_candidates;

	// 3. Edge adjacency -> silhouette quads, mirroring tool.exe's manifold pairer: collect ALL
	// triangle references for an edge, then pair them requiring OPPOSITE WINDING, over three passes
	// of decreasing strictness - pass 0 requires a shared material, pass 1 drops that, pass 2 lets a
	// triangle pair with itself (fold-back edges). Our material analogue is the PART.
	//
	// DELIBERATE DEVIATION: the tool DELETES whatever stays unpaired; we sentinel-close it. The tool
	// can delete because it pairs section seams at tag time through four authored blocks, so anything
	// still unpaired there is a genuine hole. We have no such data, so our unpaired set is real holes
	// plus every seam, and deleting it would open the volume at every section boundary. When seam
	// stitching is enabled, `stencil_shadow_model_cross_get` retags each edge it bridges so this walk
	// skips it - the two mechanisms must never close the same seam twice.
	struct s_edge_ref
	{
		uint16 triangle;
		uint16 part;
		uint16 vert_a;		// as traversed by THIS triangle (encodes the winding)
		uint16 vert_b;
		bool consumed;
	};
	std::unordered_map<uint32, std::vector<s_edge_ref>> edges;

	uint32 triangle_count = (uint32)planes.size();
	for (uint32 triangle_index = 0; triangle_index < triangle_count; triangle_index++)
	{
		uint16 owning_part = triangle_parts[triangle_index];
		for (int32 edge_slot = 0; edge_slot < 3; edge_slot++)
		{
			uint16 vert_a = triangles[triangle_index * 3 + edge_slot];
			uint16 vert_b = triangles[triangle_index * 3 + (edge_slot + 1) % 3];
			uint32 edge_key = vert_a < vert_b
				? ((uint32)vert_a << 16) | vert_b
				: ((uint32)vert_b << 16) | vert_a;
			s_edge_ref reference = { (uint16)triangle_index, owning_part, vert_a, vert_b, false };
			edges[edge_key].push_back(reference);
		}
	}

	boundary_edge_count = 0;
	same_winding_candidates = 0;
	// The denominator for the reported boundary fraction: edge REFERENCES, not distinct edge keys.
	// Dividing by keys overstates the unpaired fraction whenever a key carries several triangles.
	total_edge_refs = 0;
	for (auto& count_entry : edges)
	{
		total_edge_refs += (uint32)count_entry.second.size();
	}
	for (auto& entry : edges)
	{
		std::vector<s_edge_ref>& refs = entry.second;

		// The tool builds a single-pass edge registry and does not constrain pairing by material at
		// all. Ours is a different construction reaching the same output: the pass order only decides
		// which triangle lands in tri_left vs tri_right, and emission is symmetric under swapping
		// those roles, since vert_a/vert_b come from the right reference's traversal and the draw path
		// swaps them when the right triangle faces.
		for (int32 pass = 0; pass < 3; pass++)
		{
			for (uint32 i = 0; i < refs.size(); i++)
			{
				if (refs[i].consumed)
				{
					continue;
				}
				for (uint32 j = i + 1; j < refs.size(); j++)
				{
					if (refs[j].consumed)
					{
						continue;
					}
					// opposite winding: j traverses the edge the other way round
					if (refs[i].vert_a != refs[j].vert_b || refs[i].vert_b != refs[j].vert_a)
					{
						continue;
					}
					if (refs[i].part != refs[j].part && pass < 1)
					{
						continue;		// material/part requirement, relaxed at pass 1
					}
					if (refs[i].triangle == refs[j].triangle && pass < 2)
					{
						continue;		// self-pairing, allowed at pass 2
					}

					// EMISSION INVARIANT (matched to the caps' convention): emitted order =
					// REVERSE of the FACING triangle's traversal - store the second
					// triangle's traversal; no swap when the first faces, swap when the
					// second does.
					s_stencil_shadow_quad quad;
					quad.vert_a = refs[j].vert_a;
					quad.vert_b = refs[j].vert_b;
					quad.tri_left = refs[i].triangle;
					quad.tri_right = refs[j].triangle;
					quads.push_back(quad);
					refs[i].consumed = true;
					refs[j].consumed = true;
					break;
				}
			}
		}

		// Diagnostic: references left unpaired because another traverses the same edge the SAME way
		// round. The pairing above matches only opposite-winding references, so on an inconsistently
		// wound mesh these sentinel-close as though the mesh were open there. The tool's edge record
		// carries a reversed bit instead, implying it pairs them and suppresses the emission swap -
		// this counts how much geometry that difference touches.
		for (uint32 i = 0; i < refs.size(); i++)
		{
			if (refs[i].consumed)
			{
				continue;
			}
			for (uint32 j = i + 1; j < refs.size(); j++)
			{
				if (!refs[j].consumed
					&& refs[i].vert_a == refs[j].vert_a
					&& refs[i].vert_b == refs[j].vert_b)
				{
					same_winding_candidates++;
					break;
				}
			}
		}

		// whatever could not be paired closes itself (reversed of its own traversal)
		for (uint32 i = 0; i < refs.size(); i++)
		{
			if (refs[i].consumed)
			{
				continue;
			}
			boundary_edge_count++;
			s_stencil_shadow_quad sentinel;
			sentinel.vert_a = refs[i].vert_b;
			sentinel.vert_b = refs[i].vert_a;
			sentinel.tri_left = refs[i].triangle;
			sentinel.tri_right = k_stencil_shadow_boundary_triangle;
			quads.push_back(sentinel);
		}
	}

	// A BOUNDARY-EDGE QUALITY GATE OF OUR OWN - tag debug has no equivalent. Its
	// render_model_check_shadow_manifold is a pure tag-data test over authored section-pair bits
	// that rejects a whole MODEL and never looks at an edge; that one is ported faithfully at the
	// object level as stencil_shadow_model_is_manifold. This is an additional rejection with an
	// arbitrary threshold, and a section dropped here casts nothing at all - on a biped that removes
	// a whole body part's shadow rather than degrading it. At least it reports rather than failing
	// silently.
	//
	// The threshold is per SOURCE: clusters run far more open than models, since a BSP partition is
	// cut at its portals, so they get a looser limit - but they do get a gate, because a cluster at
	// 70% boundary edges is a broken weld rather than an open partition, and without one the tier
	// draws garbage silently.
	if (input->boundary_reject_percent > 0
		&& boundary_edge_count * 100 > (uint32)edges.size() * (uint32)input->boundary_reject_percent)
	{
		event(_event_status, "rasterizer:dx9:stencil:build: section rejected (non-manifold: %u/%u boundary edges, limit %d%%)",
			boundary_edge_count, (uint32)edges.size(), input->boundary_reject_percent);
		g_stencil_shadow_build_fail = "boundary-gate";
		return false;
	}
	return true;
}

// Returns the created buffer, or NULL with g_stencil_shadow_build_fail set. The caller owns it
// from here: stage 5 either commits it into the section or releases it.
static IDirect3DVertexBuffer9* stencil_shadow_build_vertex_buffer(
	const s_stencil_shadow_build_input* input,
	s_stencil_shadow_build_state* state)
{
	const int32 classification = input->global_geometry_classification;
	const bool mixed_nodes = state->mixed_nodes;
	const std::vector<real_point3d>& welded_positions = state->welded_positions;
	bool& articulated = state->articulated;

	// 4. Shadow vertex buffer: doubled welded verts {pos, 0} / {pos, 1}
	IDirect3DDevice9Ex* device = rasterizer_dx9_device_get_interface();
	if (!device)
	{
		g_stencil_shadow_build_fail = "no-d3d-device";
		return NULL;
	}

	// articulated sections (mixed per-vertex nodes, or fully skinned approximated by their
	// dominant node) get a DYNAMIC VB refreshed per draw; static sections stay write-once
	articulated = mixed_nodes
		|| classification == _geometry_classification_skinned;

	// The build-fail string names WHICH step failed; rasterizer_dx9_log_hr adds why, decoded.
	uint32 vb_vertex_count = (uint32)welded_positions.size() * 2;
	IDirect3DVertexBuffer9* shadow_vb = NULL;
	HRESULT hr;
	rasterizer_dx9_log_hr(
		hr,
		device->CreateVertexBuffer(
			vb_vertex_count * sizeof(s_stencil_shadow_vertex),
			articulated ? (D3DUSAGE_WRITEONLY | D3DUSAGE_DYNAMIC) : D3DUSAGE_WRITEONLY,
			0,
			D3DPOOL_DEFAULT,
			&shadow_vb,
			NULL)
	);
	if (FAILED(hr))
	{
		g_stencil_shadow_build_fail = "vb-create-failed";
		return NULL;
	}

	s_stencil_shadow_vertex* vb_data = NULL;
	rasterizer_dx9_log_hr(
		hr,
		shadow_vb->Lock(0, 0, (void**)&vb_data, articulated ? D3DLOCK_DISCARD : 0)
	);
	if (FAILED(hr))
	{
		shadow_vb->Release();
		g_stencil_shadow_build_fail = "vb-lock-failed";
		return NULL;
	}
	for (uint32 welded_index = 0; welded_index < welded_positions.size(); welded_index++)
	{
		vb_data[welded_index * 2].position = welded_positions[welded_index];
		vb_data[welded_index * 2].extrude = 0.f;
		vb_data[welded_index * 2 + 1].position = welded_positions[welded_index];
		vb_data[welded_index * 2 + 1].extrude = 1.f;
	}
	shadow_vb->Unlock();
	return shadow_vb;
}

static bool stencil_shadow_build_commit(
	const s_stencil_shadow_build_input* input,
	s_stencil_shadow_build_state* state,
	IDirect3DVertexBuffer9* shadow_vb,
	s_stencil_shadow_section* out_shadow)
{
	geometry_section* geometry = input->geometry;
	const bool has_bone_weights = state->has_bone_weights;
	const std::vector<real_point3d>& welded_positions = state->welded_positions;
	const std::vector<uint8>& welded_nodes = state->welded_nodes;
	const std::vector<uint8>& welded_bone_indices = state->welded_bone_indices;
	const std::vector<real32>& welded_bone_weights = state->welded_bone_weights;
	const std::vector<uint16>& triangles = state->triangles;
	const std::vector<real_plane3d>& planes = state->planes;
	const std::vector<s_stencil_shadow_rigid_group>& groups = state->groups;
	const std::vector<s_stencil_shadow_quad>& quads = state->quads;
	const std::vector<uint32>& quad_same_winding = state->quad_same_winding;
	const bool articulated = state->articulated;
	const uint32 triangle_count = (uint32)planes.size();
	IDirect3DDevice9Ex* device = rasterizer_dx9_device_get_interface();

	// 5. Commit to Cartographer-owned allocations
	out_shadow->plane_count = triangle_count;
	out_shadow->planes = new real_plane3d[triangle_count];
	memcpy(out_shadow->planes, planes.data(), triangle_count * sizeof(real_plane3d));
	out_shadow->planes_soa = new real32[((triangle_count + 3) / 4) * 16];
	stencil_shadow_planes_fill_soa(out_shadow);

	out_shadow->group_count = (uint32)groups.size();
	out_shadow->groups = new s_stencil_shadow_rigid_group[groups.size()];
	memcpy(out_shadow->groups, groups.data(), groups.size() * sizeof(s_stencil_shadow_rigid_group));

	out_shadow->triangles = new uint16[triangle_count * 3];
	memcpy(out_shadow->triangles, triangles.data(), triangle_count * 3 * sizeof(uint16));

	out_shadow->quad_count = (uint32)quads.size();
	out_shadow->quads = new s_stencil_shadow_quad[quads.size()];
	memcpy(out_shadow->quads, quads.data(), quads.size() * sizeof(s_stencil_shadow_quad));
	out_shadow->quad_same_winding_bits = new uint32[(quads.size() + 31) / 32]();
	memcpy(out_shadow->quad_same_winding_bits, quad_same_winding.data(),
		quad_same_winding.size() * sizeof(uint32));

	out_shadow->shadow_vb = shadow_vb;
	out_shadow->welded_vertex_count = (uint32)welded_positions.size();

	out_shadow->articulated = articulated;
	{
		// base positions kept for every section: cross-section seam matching reconstructs
		// bind-pose endpoints from them (articulated sections also animate from them)
		uint32 welded_count = (uint32)welded_positions.size();
		out_shadow->base_positions = new real_point3d[welded_count];
		memcpy(out_shadow->base_positions, welded_positions.data(), welded_count * sizeof(real_point3d));
		out_shadow->vertex_nodes = new uint8[welded_count];
		memcpy(out_shadow->vertex_nodes, welded_nodes.data(), welded_count);
		if (has_bone_weights && welded_bone_indices.size() == welded_count * 4)
		{
			out_shadow->vertex_bone_indices = new uint8[welded_count * 4];
			out_shadow->vertex_bone_weights = new real32[welded_count * 4];
			memcpy(out_shadow->vertex_bone_indices, welded_bone_indices.data(), welded_count * 4);
			memcpy(out_shadow->vertex_bone_weights, welded_bone_weights.data(),
				welded_count * 4 * sizeof(real32));
		}
		if (articulated)
		{
			out_shadow->world_positions = new real_point3d[welded_count];
		}
		// Cache the section's node_map for the skinning-pool palette path: the pool stores per-region
		// palettes in LOCAL node_map order on Vista content, and the tag's own copy may not be
		// resident at draw time.
		if (input->node_map && input->node_map_count > 0)
		{
			out_shadow->pool_node_map = new uint8[input->node_map_count];
			memcpy(out_shadow->pool_node_map, input->node_map, input->node_map_count);
			out_shadow->pool_node_map_count = input->node_map_count;
		}

		// The GPU-skinning STATIC VB for articulated sections: written once at bind pose with the
		// palette indices and weights baked in, so the c50 palette poses it per draw and the
		// per-frame lock-and-rewrite retires. Eligible when a node_map exists, fits the c50 palette
		// window (67 slots clears c254/c255), and every node a vertex binds resolves to a map slot -
		// any miss abandons the skinned VB rather than mis-binding a vertex, leaving the section on
		// the CPU path with its dynamic VB still built and refreshed.
		if (articulated && out_shadow->pool_node_map
			&& out_shadow->pool_node_map_count > 0
		&& out_shadow->pool_node_map_count <= k_stencil_shadow_max_palette_nodes)
		{
			auto local_slot_of = [&](uint8 model_node) -> int32
			{
				for (int32 local = 0; local < out_shadow->pool_node_map_count; local++)
				{
					if (out_shadow->pool_node_map[local] == model_node)
					{
						return local;
					}
				}
				return NONE;
			};

			std::vector<s_stencil_shadow_skinned_vertex> skinned_vertices;
			skinned_vertices.resize(welded_count * 2);
			bool mapped = true;
			for (uint32 i = 0; i < welded_count && mapped; i++)
			{
				s_stencil_shadow_skinned_vertex vertex = {};
				vertex.position = welded_positions[i];

				if (has_bone_weights && welded_bone_indices.size() == (size_t)welded_count * 4)
				{
					// full 4-bone payload. Weights are quantized to ubyte4n summing EXACTLY 255 -
					// the residue goes to the heaviest lane, so the blend never scales the vertex.
					int32 weight_bytes[4];
					int32 weight_total = 0;
					int32 heaviest = 0;
					for (int32 bone = 0; bone < 4; bone++)
					{
						const real32 weight = welded_bone_weights[i * 4 + bone];
						weight_bytes[bone] = MAX((int32)(weight * 255.f + 0.5f), 0);
						weight_total += weight_bytes[bone];
						if (weight_bytes[bone] > weight_bytes[heaviest])
						{
							heaviest = bone;
						}
					}
					weight_bytes[heaviest] += 255 - weight_total;
					for (int32 bone = 0; bone < 4; bone++)
					{
						if (weight_bytes[bone] <= 0)
						{
							continue;	// lane stays {0,0} - weight 0 reads palette[0] harmlessly
						}
						const int32 local = local_slot_of(welded_bone_indices[i * 4 + bone]);
						if (local == NONE)
						{
							mapped = false;
							break;
						}
						vertex.indices[bone] = (uint8)(local * 3);
						vertex.weights[bone] = (uint8)weight_bytes[bone];
					}
				}
				else
				{
					// dominant-node section (rigid_boned with mixed nodes): one bone, full weight
					const int32 local = local_slot_of(welded_nodes[i]);
					if (local == NONE)
					{
						mapped = false;
						break;
					}
					vertex.indices[0] = (uint8)(local * 3);
					vertex.weights[0] = 255;
				}

				vertex.extrude = 0.f;
				skinned_vertices[i * 2] = vertex;
				vertex.extrude = 1.f;
				skinned_vertices[i * 2 + 1] = vertex;
			}

			if (mapped)
			{
				IDirect3DVertexBuffer9* skinned_vb = NULL;
				if (SUCCEEDED(device->CreateVertexBuffer(
					welded_count * 2 * sizeof(s_stencil_shadow_skinned_vertex),
					D3DUSAGE_WRITEONLY, 0, D3DPOOL_DEFAULT, &skinned_vb, NULL)))
				{
					void* skinned_data = NULL;
					if (SUCCEEDED(skinned_vb->Lock(0, 0, &skinned_data, 0)))
					{
						memcpy(skinned_data, skinned_vertices.data(),
							welded_count * 2 * sizeof(s_stencil_shadow_skinned_vertex));
						skinned_vb->Unlock();
						out_shadow->skinned_vb = skinned_vb;
					}
					else
					{
						skinned_vb->Release();
					}
				}
			}
		}
	}


	out_shadow->valid = true;
	// The section's own extent in the space its vertices live in. The drawn shadow can never be
	// smaller than this, so an extent that already dwarfs the object means the GEOMETRY is wrong
	// rather than the extrusion.
	{
		real_point3d lo = welded_positions[0], hi = welded_positions[0];
		for (uint32 i = 1; i < (uint32)welded_positions.size(); i++)
		{
			const real_point3d* p = &welded_positions[i];
			lo.x = MIN(lo.x, p->x);
			lo.y = MIN(lo.y, p->y);
			lo.z = MIN(lo.z, p->z);
			hi.x = MAX(hi.x, p->x);
			hi.y = MAX(hi.y, p->y);
			hi.z = MAX(hi.z, p->z);
		}
		// tag debug's invariant is that the shadow geometry is exactly the first
		// shadow_casting_part_count parts, totalling shadow_casting_triangle_count triangles, so
		// planes != expect means the wrong set is being swept in.
		int32 type_selected_parts = 0;
		// Two casting part types exist: opaque_shadow_ONLY is a proxy mesh that casts but never
		// renders, opaque_shadow_casting is the visible mesh. Which one the cache carries decides
		// what the silhouette is built from - a purpose-built closed proxy, or render geometry that
		// frequently is not closed, which would also explain non-manifold rejections.
		int32 shadow_only_parts = 0;
		for (int32 i = 0; i < geometry->parts.count; i++)
		{
			const geometry_part* p = geometry->parts[i];
			if (stencil_shadow_part_casts(p))
			{
				type_selected_parts++;
			}
			if (p->type == _geometry_part_type_opaque_shadow_only)
			{
				shadow_only_parts++;
			}
		}
		// The per-section report. boundary carries its own denominator so the manifold gate's MARGIN
		// is readable rather than just its verdict, and weld names which weld ran, since that is a
		// per-section property - a section on the heuristic legitimately reports different verts and
		// boundary counts than one on the authored map, which would otherwise read as a pairing bug.
		event(_event_verbose, "rasterizer:dx9:stencil:build: verts=%u planes=%u (expect %u) parts=%d/%d (by_type %d, shadow_only %d) class=%d extent=(%.3f x %.3f x %.3f) quads=%u boundary=%u/%u refs (unpaired fraction) samewind=%u weld=%s decomp=%s",
			out_shadow->welded_vertex_count, out_shadow->plane_count,
			input->section_info->shadow_casting_triangle_count,
			state->casting_part_count, geometry->parts.count,
			type_selected_parts, shadow_only_parts,
			input->global_geometry_classification,
			hi.x - lo.x, hi.y - lo.y, hi.z - lo.z, out_shadow->quad_count,
			state->boundary_edge_count, state->total_edge_refs, state->same_winding_candidates,
			state->use_authored_weld ? "authored" : "heuristic",
			// decomp: applied = normalized positions were decompressed against resolved bounds;
			// not-normalized = they were already world-space; MISSING-BOUNDS = normalized with no
			// compression_info anywhere, so THIS section stays oversized.
			((uint16)input->section_info->geometry_compression_flags & 1) == 0
				? "not-normalized"
				: (input->position_bounds ? "applied" : "MISSING-BOUNDS"));
	}
	stencil_shadow_section_validate(out_shadow);
	return true;
}

// The generator proper. Both tiers reach it, but neither calls it: a render model section arrives
// through stencil_shadow_section_build below, a BSP cluster through stencil_shadow_cluster_get, and
// both of those are reached through the caches. Private for that reason - building outside the cache
// would rebuild per object and leak the vertex buffer each time.
static bool stencil_shadow_build_from_geometry(
	const s_stencil_shadow_build_input* input,
	s_stencil_shadow_section* out_shadow)
{
	memset(out_shadow, 0, sizeof(*out_shadow));
	g_stencil_shadow_build_fail = "unknown";	// overwritten by every failure path below

	if (!input || !input->section_info || !input->geometry)
	{
		g_stencil_shadow_build_fail = "no-resident-data";
		return false;
	}
	// The builder reads ONLY the input block, which is why a render model section and a BSP cluster
	// can share it rather than forking.
	if (!input->geometry->strip_indices[0])
	{
		g_stencil_shadow_build_fail = "no-strip-indices";
		return false;
	}

	s_stencil_shadow_build_state state = {};
	if (!stencil_shadow_build_weld_vertices(input, &state)
		|| !stencil_shadow_build_enumerate_triangles(input, &state)
		|| !stencil_shadow_build_pair_edges(input, &state))
	{
		return false;
	}

	IDirect3DVertexBuffer9* shadow_vb = stencil_shadow_build_vertex_buffer(input, &state);
	if (!shadow_vb)
	{
		return false;
	}
	return stencil_shadow_build_commit(input, &state, shadow_vb, out_shadow);
}

// The render-model adapter: fills the generic build input from a render_model_section. Private for
// the same reason as the generator - stencil_shadow_section_get is the cached way in.
static bool stencil_shadow_section_build(
	const render_model_section* section,
	render_model_section_data* resident_data,
	const real32* position_bounds,
	s_stencil_shadow_section* out_shadow)
{
	if (!resident_data)
	{
		memset(out_shadow, 0, sizeof(*out_shadow));
		g_stencil_shadow_build_fail = "no-resident-data";
		return false;
	}

	s_stencil_shadow_build_input input = {};
	input.section_info = &section->section_info;
	input.geometry = &resident_data->section;
	input.point_data = &resident_data->point_data;
	input.node_map = resident_data->node_map.count > 0 ? resident_data->node_map[0] : NULL;
	input.node_map_count = resident_data->node_map.count;
	input.global_geometry_classification = section->global_geometry_classification;
	input.rigid_node = section->rigid_node;
	input.position_bounds = position_bounds;
	input.boundary_reject_percent = 10;		// the model tier's long-standing threshold
	input.select_parts_by_type = false;		// models use the COUNT rule
	return stencil_shadow_build_from_geometry(&input, out_shadow);
}

// ~0.2mm grid absorbs the float noise of reconstructing the same seam vertex through two
// different nodes' inverse-bind transforms
static uint64 stencil_shadow_seam_key(const real_point3d* a, const real_point3d* b)
{
	auto quantize = [](const real_point3d* p) -> uint64
	{
		int32 qx = (int32)floorf(p->x * 4096.f + 0.5f);
		int32 qy = (int32)floorf(p->y * 4096.f + 0.5f);
		int32 qz = (int32)floorf(p->z * 4096.f + 0.5f);
		uint64 hash = (uint64)(uint32)qx * 73856093ull;
		hash ^= (uint64)(uint32)qy * 19349663ull;
		hash ^= (uint64)(uint32)qz * 83492791ull;
		return hash;
	};
	uint64 ka = quantize(a);
	uint64 kb = quantize(b);
	// unordered pair
	return ka < kb ? (ka * 1000003ull) ^ kb : (kb * 1000003ull) ^ ka;
}

// MAXIMUM_CLUSTERS_PER_STRUCTURE is 512 and MAXIMUM_STRUCTURE_BSPS_PER_SCENARIO is 16, so 16 bits of
// cluster and 8 of bsp index is comfortably wide. The whole cache is cleared on map unload, so a bsp
// index cannot alias across scenarios.
static uint32 stencil_shadow_cluster_key(int16 bsp_index, int32 cluster_index)
{
	return (((uint32)(uint16)bsp_index) << 16) | ((uint32)cluster_index & 0xFFFF);
}
