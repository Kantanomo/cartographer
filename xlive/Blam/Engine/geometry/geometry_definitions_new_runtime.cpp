#include "stdafx.h"
#include "geometry_definitions_new_runtime.h"

#include "rasterizer/dx9/rasterizer_dx9.h"
#include "rasterizer/dx9/rasterizer_dx9_main.h"
#include "cache/cache_files.h"
#include "cache/pc_geometry_cache.h"
#include "memory/data.h"
#include "math/matrix_math.h"
#include "models/models.h"
#include "render/render_lod_new.h"
#include "structures/structure_bsp_definitions.h"
#include "H2MOD/Modules/h2log/h2log.h"

#include <unordered_map>
#include <vector>

/* globals */

// PER-MAP DIAGNOSTIC BUDGETS — FILE SCOPE ON PURPOSE, and reset by
// stencil_shadow_generation_cache_clear.
//
// it. 310 established the rule the hard way: as function-local statics these capped per PROCESS, so
// whichever map loaded first consumed every budget and later maps were never described at all —
// which is why it. 226 could only find `vbuf:` data in a *previous session's* log. it. 330 and
// it. 353 then swept four more latches out of function scope for the same reason, and it. 480 a
// further pair that the `static bool` search had missed because they are int32.
//
// So: a latch that reports a condition ONCE must be file scope and must appear in the reset below.
// A new one that is function-static will silently describe only the first map of a session.
static int32 g_stencil_shadow_logged_point_data = 0;			// caps `stencil pointdata:` at 8 sections
static uint8 g_stencil_shadow_logged_classification[8] = {};	// caps `stencil vbuf:` at 4 per class
// This latch NAMES an unhandled vertex declaration in the degenerate-weld warning, which is how
// declaration 6 was found (it. 226) — process-lifetime suppression would hide a *different*
// unhandled format on a second map.
static bool g_stencil_shadow_warned_degenerate_weld = false;
static bool g_stencil_shadow_warned_no_definition = false;
static bool g_stencil_shadow_warned_no_section_data = false;
static bool g_stencil_shadow_warned_no_bounds = false;
static bool g_stencil_shadow_warned_section_bounds = false;
static bool g_stencil_shadow_logged_authored_weld = false;
static bool g_stencil_shadow_warned_no_shadow_parts = false;
static bool g_stencil_shadow_warned_unwelded = false;
// A CAPACITY CAP biting. td-INDEX.md lists "did a capacity cap ever bite?" as a question the next
// run is supposed to answer, so a cap that bit harder on a later map must not be silent.
static bool g_stencil_shadow_warned_plane_cap = false;
static bool g_stencil_shadow_warned_no_node_map = false;
// it. 494: the 4-bone payload's own node_map fallback — a separate latch from the single-node one
// above so both can report; they cover different branches of the same defect.
static bool g_stencil_shadow_warned_bone_no_map = false;
static bool g_stencil_shadow_warned_normalized_no_bounds = false;
static bool g_stencil_shadow_probed_render_only_used = false;
static bool g_stencil_shadow_warned_bad_strip = false;
// it. 472: WHY the last section build failed. `stencil_shadow_section_build` has EIGHT distinct
// `return false` paths and six of them were silent, so the `BUILD FAILED` line could not be
// attributed — and the it. 422 plan was to COUNT those lines as the gate's cost. Set on every
// failure path, printed by the BUILD FAILED line, so the tally can be filtered by cause.
static const char* g_stencil_shadow_build_fail = "unset";

// Published to the rasterizer's caster loop — see the header for the ownership warning.
const int8* g_stencil_shadow_render_only_flags = NULL;
int32 g_stencil_shadow_render_only_node_count = 0;

/* code */

// Fill the tag-debug SoA 4-block plane layout ([nx x4][ny x4][nz x4][d x4] per block) from
// the AoS planes — the movemask facing fast path consumes this. Blocks pad to 4 with zero
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

// Engine accessor (halo2.exe 0x675DD9): reads ALL vertex position formats — 12B float3,
// 16B float3+detail, and 8B compressed int16 normalized against the model's
// compression_info position_bounds (6 floats: x lo/hi, y lo/hi, z lo/hi). The 12B-only
// fetcher above fails on compressed sections (source of some nosec build skips).
int32 __cdecl geometry_section_get_compressed_vertex(
	geometry_section* section, const real32* position_bounds, int32 index,
	real_point3d* out_position, int32* out_detail)
{
	return INVOKE(0x275DD9, 0, geometry_section_get_compressed_vertex,
		section, position_bounds, index, out_position, out_detail);
}

// Position-stream declarations seen in Vista caches for vertex_buffers[0]:
//   1 -> float3                       (stride 12)  rigid
//   2 -> float3 + node byte + 3 pad   (stride 16)  rigid_boned
//   3 -> 3x int16 + node + pad        (stride 8)   compressed
//   4 -> float3 + 8 bytes skinning    (stride 20)  SKINNED
// geometry_section_get_compressed_vertex (halo2.exe 0x675DD9) handles only 1/2/3; declaration
// 4 hits its `default` and returns global_origin3d with detail 0. Every skinned section
// therefore read (0,0,0) for all of its vertices, welded to a single point and cast nothing --
// the long-standing "player shadow is only the head" (the head is a rigid_boned section, the
// body is skinned). Decode declaration 4 ourselves; fall through to the engine for the rest.
static const int32 k_vertex_declaration_skinned = 4;
// ...and its byte-identical siblings. Vista's declaration table (`vertex_declarations`
// @ halo2.exe 0x7E49F8, stride 21, indexed by rasterizer_vertex_buffer.declaration via
// rasterizer_vertex_get_declaration 0x67C78C) has entries 4, 5, 6 and 7 **byte-identical** apart
// from a leading self-index byte:
//
//     04 | 00 02 01 07 02 07 FF ...
//     06 | 00 02 01 07 02 07 FF ...        <- same bytes, different self-index
//
// so whatever the format means, 6 describes the same vertex layout as 4.
//
// NOTE (corrected it. 251): an earlier version of this comment glossed those bytes as
// "(usage, type) pairs -> float3 + two 4-byte elements = stride 20". **That decode is not
// supported** -- td-caps-draw.md records the declaration entry format as UNDECODED (its iteration-64
// correction shows byte 2 is not a register index, since the microcode reads v0/v3/v6 where the
// bytes say 0/9/12). The gloss was decoration, not evidence.
//
// The conclusion stands on two things that need no interpretation of the format:
//   1. the table entries for 4-7 are byte-identical, and
//   2. the live log reports stride 20 for declaration 4 AND declaration 6 on real sections.
// Do not extend this reasoning to other declarations by "reading" the pairs.
//
// Declaration 6 is live in
// the current map on a class-3 section, and because we accepted only 4, it fell through to
// geometry_section_get_compressed_vertex (halo2.exe 0x675DD9) -- which decodes ONLY 1/2/3 and
// returns global_origin3d (0,0,0) for anything else. The result was 509 vertices welding to ONE,
// zero extent, and 592 degenerate planes with 888 null quads (see td-declaration-6-unhandled.md).
//
// The stride check below is the real guard: accept these declarations only when the buffer actually
// measures 20 bytes per vertex, so a future table change cannot silently misparse.
// EXTENDED 7 -> 9 (it. 227 widened to 4-7; it. 348 to 8; it. 349 to 9 after reading the entry that had
// been SKIPPED). Grounded in the declaration table, and NOT in decoding the descriptor pairs (which
// td-caps-draw.md rightly warns against).
//
// `vertex_declarations` (halo2.exe 0x7E49F8, stride 21, via `rasterizer_vertex_get_declaration` 0x67C78C).
// Entries are `[decl_index][(a,b) pairs...][0xFF][zero pad]`. Dumped:
//
//   1        : (00,02)                     -> position only      (decoder forces out_detail = 0)
//   2, 3     : (00,02) (01,07)             -> position + detail
//   4,5,6,7,8,9 : (00,02) (01,07) (02,07)  -> BYTE-IDENTICAL, three elements
//   0        : empty
//   10 - 15  : NO (00,02) element          -> secondary streams, vertex_buffers[1+]
//
// The bound is **byte-identity with declaration 4**, whose stride-20 layout was confirmed from live data:
// entries 4-9 are byte-identical to each other and to nothing else in the table. That is the whole
// justification -- do NOT reason from "carries a position element".
//
// (it. 348 stopped at 8, claiming it "a real upper bound", without having read entry 9 -- which is
// identical to 4-8. it. 349 fixed the range but then claimed the position-bearing set was "exactly 1-9";
// it. 350 dumped the table through entry 51 and RETRACTED that too. `vertex_declarations` is the
// rasterizer's GLOBAL vertex-format registry -- 50+ entries, position-bearing ones at 19-22, 35-44, 48-51 --
// so carrying a position element says nothing about whether a declaration can appear on a section's
// vertex_buffers[0]. Which declarations actually occur there is EMPIRICAL: watch the `stencil vbuf:` log,
// which is how declaration 6 was found. See td-declaration-6-unhandled.md.)
//
// Rejecting one of these sends it to `geometry_section_get_compressed_vertex`, whose switch handles only
// 1/2/3 and returns `global_origin3d` -- the declaration-6 collapse of it. 226. The degenerate-weld guard
// catches and NAMES it rather than failing silently, but the section still casts nothing.
//
// The runtime **stride == 20** check below is the real guard and is unchanged. Byte-identical descriptors
// say these declarations share three semantic elements, NOT a storage layout -- the table provably cannot
// encode storage, since 2 and 3 are also byte-identical yet store float3+byte (stride 16) and
// 3 x int16+byte (stride 8).
static const int32 k_vertex_declaration_skinned_last = 9;
static const uint32 k_vertex_declaration_skinned_stride = 20;

static bool stencil_shadow_declaration_is_skinned(const rasterizer_vertex_buffer* buffer)
{
	const int32 declaration = (int32)buffer->declaration;
	return declaration >= k_vertex_declaration_skinned
		&& declaration <= k_vertex_declaration_skinned_last
		&& (uint32)buffer->stride == k_vertex_declaration_skinned_stride;
}

// it. 647 PROBE — is there a usable per-vertex NORMAL, and where?
//
// This is the measurement the cluster weld needs. Welding purely by position fuses surfaces that only
// TOUCH — a wall and a floor meeting at a corner — which is what leaves clusters at 65-75% unpaired
// edges and gets them rejected. The standard discriminator is the normal: same position + same normal
// is one surface, same position + different normal is a corner.
//
// The vbuf dump already narrowed it to one candidate. Cluster geometry carries five buffers:
//     [0] declaration=1  stride=12   position, float3   <- all we decode today
//     [1] declaration=23 stride=8
//     [2] declaration=25 stride=36   <- 36 == 3 x float3: normal, binormal, tangent
//     [3] declaration=29 stride=8
//     [4] declaration=47 stride=12
//
// 36 bytes reads as a tangent frame and the normal should be the first float3. "Should" is not
// evidence, so this TESTS it: a normal is unit length. If the reported lengths sit at ~1.000 the
// layout is confirmed and the weld fix is mechanical; if they do not, the guess is wrong and the
// remaining buffers have to be examined before anything is built on it.
//
// Deliberately reports min/max over EVERY vertex rather than sampling one — a single vertex reading
// 1.0 by coincidence is exactly the kind of confirmation that wastes a day.
static void stencil_shadow_probe_normal_stream(geometry_section* geometry, int32 vertex_count)
{
	for (int32 buffer_index = 0; buffer_index < geometry->vertex_buffers.count; buffer_index++)
	{
		const rasterizer_vertex_buffer* buffer = geometry->vertex_buffers[buffer_index];
		if (!buffer || !buffer->vertex_data || buffer->stride < 12 || buffer->count <= 0)
		{
			continue;
		}
		const int32 count = (vertex_count < buffer->count) ? vertex_count : buffer->count;
		if (count <= 0)
		{
			continue;
		}
		real32 min_length = 1e30f, max_length = -1e30f;
		for (int32 v = 0; v < count; v++)
		{
			const real32* p = (const real32*)((const uint8*)buffer->vertex_data + (size_t)v * buffer->stride);
			const real32 length = sqrtf(p[0] * p[0] + p[1] * p[1] + p[2] * p[2]);
			if (length < min_length) { min_length = length; }
			if (length > max_length) { max_length = length; }
		}
		const real32* first = (const real32*)buffer->vertex_data;
		UNREFERENCED_PARAMETER(first);

		LOG_INFO_GAME("stencil normalprobe: buffer[{}] decl={} stride={} first_float3_length={:.4f}..{:.4f} (1.000 == a NORMAL) sample=({:.3f},{:.3f},{:.3f})",
			buffer_index, (int32)buffer->declaration, (int32)buffer->stride,
			min_length, max_length, first[0], first[1], first[2]);
	}
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
	// it. 445: bound the index against the BUFFER's own count, not the caller's.
	//
	// Callers iterate to `weld_vertex_count`, which comes from `section_info` (opaque_vertex_count
	// / total_vertex_count), while the vertex buffer carries an INDEPENDENT `count`. Those are two
	// separate tag fields and nothing enforces that they agree. They do on every section measured
	// (84/84 on the biped's section 0, 401/401 on section 13, 319/319 on the it. 415 model), so
	// this is unreachable on valid data — but the value returned here is dereferenced for a full
	// 20-byte skinned vertex, so a disagreement reads off the end of the buffer.
	//
	// Same class as it. 443/444 and the same it. 347 rationale (Cartographer runs user-modified
	// maps). NULL is the correct response: it is already this function's "not a skinned vertex"
	// answer, and the caller falls through to `geometry_section_get_compressed_vertex` rather
	// than consuming a wild pointer.
	if (vertex_index < 0 || vertex_index >= (int32)buffer->count)
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

// Reads any buffer-0 position format. out_node is the engine's detail byte for declarations
// 1/2/3, and the DOMINANT (highest-weight) local node for declaration 4.
// POSITION DECOMPRESSION (it. 388). Halo 2 stores model positions NORMALIZED to [-1, +1] whenever
// `section_info.geometry_compression_flags & 1` is set, and reconstructs them **on the GPU**:
// `rasterizer_dx9_set_vertex_compression_constants` (halo2.exe 0x66F551), fed by
// `render_visible_section_set_vertex_compression` (0x6809C4), uploads
//     c170.xyz = (position_bounds.max - position_bounds.min) * 0.5    // half-extent
//     c171.xyz = (position_bounds.min + position_bounds.max) * 0.5    // centre
// and the vertex shader computes `p_model = p_norm * half_extent + centre`.
//
// We build volumes on the CPU, so we must do it ourselves. `geometry_section_get_compressed_vertex`
// (0x675DD9) only applies bounds in its **case 3** arm; cases 1 and 2 hand back the raw normalized
// floats, and our own stride-20 skinned decode reads raw floats too.
//
// Verified on live tag data (it. 387): a biped section with `geometry_compression_flags = 3` had vertex
// positions spanning [-1, +1] with exact 1.0 values, and decompressing a normalized x of 1.0 landed
// exactly on `position_bounds.x1`. Left undone, volumes come out anisotropically 3-6x oversized -- the
// reported symptom 1, and a LARGER error than the missing inverse bind.
//
// td needs no equivalent: its shadow path skins an authored, UNCOMPRESSED point array
// (`section_skin_from_rigid_point_groups`, 12 bytes per point), never the compressed vertex stream.
static void stencil_shadow_decompress_position(const real32* position_bounds, real_point3d* position)
{
	// bounds layout is [x_lo, x_hi, y_lo, y_hi, z_lo, z_hi] (confirmed it. 347 against case 3)
	position->x = position->x * ((position_bounds[1] - position_bounds[0]) * 0.5f)
		+ ((position_bounds[0] + position_bounds[1]) * 0.5f);
	position->y = position->y * ((position_bounds[3] - position_bounds[2]) * 0.5f)
		+ ((position_bounds[2] + position_bounds[3]) * 0.5f);
	position->z = position->z * ((position_bounds[5] - position_bounds[4]) * 0.5f)
		+ ((position_bounds[4] + position_bounds[5]) * 0.5f);
}

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

	// SECOND HALF OF THE it. 346 RULE, missed here (it. 417). The `&& position_bounds != NULL`
	// term is a guard, and its fallback is *the exact defect it. 388 was written to remove*:
	// the flags say the positions are normalized, we have no bounds to expand them with, so we
	// use the raw [-1, 1] values as if they were model space. Measured on this content (it. 415)
	// that inflates the volume by 6.3x in x, 5.4x in y and 2.8x in z -- i.e. symptom 1 returns in
	// full, silently, and looking exactly like the original bug rather than like missing data.
	//
	// The existing `warned_no_bounds` latch does NOT cover this: it fires only for
	// `declaration == 3 && !bounds` (the it. 347 crash guard, line ~2713), which is a different
	// and much narrower case. Declarations 1, 2 and 4-9 fall through here unreported.
	//
	// Diagnostic only -- deliberately does not change what is drawn. Without bounds there is no
	// correct answer available, and rejecting the section would trade a visible wrong shadow for
	// an invisible missing one, which is harder to notice and harder to attribute.
	if ((compression_flags & 1) != 0 && position_bounds == NULL && !g_stencil_shadow_warned_normalized_no_bounds)
	{
		g_stencil_shadow_warned_normalized_no_bounds = true;
		LOG_INFO_GAME("stencil WARNING: compression_flags={} says positions are NORMALIZED but no position_bounds are available — decompression skipped, this section's volume will be grossly oversized (~6x)",
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

// I5 — weld key matched to tool.exe's rule. connected_geometry_point_welder.cpp
// (sub_43BC90 "welding point positions" / sub_43C2E0 "welding point skinning") groups points
// into spatial neighbourhoods, snaps each group to its centroid, then splits into
// subneighbourhoods by BIT-EXACT memcmp of:
//     position (12 B) + node indices (16 B) + node weights (16 B)
// Only points identical in all three weld together.
//
// Vista's exported vertices are already the tool's post-weld, post-centroid-snap output, so
// exact position bits are the right spatial test. What we were missing is the skinning half:
// the key used only the DOMINANT node, so two vertices sharing a position and dominant bone
// but differing in their secondary weights welded here when the tool would have kept them
// apart — merging distinct points and fusing edges that should stay separate.
//
// skin_a / skin_b carry the declaration-4 payload verbatim (4 node indices, 4 weights) so the
// comparison is bit-exact like the tool's. For formats with no weight payload they degrade to
// the single node index, which is the old behaviour and correct for those.
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
// Generation verification (user-requested): every invariant the draw path depends on.
//  1. index ranges: triangle verts < welded count; quad verts/tris in range or sentinel
//  2. positions finite and inside a sane bound
//  3. planes consistent with their triangle (d == dot(n, p0), non-degenerate normal)
//  4. edge membership + WINDING: an interior quad's (vert_a -> vert_b) must appear in
//     tri_left's winding order and reversed in tri_right's — the emission logic's core
//     assumption; a violation flips the sheet's INCR/DECR (streak artifacts)
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
			if (v0 == edge_a && v1 == edge_b) return 1;
			if (v0 == edge_b && v1 == edge_a) return -1;
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
		LOG_INFO_GAME("stencil VALIDATE FAILED: tri_idx={} quad_idx={} pos={} planes={} edges={} winding={}",
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

				LOG_INFO_GAME("  offender quad {}: edge ({},{}) L={} ({},{},{}) order={} R={} ({},{},{}) order={} flagged={} self={}",
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
		LOG_INFO_GAME("stencil validate: ok ({} verts, {} planes, {} quads)",
			shadow->welded_vertex_count, shadow->plane_count, shadow->quad_count);
	}
}
bool stencil_shadow_build_from_geometry(
	const s_stencil_shadow_build_input* input,
	s_stencil_shadow_section* out_shadow)
{
	memset(out_shadow, 0, sizeof(*out_shadow));
	g_stencil_shadow_build_fail = "unknown";	// it. 472 — overwritten by every failure path below

	if (!input || !input->section_info || !input->geometry)
	{
		g_stencil_shadow_build_fail = "no-resident-data";
		return false;
	}

	// it. 643: the builder reads ONLY these. A render model section and a BSP cluster both supply
	// them, which is the whole reason the cluster tier reuses this function rather than forking it.
	const geometry_section_info* info = input->section_info;
	const int32 classification = input->global_geometry_classification;
	const geometry_point_data* point_data = input->point_data;
	geometry_section* geometry = input->geometry;
	const uint16* strip_indices = geometry->strip_indices[0];
	if (!strip_indices)
	{
		g_stencil_shadow_build_fail = "no-strip-indices";
		return false;
	}

	// DIAGNOSTIC ONLY (no behaviour change) — is the AUTHORED point data present in Vista caches?
	//
	// td never welds at runtime: section_skin_from_rigid_point_groups (td 0x19EAF0) skins a
	// pre-welded POINT array and isq_object_do_skinning_work (td 0x19F390) scatters the results
	// back to vertices through vertex_to_point_map (td section_data+320). Both the map and the
	// rigid point groups are authored at tag build time.
	//
	// Vista DECLARES all of it -- render_model_section_data::point_data carries raw_points,
	// runtime_point_data, rigid_point_groups (4 bytes, td's exact layout) and vertex_point_indices
	// (== td's vertex_to_point_map). But declaration is not presence: invalid_section_pair_bits
	// survived the cache build while isq/dsq did not, so this has to be measured.
	//
	// If vertex_point_indices.count == the vertex count, the authored weld is available and our
	// s_position_key heuristic can be replaced by an EXACT weld (ours can merge vertices the tool
	// kept separate, or split ones it merged, and either changes the silhouette). If the counts
	// are 0 it is stripped like isq/dsq and the heuristic is a necessary invention.
	{
		// point_data is NULL for a BSP cluster — the format has no such block there at all, so
		// "absent" is structural rather than measured and there is nothing to report.
		if (point_data && g_stencil_shadow_logged_point_data < 8)
		{
			g_stencil_shadow_logged_point_data++;
			LOG_INFO_GAME("stencil pointdata: raw_points={} runtime_point_data={} rigid_groups={} vertex_point_indices={} class={} verts={}",
				point_data->raw_points.count,
				point_data->runtime_point_data.size,
				point_data->rigid_point_groups.count,
				point_data->vertex_point_indices.count,
				(int32)info->geometry_classification,
				info->total_vertex_count);
		}
	}

	// P1-3 groundwork — locate the per-vertex node indices/weights for skinned sections.
	// geometry_section_get_compressed_vertex (halo2.exe 0x675DD9) only decodes vertex_buffers[0]
	// (the POSITION stream): format 1 = float3, format 2 = float3 + 1 node byte, format 3 =
	// 3x int16 + 1 node byte. All three carry at most ONE node, which is why our skinning is
	// dominant-node. But rasterizer_dx9_set_vertex_shader_permutation takes a
	// max_nodes_per_vertex, so multi-bone weights must live in another buffer of this section.
	// Rather than guess the layout, dump the section's buffer table once per classification and
	// write the decode against real data (td parity target: section_skin_from_rigid_point_groups,
	// td 0x19EAF0, sum(w_i * (P . M_i)) over up to 4 bones).
	{
		// four samples per classification, not one: a single sample cannot distinguish
		// "class 1 always has rigid_node 0" from "the first one happened to"
		if (classification >= 0 && classification < 8
			&& g_stencil_shadow_logged_classification[classification] < 4)
		{
			g_stencil_shadow_logged_classification[classification]++;
			// rigid_node is logged to settle whether the classification-1 case is live: td applies
			// section->rigid_node with NO classification check (rasterizer_model_section_draw,
			// td 0x10F0E0, loads v12-v14 from it), and the shader table gives class 1 (rigid) the
			// vertex-attr-matrix variants that READ v12-v14 while class 2 (rigid_boned) uses the
			// indexed palette and ignores them. Our draw applies rigid_node only for class 2 --
			// the inverse. If class-1 sections report a non-zero rigid_node here, those sections
			// are being transformed by node 0 instead of their own node (see
			// td-rigid-node-inversion.md).
			LOG_INFO_GAME("stencil vbuf: classification={} rigid_node={} buffers={} verts={}",
				classification, (int32)input->rigid_node, geometry->vertex_buffers.count,
				info->total_vertex_count);
			for (int32 buffer_index = 0; buffer_index < geometry->vertex_buffers.count; buffer_index++)
			{
				const rasterizer_vertex_buffer* buffer = geometry->vertex_buffers[buffer_index];

				UNREFERENCED_PARAMETER(buffer);

				LOG_INFO_GAME("stencil vbuf:   [{}] declaration={} stride={} count={} data={}",
					buffer_index, (int32)buffer->declaration, (int32)buffer->stride,
					(int32)buffer->count, buffer->vertex_data ? 1 : 0);
			}
			// P1-3: declaration 4 confirmed from live data as
			//   float x,y,z; uint8 node_index[4]; uint8 node_weight[4]
			// (weights ubyte4 summing to 255; indices local to node_map). Sanity-log the
			// weight sum so a future format change is caught rather than silently mis-skinned.
			const uint8* skinned_first = stencil_shadow_get_skinned_vertex(geometry, 0);
			if (skinned_first)
			{
				int32 min_sum = 4096, max_sum = -1, max_index = -1;
				for (int32 v = 0; v < info->total_vertex_count; v++)
				{
					const uint8* p = stencil_shadow_get_skinned_vertex(geometry, v);
					int32 sum = p[k_skinned_weight_offset] + p[k_skinned_weight_offset + 1]
						+ p[k_skinned_weight_offset + 2] + p[k_skinned_weight_offset + 3];
					if (sum < min_sum) min_sum = sum;
					if (sum > max_sum) max_sum = sum;
					for (int32 k = 0; k < 4; k++)
					{
						if (p[k_skinned_index_offset + k] > max_index)
						{
							max_index = p[k_skinned_index_offset + k];
						}
					}
				}
				LOG_INFO_GAME("stencil skin: weight_sum={}..{} (expect ~255) max_local_node={} node_map_count={}",
					min_sum, max_sum, max_index, input->node_map_count);
			}
		}
	}

	// 1. Weld section vertices by exact position + node (tool welds by proximity + centroid
	// snap + identical skinning; exported model data is already centroid-snapped, so exact
	// match suffices). The per-vertex detail byte (formats 2/3) is the LOCAL node index,
	// remapped through the section node_map; format 1 (plain float3) reports 0.
	// Weld only the OPAQUE vertices. td's scatter loop is bounded by
	// section_info + 12 == opaque_vertex_count (isq_object_do_skinning_work, td 0x19F390), not by
	// the total. Shadow-casting part types are 1 and 2 -- both `opaque_*` -- so no shadow triangle
	// can reference a transparent vertex; welding them only inflates the shadow VB, which is
	// sized 2 * welded_vertex_count. The tool also sorts opaque vertices ahead of transparent
	// ones, so the opaque ones keep exactly the welded indices a full walk would have given them.
	//
	// Guarded: a cache leaving opaque_vertex_count at 0 (or above the total) falls back to the
	// total rather than welding nothing.
	uint16 weld_vertex_count = info->opaque_vertex_count;
	if (weld_vertex_count == 0 || weld_vertex_count > info->total_vertex_count)
	{
		weld_vertex_count = info->total_vertex_count;
	}

	std::unordered_map<s_position_key, uint16, s_position_key_hasher> weld_lookup;
	// sized by the TOTAL so any vertex index a triangle names is in range; entries past
	// weld_vertex_count stay k_unwelded and are rejected at triangle emission
	static const uint16 k_unwelded = 0xFFFF;
	std::vector<uint16> weld_map(info->total_vertex_count, k_unwelded);
	std::vector<real_point3d> welded_positions;
	std::vector<uint8> welded_nodes;
	welded_positions.reserve(info->total_vertex_count);
	welded_nodes.reserve(info->total_vertex_count);

	const uint8* node_map = input->node_map;
	int32 node_map_count = input->node_map_count;
	bool mixed_nodes = false;

	// P1-3: full 4-bone skinning payload, captured per welded vertex when the position stream
	// is declaration 4. td does exactly this blend in section_skin_from_rigid_point_groups
	// (td 0x19EAF0): P' = sum(w_i * (P . M_i)).
	std::vector<uint8> welded_bone_indices;
	std::vector<real32> welded_bone_weights;
	bool has_bone_weights = stencil_shadow_get_skinned_vertex(geometry, 0) != NULL;
	if (has_bone_weights)
	{
		welded_bone_indices.reserve(info->total_vertex_count * 4);
		welded_bone_weights.reserve(info->total_vertex_count * 4);
	}

	// AUTHORED WELD (preferred). vertex_point_indices IS td's vertex_to_point_map -- the same
	// field, same element type, present in both formats and differing only in tag_block width
	// (td section_data+316/320; Vista render_model_section_data::point_data). When the cache
	// ships it, the tool's own vertex->point grouping is available and we use it verbatim
	// instead of reconstructing one.
	//
	// Why this is strictly better than the heuristic: tool.exe's welder (welder.cpp) merges on
	// a POSITION tolerance and a NODE WEIGHT tolerance over multiple passes, then writes the
	// exported vertex data FROM the welded points. Our exact-match key recovers that grouping
	// only because the tolerances were already baked in upstream -- it cannot merge two points
	// the tool kept apart, but it also cannot know about a merge the tool made on a tolerance
	// our exact compare would reject. Reading the authored indices removes the inference.
	//
	// Self-gating: absent or wrong-sized data leaves use_authored_weld false and the existing
	// heuristic runs unchanged.
	const uint16* authored_point_indices = NULL;
	if (point_data && point_data->vertex_point_indices.count == info->total_vertex_count
		&& info->total_vertex_count > 0)
	{
		authored_point_indices = point_data->vertex_point_indices[0];
	}
	const bool use_authored_weld = authored_point_indices != NULL;
	std::unordered_map<uint16, uint16> authored_lookup;
	{
		if (!g_stencil_shadow_logged_authored_weld)
		{
			g_stencil_shadow_logged_authored_weld = true;
			LOG_INFO_GAME("stencil weld: authored vertex_point_indices {} (count={} verts={})",
				use_authored_weld ? "IN USE" : "absent — using exact-match heuristic",
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

		// the per-vertex detail byte is a NODE index only on rigid_boned/skinned sections;
		// on plain rigid sections it can carry unrelated data — trusting it there
		// misclassified rigid sections as articulated and transformed their verts by
		// garbage bone matrices (vertices flung to wrong positions: streak fins)
		uint8 model_node = 0;
		if (classification >= _geometry_classification_rigid_boned)
		{
			model_node = (uint8)local_node;
			if (node_map && local_node >= 0 && local_node < node_map_count)
			{
				model_node = node_map[local_node];
			}
			else if (!g_stencil_shadow_warned_no_node_map)
			{
				// A guarded correction needs an `else` that says so (it. 346): the default
				// assigned above is the UNMAPPED local index, so falling through here binds
				// every affected vertex to a DIFFERENT bone -- which is exactly symptom 2's
				// "animation is mistranslated" signature, and it would be silent.
				//
				// Not hypothetical arithmetic: the map is genuinely non-identity. Read live
				// (it. 416) off the biped's skinned section 13, node_map_size 9:
				//     local  0 1 2 3 4  5  6  7  8
				//     model  0 1 2 3 6 10 14 15 16
				// so a vertex naming local 4 must resolve to model node 6. Using 4 would
				// transform it by an unrelated bone.
				g_stencil_shadow_warned_no_node_map = true;
				LOG_INFO_GAME("stencil WARNING: class {} section has no usable node_map (count={}) for local node {} — falling back to the LOCAL index, vertices will bind to the wrong nodes",
					(int32)classification, node_map_count, local_node);
			}
		}

		// tool.exe compares the full skinning payload, not just the dominant bone (I5)
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
					if (node_map && bone_local >= 0 && bone_local < node_map_count)
					{
						bone_node = node_map[bone_local];
					}
					// it. 494: this fallback used to be SILENT, while the single-node path 100 lines
					// above warns for the identical condition (it. 416). Both leave the UNMAPPED local
					// index in place, which binds the vertex to a different bone — symptom 2's
					// "animation is mistranslated" signature exactly — and the weighted path is the
					// one that runs on real bipeds. A silent version of a defect the other branch
					// shouts about is the it. 471-478 pattern; it is now observable.
					//
					// The two causes are distinguished because they mean different things:
					//   no-map       -> the section carries no node_map at all; EVERY bone of every
					//                   weighted vertex here is mis-bound
					//   idx>=count   -> the map exists but this payload index is past its end; only
					//                   that one bone is mis-bound. it. 407 measured local indices
					//                   running right up to the bound (max 18 vs count 19), so an
					//                   off-by-one in the authored data would land exactly here
					else if (!g_stencil_shadow_warned_bone_no_map)
					{
						g_stencil_shadow_warned_bone_no_map = true;
						LOG_INFO_GAME("stencil WARNING: 4-bone payload could not map local node {} ({}) — falling back to the LOCAL index, this vertex binds to the WRONG bone (class {}, node_map_count {})",
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

	// DEGENERATE-WELD GUARD. Observed live (h2mod.log 17/08, session 00:00): a class-3 section
	// reported `verts=1 planes=592 extent=(0.000 x 0.000 x 0.000) quads=888` — 592 triangles
	// collapsed onto ONE welded vertex, every plane degenerate, 888 silhouette quads of nothing,
	// all of it then drawn.
	//
	// The cause is upstream: geometry_section_get_compressed_vertex (halo2.exe 0x675DD9) decodes
	// declarations 1, 2 and 3, and for ANY OTHER declaration falls through to
	// `return global_origin3d` — (0,0,0) for every vertex. They then share one weld key and
	// collapse. Sibling sections of the same model decode fine, so it is per section: that
	// section's position buffer uses a format we do not handle.
	//
	// Building from it is strictly worse than skipping it: the planes carry zero normals (the
	// validator's own `n_len_sq < 1e-12` test), so the facing bitvector is meaningless and the
	// volume is a degenerate fan at the origin. Reject, and report the declaration so the
	// unhandled format can be identified.
	if (welded_positions.size() <= 1 && info->shadow_casting_triangle_count > 0)
	{
		if (!g_stencil_shadow_warned_degenerate_weld)
		{
			g_stencil_shadow_warned_degenerate_weld = true;
			int32 decl = -1, stride = -1;
			if (geometry->vertex_buffers.count > 0 && geometry->vertex_buffers[0])
			{
				decl = (int32)geometry->vertex_buffers[0]->declaration;
				stride = (int32)geometry->vertex_buffers[0]->stride;
			}
			LOG_INFO_GAME("stencil WARNING: section welded to {} vertices from {} verts (class={} declaration={} stride={}) — position decode failed, section skipped",
				(uint32)welded_positions.size(), info->total_vertex_count,
				(int32)classification, decl, stride);
		}
		g_stencil_shadow_build_fail = "position-decode";
		return false;
	}

	// 2. Enumerate shadow-casting triangles part by part, walking each triangle strip with
	// degenerate skip and parity winding (matches tag-debug strip walk + tool plane order).
	std::vector<uint16> triangles;			// welded, 3 per triangle
	std::vector<real_plane3d> planes;
	std::vector<s_stencil_shadow_rigid_group> groups;
	// owning part per triangle — I6's pass-0 pairing requires partners to share a part
	// (our analogue of td's "same material" constraint)
	std::vector<uint16> triangle_parts;

	// D9 — td selects casting parts BY COUNT, not by type. The cap loop in
	// rasterizer_stencilshadow_shadows_model_section_draw walks parts
	// [0, section_info.shadow_casting_part_count) and never inspects part->type: the tool sorts
	// shadow-casting parts to the front of the block and records how many there are.
	//
	// Testing part->type instead is unsafe because geometry_postprocess (td 0x214DD0) remaps
	// part types {0,1,2,3} -> {2,1,3,4} exactly ONCE, guarded by a flag written back into the
	// tag (_geometry_part_flag_new_part_types). A part still in the OLD numbering reads as
	// 0 = not_drawn (we'd skip a caster) or 2 = opaque_nonshadowing (we'd sweep in a
	// non-caster) -- both silent, and the second widens the silhouette at every extrusion.
	int32 casting_part_count = info->shadow_casting_part_count;
	if (casting_part_count > geometry->parts.count)
	{
		casting_part_count = geometry->parts.count;
	}
	// SAFETY on an untested assumption: this whole scheme rests on Vista's cache populating
	// shadow_casting_part_count AND on the tool having sorted casting parts to the front. If
	// the count reads 0 while type-based selection can still find casters, taking td's route
	// verbatim would silently build nothing at all -- no triangles, no shadow, no error.
	// Fall back to the type test in that case and say so, rather than fail to a blank screen.
	// it. 650: clusters take td's CLUSTER rule — every part, filtered by type. See the input field.
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
				LOG_INFO_GAME("stencil shadows: shadow_casting_part_count is 0 but {} parts pass the type test — falling back to type selection (D9 assumption does not hold on this cache)",
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
			continue;		// fallback path only — td selects purely by count
		}

		uint32 part_triangle_start = (uint32)planes.size();
		// it. 444: the FIRST-ORDER guard. `strip_start_index` and `strip_length` are both tag
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
				LOG_INFO_GAME("stencil WARNING: part strip range {}..{} exceeds the {}-entry strip block — corrupt tag data, part skipped",
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

			// it. 444: `index_N` is an arbitrary uint16 straight out of tag strip data, while
			// `weld_map` is only `total_vertex_count` entries (84 on the biped's section 0). A
			// corrupt strip therefore reads `weld_map[60000]` on an 84-element vector. The
			// `k_unwelded` test below catches a bad RESULT, but only after the read has happened.
			// Inert for valid data (the tool guarantees strip indices < total_vertex_count);
			// guarded on the it. 347 precedent for user-modified maps. Build-time only — this
			// loop runs once per section per map, so the three compares cost nothing.
			if ((size_t)index_0 >= weld_map.size()
				|| (size_t)index_1 >= weld_map.size()
				|| (size_t)index_2 >= weld_map.size())
			{
				if (!g_stencil_shadow_warned_bad_strip)
				{
					g_stencil_shadow_warned_bad_strip = true;
					LOG_INFO_GAME("stencil WARNING: strip index out of range ({}/{}/{} vs {} vertices) — corrupt strip data, triangle dropped",
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
					LOG_INFO_GAME("stencil WARNING: shadow triangle references a non-opaque vertex (opaque={} total={}) — triangle dropped",
						weld_vertex_count, info->total_vertex_count);
				}
				continue;
			}

			// DEGENERATE **AFTER** WELDING (it. 646).
			//
			// The strip-stitch test above compares RAW indices, which is the only thing that exists
			// at that point. But `weld_map` can send two DISTINCT raw indices to the SAME welded
			// vertex, and then a triangle that was fine in the strip has zero area here.
			//
			// It is not a cosmetic waste. `plane.n` below is a cross product of two parallel edges,
			// so the normal is ZERO — and the facing test dots the light against it and takes the
			// sign of 0. That bit is noise, it flips frame to frame, and every edge the triangle
			// touches becomes a silhouette that appears and disappears. The symptom is a shattered
			// triangular fragment pattern over the caster, which is what the first cluster run
			// showed (`VALIDATE FAILED: ... planes=106`, i.e. 106 zero-length normals).
			//
			// Never bit the model tier because models barely weld — a model's coincident vertices
			// are split by node or skinning payload, so they stay distinct. BSP clusters weld purely
			// by position (no nodes, no skinning) and world geometry is full of coincident vertices:
			// the first run welded 957 raw vertices down to 446.
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
					LOG_INFO_GAME("stencil WARNING: section exceeded {} planes — remaining shadow triangles dropped",
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
		// it. 472: split by whether the section DECLARES any shadow-casting triangles. A section
		// with shadow_casting_triangle_count == 0 producing no planes is entirely benign and is
		// expected to be common — it must not be counted against the it. 422 gate. Zero planes
		// despite a non-zero declared count is a real strip-walk failure and a genuine defect.
		g_stencil_shadow_build_fail = info->shadow_casting_triangle_count == 0
			? "no-shadow-tris-declared"
			: "no-planes-DESPITE-declared-tris";
		return false;
	}

	// 3. Edge adjacency -> silhouette quads.
	//
	// I6 — this now mirrors tool.exe's manifold pairer (sub_454500, render_geometry.cpp:3156)
	// rather than the single-slot rule it replaced. td collects ALL triangle references for an
	// edge and then pairs them, requiring OPPOSITE WINDING (`(r1.edge_ref < 0) != (r2.edge_ref
	// < 0)`) and the same section, over three passes of decreasing strictness:
	//   pass 0: partner must be a different triangle AND share the material
	//   pass 1: material requirement dropped
	//   pass 2: a triangle may pair with ITSELF (fold-back / degenerate edges)
	// Leftovers are then discarded.
	//
	// Our material analogue is the PART (Vista encodes shadow capability in the part type, and
	// we already restrict to the casting parts), so pass 0 = same part, pass 1 = any part.
	// The old code paired only an exactly-reversed SECOND reference and sentinel-closed
	// everything else, so an edge fan with 4 references (2 forward, 2 reversed) produced 1 pair
	// plus 2 sentinels where td produces 2 clean pairs. Sentinels emit geometry a real pair
	// does not, so that was a direct source of surplus silhouette sheets.
	//
	// DEVIATION RETAINED: td *deletes* whatever stays unpaired; we still sentinel-close it,
	// because section seams legitimately arrive here as unpaired boundary edges. Discarding them
	// would open the volume at every seam.
	//
	// Why td can delete and we cannot: td pairs seams at TAG time, through four blocks the tool
	// authors -- forward shared edges (uint16 indices), forward shared edge groups, backward
	// shared edges, backward shared edge groups (td-isq-generation.md, it. 237/241). Once those
	// are paired, anything td still finds unpaired is a genuine hole in the mesh, so deleting it
	// is correct. We have no such tag data, so our unpaired set is "real holes + every seam", and
	// deleting it would open the volume at every section boundary.
	//
	// STALE-COMMENT FIX (it. 243): this used to read "our cross-section shared-edge stitching
	// (I4) is not implemented". It IS implemented -- it is *disabled*, which is different. See
	// stencil_shadow_model_cross_get and td-seam-stitching-precondition.md.
	//
	// COUPLED WITH THE RE-ENABLE: if stitching is switched back on, the two mechanisms must not
	// both close the same seam. The scheme is that a matched boundary edge is retagged
	// k_stencil_shadow_matched_boundary (0xFFFE) so the per-section walk SKIPS it and the
	// cross-quad bridges it instead -- the disable comment's "sentinels stay untagged".
	//
	// STATE, VERIFIED it. 243-244: the retag is **not** performed anywhere. The only writes to
	// tri_right in this file are the paired case (refs[j].triangle) and the 0xFFFF sentinel just
	// below; 0xFFFE is never written. So k_stencil_shadow_matched_boundary is currently DEAD --
	// the consumer survives (the draw loop's skip, and the validator's boundary test) but the
	// producer was removed with the rest of the stitching build.
	//
	// Consequence: re-enabling stitching is NOT a flag flip. Restoring cross-quad construction
	// without also restoring the retag gives every bridged seam a bridge quad AND two sentinel
	// closures -- double-counted stencil along every seam, which would look like the wedge
	// artifact the disable was meant to cure and invite the wrong diagnosis. The matching
	// primitive `stencil_shadow_seam_key` is still present; `stencil_shadow_bind_position` was
	// DELETED in it. 609 (no callers — found by the it. 608 mechanical pass, never in it. 364's
	// inventory), so a revival would have to reinstate it.
	struct s_edge_ref
	{
		uint16 triangle;
		uint16 part;
		uint16 vert_a;		// as traversed by THIS triangle (encodes the winding)
		uint16 vert_b;
		bool consumed;
	};
	std::unordered_map<uint32, std::vector<s_edge_ref>> edges;
	std::vector<s_stencil_shadow_quad> quads;
	std::vector<uint32> quad_same_winding;	// bit per quad (reserved; no pairs set it now)

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

	uint32 boundary_edge_count = 0;
	uint32 same_winding_candidates = 0;		// diagnostic, see the loop below
	// it. 648: the DENOMINATOR that makes `boundary` readable. The log used to divide unpaired refs
	// by `edges.size()`, which counts distinct edge KEYS — two different things, and the resulting
	// ratio can exceed the real unpaired fraction by a lot when keys carry several triangles. That
	// misread ratio is what set it. 646's 50% cluster gate and made every cluster look broken.
	uint32 total_edge_refs = 0;
	for (auto& count_entry : edges)
	{
		total_edge_refs += (uint32)count_entry.second.size();
	}
	for (auto& entry : edges)
	{
		std::vector<s_edge_ref>& refs = entry.second;

		// Three relaxation passes: pass 0 requires both references to share a part, pass 1 relaxes
		// that, pass 2 additionally permits self-pairing.
		//
		// PROVENANCE (it. 277 claimed this was unverifiable; CORRECTED it. 278 -- the tool's
		// algorithm IS recoverable and has been read).
		//
		// tool.exe `sub_42D1C0` == connected_geometry_edge_builder.cpp (found via its
		// progress_new("building edges") string; see td-isq-generation.md). It does NOT use
		// relaxation passes. It builds a single-pass EDGE REGISTRY: for each triangle edge, search
		// the first endpoint vertex's incident-edge list for an edge whose other endpoint matches,
		// then REUSE it (appending this triangle) or CREATE it (registering on both endpoints).
		//
		// Ours is a different construction reaching a comparable result, and that is fine -- this
		// generator is judged on output equivalence. But note one real divergence, deliberately
		// recorded rather than silently kept:
		//
		//   THE TOOL DOES NOT CONSTRAIN PAIRING BY MATERIAL. It pairs across materials and records
		//   the mismatch: edge->field_0 takes the first triangle's material and is set to -1 when a
		//   later triangle disagrees. We instead REQUIRE a shared part in pass 0 and only relax it
		//   in pass 1. The same pairs still form -- pass 1 relaxes -- but the ORDER differs, and
		//   order decides which triangle lands in tri_left vs tri_right.
		//
		//   THAT DIFFERENCE IS PROVABLY HARMLESS (it. 284). Emission is symmetric under swapping
		//   the two roles, because vert_a/vert_b are taken from the RIGHT reference's traversal and
		//   the draw path swaps them when the right triangle faces:
		//
		//     roles (i=A, j=B):  verts = B's traversal;  right_faces = f(B)
		//     roles (i=B, j=A):  verts = A's traversal   = REVERSE of B's;  right_faces = f(A)
		//
		//   Take f(A)=1, f(B)=0. First assignment: right_faces=0, no swap, emits B's traversal.
		//   Second: right_faces=1, swaps, emits the reverse of A's traversal -- which is B's
		//   traversal. Identical quad. The silhouette test f(left) XOR f(right) is symmetric too,
		//   so the emit/skip decision is unchanged. Whichever order the passes pair in, the emitted
		//   geometry is the same.
		//
		// Also confirmed there: `reversed` is the SIGN BIT of an edge designator (index is
		// value & 0x7FFFFFFF), the same convention as the BSP portal code, and an edge may carry
		// MORE than two triangles -- non-manifold edges are represented, not rejected.
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
					// REVERSE of the FACING triangle's traversal — store the second
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

		// DIAGNOSTIC ONLY (it. 229) — no behaviour change. Counts references left unpaired because
		// another reference traverses the same edge the SAME way round. The pairing loop above
		// matches only OPPOSITE-winding references, so on an inconsistently wound mesh these fall
		// through to the boundary-edge path below and are emitted with the 0xFFFF sentinel, as
		// though the mesh were open there. td's edge record instead carries a `reversed` bit,
		// implying the tool pairs them and suppresses the emission swap.
		//
		// This measures how much geometry the difference actually touches before anything is
		// changed: edge pairing drives silhouette topology for every model, so it must not be
		// altered on inference. Read alongside the validator's `winding=` count.
		// See td-same-winding-pairs.md.
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

	// ⚠ THIS IS OURS, NOT td's — the claim below that it is "the original's equivalent" was WRONG
	// (corrected it. 422 by decompiling the function it names).
	//
	// `render_model_check_shadow_manifold` (td 0x1869F0) is a pure TAG-DATA gate and contains no
	// geometry test whatsoever. It walks every REGION PAIR, takes each region's ACTIVE permutation
	// from the object's own per-region permutation indices (`object + 392`), reads the section index
	// at each of the SIX LOD slots, forms the triangular pair index `min + max*(max-1)/2`, and tests
	// that bit in the authored `invalid_section_pair_bits`. It rejects the WHOLE MODEL, never an
	// individual section, and it never looks at an edge.
	//
	// We already port that faithfully at the object level as `stencil_shadow_model_is_manifold`
	// (P1-2, the `dbg_nonmanifold` counter). The test below is an ADDITIONAL, INVENTED rejection
	// with no counterpart in td, and the 10% threshold is arbitrary.
	//
	// Consequence worth knowing before trusting a missing shadow: a section dropped here casts
	// nothing at all, so on a biped this removes a whole body part's shadow rather than degrading
	// it. It is at least reported (the log below), not silent. Whether to keep it is a ledger item
	// — do NOT remove it blind, but do not treat it as td parity either.
	//
	// original (inaccurate) comment follows:
	// manifold quality gate (the original's equivalent: render_model_check_shadow_manifold /
	// "bad shadow volumes, object will not shadow correctly"): heavily open meshes produce
	// sliver volumes and miscounts — refuse to shadow them
	//
	// it. 643/646 — THE THRESHOLD IS PER SOURCE. Clusters run far more open than models (a BSP
	// partition is cut at its portals), so they get 50% rather than 10% — but they DO get a gate.
	// it. 643's full bypass was wrong: the first run built clusters at 70% boundary edges, which is a
	// broken weld rather than an open partition, and with no gate the tier drew garbage silently
	// instead of saying so. See the field's note.
	if (input->boundary_reject_percent > 0
		&& boundary_edge_count * 100 > (uint32)edges.size() * (uint32)input->boundary_reject_percent)
	{
		LOG_INFO_GAME("stencil shadows: section rejected (non-manifold: {}/{} boundary edges, limit {}%)",
			boundary_edge_count, edges.size(), input->boundary_reject_percent);
		g_stencil_shadow_build_fail = "it422-boundary-gate";	// THE tally target — count only these
		return false;
	}

	// 4. Shadow vertex buffer: doubled welded verts {pos, 0} / {pos, 1}
	IDirect3DDevice9Ex* device = rasterizer_dx9_device_get_interface();
	if (!device)
	{
		g_stencil_shadow_build_fail = "no-d3d-device";
		return false;
	}

	// articulated sections (mixed per-vertex nodes, or fully skinned approximated by their
	// dominant node) get a DYNAMIC VB refreshed per draw; static sections stay write-once
	bool articulated = mixed_nodes
		|| classification == _geometry_classification_skinned;

	uint32 vb_vertex_count = (uint32)welded_positions.size() * 2;
	IDirect3DVertexBuffer9* shadow_vb = NULL;
	if (FAILED(device->CreateVertexBuffer(
		vb_vertex_count * sizeof(s_stencil_shadow_vertex),
		articulated ? (D3DUSAGE_WRITEONLY | D3DUSAGE_DYNAMIC) : D3DUSAGE_WRITEONLY,
		0,
		D3DPOOL_DEFAULT,
		&shadow_vb,
		NULL)))
	{
		g_stencil_shadow_build_fail = "vb-create-failed";
		return false;
	}

	s_stencil_shadow_vertex* vb_data = NULL;
	if (FAILED(shadow_vb->Lock(0, 0, (void**)&vb_data, articulated ? D3DLOCK_DISCARD : 0)))
	{
		shadow_vb->Release();
		g_stencil_shadow_build_fail = "vb-lock-failed";
		return false;
	}
	for (uint32 welded_index = 0; welded_index < welded_positions.size(); welded_index++)
	{
		vb_data[welded_index * 2].position = welded_positions[welded_index];
		vb_data[welded_index * 2].extrude = 0.f;
		vb_data[welded_index * 2 + 1].position = welded_positions[welded_index];
		vb_data[welded_index * 2 + 1].extrude = 1.f;
	}
	shadow_vb->Unlock();

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
		// it. 658: cache the section's node_map for the skinning-pool palette path — the pool
		// stores per-region palettes in LOCAL node_map order on Vista content (force_node_maps,
		// measured it. 657), and the tag's own copy may not be resident at draw time.
		if (input->node_map && input->node_map_count > 0)
		{
			out_shadow->pool_node_map = new uint8[input->node_map_count];
			memcpy(out_shadow->pool_node_map, input->node_map, input->node_map_count);
			out_shadow->pool_node_map_count = input->node_map_count;
		}

		// it. 660 — the GPU-skinning STATIC VB, for articulated sections. Written once at bind
		// pose with the palette indices/weights baked in; the palette at c50 poses it per draw,
		// which is what retires the per-frame Lock/rewrite. Eligibility:
		//   * a node_map exists and fits the c50 palette window (count <= 67: c50 + 3*66 + 2 = 250,
		//     comfortably clear of c254/c255);
		//   * EVERY node a vertex binds resolves to a map slot. Stream-sourced indices are in the
		//     map by construction; the "no usable node_map" fallback path stores unmapped values,
		//     and any miss below abandons the skinned VB rather than mis-binding one vertex.
		// Failure of any kind leaves skinned_vb NULL and the section on the CPU path exactly as
		// before this iteration — the dynamic VB above is still built and still refreshed.
		if (articulated && out_shadow->pool_node_map
			&& out_shadow->pool_node_map_count > 0 && out_shadow->pool_node_map_count <= 67)
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
					// full 4-bone payload. Weights are quantized to ubyte4n summing EXACTLY 255 —
					// the residue goes to the heaviest lane, so the blend never scales the vertex.
					int32 weight_bytes[4];
					int32 weight_total = 0;
					int32 heaviest = 0;
					for (int32 bone = 0; bone < 4; bone++)
					{
						const real32 weight = welded_bone_weights[i * 4 + bone];
						weight_bytes[bone] = (int32)(weight * 255.f + 0.5f);
						if (weight_bytes[bone] < 0) { weight_bytes[bone] = 0; }
						weight_total += weight_bytes[bone];
						if (weight_bytes[bone] > weight_bytes[heaviest]) { heaviest = bone; }
					}
					weight_bytes[heaviest] += 255 - weight_total;
					for (int32 bone = 0; bone < 4; bone++)
					{
						if (weight_bytes[bone] <= 0)
						{
							continue;	// lane stays {0,0} — weight 0 reads palette[0] harmlessly
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

	// it. 510 PROBE — DIAGNOSTIC ONLY. Does the SHADOW actually reference any render-only node?
	//
	// it. 509 measured 14 of 33 nodes render-only on a biped, which sounds material — but that counts
	// the MODEL, not the shadow. A render-only node with no shadow-casting geometry bound to it costs
	// nothing, because the composition we skip (0x53599B) would move a node no shadow vertex uses.
	// This intersects the two sets, which is the number that actually decides whether it. 487's risky
	// plumbing is worth building.
	//
	// Counts DISTINCT nodes, and covers both binding paths: `welded_nodes` (the single-node / dominant
	// path) and `welded_bone_indices` (the 4-bone weighted path, the one that runs on real bipeds).
	// Both hold MODEL node indices — already remapped through `node_map` (it. 494) — so they are
	// directly comparable to the flag bit positions.
	//
	//   used_render_only=0  -> the shadow never binds a render-only node. it. 487 CLOSES for this
	//                          content: skipping the composition cannot move a shadow vertex, and the
	//                          divergence reduces to eye tracking on nodes the shadow may not use either
	//   used_render_only>0  -> that many nodes carry shadow geometry the engine poses and we do not.
	//                          Build the plumbing
	//
	// One-shot per map; latch reset in stencil_shadow_generation_cache_clear (it. 623).
	if (!g_stencil_shadow_probed_render_only_used && g_stencil_shadow_render_only_flags)
	{
		g_stencil_shadow_probed_render_only_used = true;
		uint32 used_bits[8] = {};
		for (uint32 i = 0; i < (uint32)welded_nodes.size(); i++)
		{
			used_bits[welded_nodes[i] >> 5] |= (1u << (welded_nodes[i] & 31));
		}
		for (uint32 i = 0; i < (uint32)welded_bone_indices.size(); i++)
		{
			used_bits[welded_bone_indices[i] >> 5] |= (1u << (welded_bone_indices[i] & 31));
		}
		int32 used_nodes = 0, used_render_only = 0;
		for (int32 node_index = 0; node_index < 256; node_index++)
		{
			if (((used_bits[node_index >> 5] >> (node_index & 31)) & 1) == 0)
			{
				continue;
			}
			used_nodes++;
			if (node_index < g_stencil_shadow_render_only_node_count
				&& ((g_stencil_shadow_render_only_flags[node_index >> 3] >> (node_index & 7)) & 1) != 0)
			{
				used_render_only++;
			}
		}
		LOG_INFO_GAME("stencil renderonly used: section nodes={} of which render_only={} (class={}) (it. 510 — 0 means the SHADOW never binds a render-only node, so it. 487's skipped composition cannot move it and the item CLOSES for this content)",
			used_nodes, used_render_only,
			(int32)classification);
	}

	out_shadow->valid = true;
	// SIZE DIAGNOSTIC: the section's own extent in the space its vertices live in. The drawn
	// shadow can never be smaller than this, so if the extent already dwarfs the object the
	// problem is the GEOMETRY, not the extrusion.
	//
	// it. 491 STRUCK THE SUPPORTING ARGUMENT THAT USED TO BE HERE, and it. 492 CORRECTED ITS REASON.
	//
	// The struck argument read: "...which is the only reading consistent with a 10000x extrusion
	// change producing no visible difference."
	//
	// it. 491 claimed the far-plane clamp in `shadow_extrude.fx` explains that. **It does not.** The
	// clamp touches only `output.oPos.z`; x, y and w are untouched, so a 10000x extrusion still
	// projects to a vastly larger screen-space silhouette. And at the shipping 2.0 wu distance the
	// clamp is **inert** anyway — z_far is 1024 wu (it. 406), so an extruded vertex a couple of
	// units behind a caster is nowhere near z/w = 0.9999 and `min()` keeps it unchanged. The clamp is
	// insurance against far-clip cutting the side sheets open at long distances, nothing more.
	//
	// The argument is still struck, for the correct reason: the observation is **unexplained**, and an
	// unexplained observation is not evidence for a particular thesis. If it was accurate it points at
	// extrusion not being applied — which is NOT supported: the vertex declaration is verified (POSITION
	// float3 @0, TEXCOORD0 float1 @12, offsetof-derived, static_asserted, checked against the compiled
	// disassembly in it. 355-357), and the shader does read `extrude_c : register(c255)` and apply
	// `extrude_c.x`. So either the observation was imprecise, or something not yet found is at work.
	// Either way it cannot be cited FOR the geometry thesis.
	//
	// The GEOMETRY diagnosis stands on its own, much stronger evidence: it. 388 (undecompressed
	// normalized positions), it. 411 (decompression reproduces the tool's own authored `raw_points`
	// on all three axes), it. 415 (measured caster bounds 0.3185/0.3735/0.7048 wu vs a normalised
	// span of 2.0). This diagnostic is still worth having — the extent it prints is a direct
	// measurement — it simply no longer leans on an observation that cannot support it.
	{
		real_point3d lo = welded_positions[0], hi = welded_positions[0];
		for (uint32 i = 1; i < (uint32)welded_positions.size(); i++)
		{
			const real_point3d* p = &welded_positions[i];
			if (p->x < lo.x) lo.x = p->x;
			if (p->y < lo.y) lo.y = p->y;
			if (p->z < lo.z) lo.z = p->z;
			if (p->x > hi.x) hi.x = p->x;
			if (p->y > hi.y) hi.y = p->y;
			if (p->z > hi.z) hi.z = p->z;
		}
		// it. 629: the per-section extent computed here fed `s_stencil_shadow_section::extent_max`,
		// whose only consumer was the PER-SECTION dynamic-extrusion experiment. it. 604 deleted that
		// mode; the field and this write outlived it silently, because neither is referenced by name
		// anywhere else so no compiler warning could reach them. Both are gone now. The `lo`/`hi`
		// bound above still feeds the size diagnostic below, which is why it survives.
		// D9 cross-check: td's invariant is that the shadow geometry is exactly the first
		// shadow_casting_part_count parts, totalling shadow_casting_triangle_count triangles.
		// planes != expected means we are still sweeping in the wrong set.
		int32 type_selected_parts = 0;
		// td distinguishes two casting types (geometry_part_type_is_shadow_casting, td 0x212BC0):
		// type 1 = opaque_shadow_ONLY -- a proxy mesh that casts but never renders -- and
		// type 2 = opaque_shadow_casting, the visible mesh. Which one we get is DATA, and it
		// decides what our silhouette is built from: a purpose-built (and reliably closed)
		// proxy, or the render mesh itself. If this reads 0 across every model, Vista's cache
		// carries no proxies and every shadow comes from render geometry -- which would also
		// explain any nonmanifold rejections, since proxies are authored closed and render
		// meshes frequently are not.
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
		// `edges=` added it. 432 so the it. 422 gate's MARGIN is visible, not just its verdict.
		// That gate rejects a section when `boundary * 10 > edges`, i.e. above 10% boundary edges,
		// and it is ours rather than td's (see the banner at the gate). Until now the build line
		// reported `boundary=` alone, so a section that passed at 9.9% looked exactly like one that
		// passed at 0.5% — the rejection line is the only place the denominator appeared, and that
		// prints only when the section is already lost.
		//
		// The 17/08 run recorded boundary edges 48 / 52 / 10 / 24 / 78 across five built sections
		// with no denominator, so the margin could only be estimated. With `edges=` the ratio is
		// read directly: comfortably below 10% means the gate is inert and the ledger item can be
		// closed; close to 10% means an ordinary authoring change would start deleting body-part
		// shadows, and the arbitrary threshold needs revisiting.
		// `weld=` added it. 474. Which weld ran is a PER-SECTION property — it. 410 measured the
		// authored `vertex_point_indices` present on some sections of a model and absent on others —
		// but the only report of it was `stencil weld:`, latched ONE-SHOT per map. That single line
		// describes whichever section happened to build first and cannot be attributed to any other.
		//
		// It matters here specifically: it. 450's prediction for the biped's section 0
		// (`verts=32 planes=48 boundary=0/52`) was derived assuming the AUTHORED map. If that
		// section actually runs the exact-match heuristic, a different `verts`/`boundary` is an
		// expected consequence of a different weld, NOT evidence that the pairing is broken — which
		// is the conclusion the index tells the reader to draw. Without this field the two are
		// indistinguishable.
		LOG_INFO_GAME("stencil build: verts={} planes={} (expect {}) parts={}/{} (by_type {}, shadow_only {}) class={} extent=({:.3f} x {:.3f} x {:.3f}) quads={} boundary={}/{} refs (unpaired fraction) samewind={} weld={} decomp={}",
			out_shadow->welded_vertex_count, out_shadow->plane_count,
			info->shadow_casting_triangle_count,
			casting_part_count, geometry->parts.count, type_selected_parts, shadow_only_parts,
			(int32)classification,
			hi.x - lo.x, hi.y - lo.y, hi.z - lo.z, out_shadow->quad_count,
			boundary_edge_count, total_edge_refs, same_winding_candidates,
			use_authored_weld ? "authored" : "heuristic",
			// `decomp=` added it. 475 — SYMPTOM 1'S PRIMARY CHECK, made per-section and countable.
			// The it. 417 warning already announces the failure case, but it is one-shot AND does not
			// name the section, so the run could only report "this happened at least once". The
			// decision needs the COUNT: normalized-with-no-bounds on 1 section of 200 is a curiosity,
			// on 200 of 200 it means it. 388 did not take effect at all and the ~6x oversizing stands.
			//   applied        -> flags&1 set and bounds resolved; it. 388 is doing its work here
			//   not-normalized -> flags&1 clear; positions were already world-space, nothing to do
			//   MISSING-BOUNDS -> flags&1 set but neither the section-level nor the model-level
			//                     compression_info had an element. THIS section stays ~6x oversized.
			((uint16)info->geometry_compression_flags & 1) == 0
				? "not-normalized"
				: (input->position_bounds ? "applied" : "MISSING-BOUNDS"));
	}
	stencil_shadow_section_validate(out_shadow);
	return true;
}

// The render-model adapter. Behaviour-identical to the pre-it.643 builder: every field below is the
// one the body used to read directly, so the model tier is unchanged by the generalisation.
bool stencil_shadow_section_build(
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
	input.select_parts_by_type = false;		// models use the COUNT rule (D9) — unchanged
	return stencil_shadow_build_from_geometry(&input, out_shadow);
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
/* shadow data cache (P1: keyed by render model datum + section index, freed on map unload) */

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
	// That is correct for PERMANENT failures (bad tag data, unbuildable geometry) — it stops us
	// retrying hopeless work every frame — but it is WRONG for a transient one. See the preload
	// below.
	render_model_definition* definition = (render_model_definition*)tag_get_fast(render_model_index);
	if (!definition || section_index < 0 || section_index >= definition->sections.count)
	{
		// permanent: tag data. Negative cache is correct.
		if (!g_stencil_shadow_warned_no_definition)
		{
			g_stencil_shadow_warned_no_definition = true;
			LOG_INFO_GAME("stencil WARNING: section_get failed — definition={} section_index={} sections={}",
				definition ? 1 : 0, section_index, definition ? definition->sections.count : -1);
		}
		return NULL;
	}
	render_model_section* section = definition->sections[section_index];

	// TRANSIENT FAILURE — must NOT be negatively cached (fixed it. 329).
	//
	// `pc_geometry_cache_preload_geometry` (halo2.exe 0x6652BC) is a streaming call and returns
	// false for reasons that resolve on their own:
	//   * `if (g_geometry_cache_flag_2) blocking_load = 0;` — the engine can STRIP the blocking flag
	//     we pass. With blocking off it skips `async_task_wait_for_completion`, so a block whose
	//     async read is still in flight reports not-loaded and returns 0.
	//   * `geometry_cache_allocate_and_read` may fail to allocate (`geometry_cache_index == -1`,
	//     which then sets `g_geometry_cache_flag_1`) when the LRU cache has no evictable page.
	//
	// Leaving the negative entry in place meant a section whose geometry merely was not resident YET
	// never cast a shadow again for the rest of the map. Load-order and memory-pressure dependent, so
	// it presents as "some objects sometimes never have shadows" with nothing in the log — the engine
	// itself retries (`structure_cluster_get_geometry_section` calls the same function fresh every
	// time and just returns NULL on failure), and so must we.
	//
	// Erase the slot so the next frame rebuilds from scratch. `slot` DANGLES after this — do not
	// touch it below the erase.
	if (!pc_geometry_cache_preload_geometry(
		&section->geometry_block_info,
		(e_pc_geometry_cache_preload_flags)(_pc_geometry_cache_preload_blocking | _pc_geometry_cache_preload_flag_2)))
	{
		g_stencil_shadow_cache.erase(key);
		static uint32 preload_retry_log = 0;
		if ((preload_retry_log++ % 300) == 0)
		{
			LOG_INFO_GAME("stencil geometry not resident: model={} section={} — retrying next frame (count {})",
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
			LOG_INFO_GAME("stencil WARNING: section {} has no section_data — cannot build a volume", section_index);
		}
		return NULL;
	}

	// compressed sections dequantize against their OWN section-level bounds first — the
	// model-level block is only a fallback. Using the model bounds for every section
	// displaced vertices on sections with differing bounds (sliver/streak volumes).
	// DIVERGENCE FROM THE ENGINE, made detectable rather than changed (it. 391).
	//
	// Both engine paths that consume position bounds use the **model-level** block only:
	//   * `render_visible_section_set_vertex_compression` (halo2.exe 0x6809C4) gates on `v4[5]`
	//     (definition->compression_info.count, +20) and passes `v4[6]` (+24) to
	//     `rasterizer_dx9_set_vertex_compression_constants` -- it never reads the SECTION's block;
	//   * `lightmap_raycast_resolve_object_hit` (0x4B2CD4) likewise takes `render_model[6]` for the
	//     bounds it hands to `geometry_section_get_compressed_vertex`.
	//
	// We prefer the section-level block when present. Since it. 388 our decompression must reproduce the
	// GPU's reconstruction EXACTLY, so if any section carries its own bounds that differ from the model's,
	// we would decompress to different positions than the renderer draws -- a wrongly-sized volume for
	// that section only.
	//
	// Not changed here, deliberately: the comment this replaced recorded an observed artifact
	// ("using the model bounds for every section displaced vertices ... sliver/streak volumes"), and
	// bounds selection affects every compressed section. On the content measured in it. 382 the
	// section-level count is **0**, so we take the model-level branch and match the engine anyway.
	// The log below fires only in the case where the two could disagree, which turns an unknown into an
	// observation. If it never fires, the preference is harmless and can simply be simplified to the
	// engine's rule; if it does fire, compare that section's bounds against the model's before deciding.
	const real32* position_bounds = NULL;
	if (section->section_info.compression_info.count > 0)
	{
		position_bounds = (const real32*)&section->section_info.compression_info[0]->position_bounds;
		if (!g_stencil_shadow_warned_section_bounds)
		{
			g_stencil_shadow_warned_section_bounds = true;
			LOG_INFO_GAME("stencil bounds: section {} has its OWN compression_info ({} entries) — we use it, the ENGINE uses the model-level block; compare them if volumes are mis-sized (it. 391)",
				section_index, section->section_info.compression_info.count);
		}
	}
	else if (definition->compression_info.count > 0)
	{
		position_bounds = (const real32*)&definition->compression_info[0]->position_bounds;
	}

	// CRASH GUARD (it. 347). `geometry_section_get_compressed_vertex` (halo2.exe 0x675DD9) case 3
	// dereferences its `bounds` argument UNCONDITIONALLY -- `bounds[1] - *bounds`, no null check -- so
	// handing it a NULL for a declaration-3 (3 x int16, stride 8) section is a null-deref, not a
	// degenerate decode. `position_bounds` above is legitimately NULL when neither the section-level nor
	// the model-level `compression_info` block has an element, which is malformed-but-possible tag data
	// (Cartographer runs user-modified maps). The engine's own caller has the same latent deref, which is
	// why shipped maps never exercise it -- that is not a guarantee we can rely on.
	//
	// Rejecting is correct and the negative cache entry is right: this is a property of the tag data, not
	// of timing (see the residency note in td-do-not-fix.md).
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
				LOG_INFO_GAME("stencil WARNING: section {} is declaration 3 (compressed) with NO compression_info — skipped to avoid a null bounds deref in the engine decoder",
					section_index);
			}
			return NULL;
		}
	}

	if (!stencil_shadow_section_build(section, section->section_data[0], position_bounds, &slot))
	{
		// it. 453: name the section that was lost.
		//
		// Every failure message inside the build says WHY but not WHICH — the function does not
		// receive a section index — so a missing body-part shadow could not be attributed from the
		// log. That matters now rather than hypothetically: it. 452 computed the biped's section 2
		// from live tag bytes and found 6 boundary edges of 33 (18.2%), which trips the it. 422
		// boundary gate, so a real body part casts nothing on the primary caster.
		//
		// Deliberately NOT latched — the point is a complete inventory of lost sections, not one
		// sample. It cannot spam for permanent failures: a failed build leaves an invalid cache
		// entry which is not rebuilt. The transient exception is the residency case, which erases
		// its slot on purpose so the next frame retries (it. 329); that one can repeat during load,
		// and a steady stream of it means the geometry cache is thrashing rather than that the
		// section is bad.
		// it. 472: the reason is now IN THIS LINE. It used to say "the preceding line gives the
		// reason", which was false for six of the eight failure paths — they were silent, so the
		// preceding line belonged to some other section entirely. That mattered because the it. 422
		// plan was to COUNT these lines as the gate's cost; instead, count only reason=it422-boundary-gate.
		// Expect reason=no-shadow-tris-declared to be common and entirely benign.
		LOG_INFO_GAME("stencil shadows: section {} BUILD FAILED — casts no shadow (reason={})",
			section_index, g_stencil_shadow_build_fail);
		return NULL;
	}
	return &slot;
}

/* cross-section stitching (td shared-edge stitches: seams bridged between sections) */

// s_stencil_shadow_model_cross is declared in this module's header — the render hook holds one.

static std::unordered_map<uint32, s_stencil_shadow_model_cross> g_stencil_shadow_cross_cache;


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

	// it. 542 — RUNTIME SEAM PAIRING RESTORED. This is the substitute for a TOOL-time step, not a port
	// of a td runtime one: tool.exe pairs edges at tag build time (`sub_42D1C0`, the "building edges"
	// pass, recovered it. 278) and writes four shared-edge tag blocks; td's runtime only LOOKS THEM UP.
	// Vista strips those blocks — `render_model_section_data` is 112 B against td's 524, and the
	// difference is the ISQ/DSQ payload (it. 518) — so there is nothing to read and the pairing has to be
	// derived here, exactly as `CLAUDE.md` says all ISQ/DSQ post-processing must be.
	//
	// WHY IT WAS DISABLED, AND WHY THAT NO LONGER HOLDS. The removal note gave two reasons:
	//   1. "the one-sided bridge left the PARTNER section's volume open" — attributed to unpaired wedge
	//      sheets fanning from bipeds. it. 504 showed that signature was the MISSING INVERSE BIND
	//      (it. 317), which is fixed and measured absent while stitching stayed off.
	//   2. "ours cannot until full-weight skinning lands" — full-weight skinning landed (it. 468); the
	//      weighted blend runs on every declaration-4 section, which is what keeps seam vertices
	//      coincident and is precisely the property td relies on.
	// Both are void (it. 504), and the fragmentation the user is seeing is what unbridged seams look
	// like: mode 1 shows a coherent volume while mode 2 shows broken counting (it. 541).
	//
	// METHOD. A boundary quad is one the per-section walk could not pair (`tri_right == 0xFFFF`). Its
	// endpoints are in BIND-POSE MODEL space (it. 333-335), which is the space seams coincide in — that
	// is why this can be built once per model and cached, and why it works for skinned sections whose
	// world positions differ every frame. Hash each boundary edge by its two endpoint positions
	// (`stencil_shadow_seam_key`, an unordered pair) and pair entries that come from DIFFERENT sections.
	//
	// RETAG. Each matched edge is retagged `k_stencil_shadow_matched_boundary` (0xFFFE) on BOTH sides so
	// the per-section draw skips it and the bridge closes it instead. Without this the seam gets a bridge
	// quad AND two sentinel closures — double-counted stencil along every seam, which would look like the
	// artefact the disable was blamed for and invite the wrong diagnosis a second time (the hazard is
	// spelled out at the sentinel site).
	struct s_seam_ref
	{
		int32 section;
		uint32 quad_index;
		uint16 vert_a;
		uint16 vert_b;
		uint16 triangle;
	};
	std::unordered_map<uint64, std::vector<s_seam_ref>> seams;

	// it. 543: iterate DENSE slots; `section_of_dense` recovers the stable section index that goes into
	// the emitted quads, so the per-model cache stays valid when a different LOD changes which sections
	// draw (the hazard it. 505 flagged in the "producer emits dense slots" alternative).
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
				// it. 543: emit the STABLE section indices, recovered from the dense slots
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

	LOG_INFO_GAME("stencil stitch: model {} paired {} cross-section seams from {} distinct boundary keys (it. 542 — runtime substitute for tool.exe's build-time edge pass; Vista strips td's four shared-edge blocks)",
		(uint32)render_model_index, paired, (uint32)seams.size());
	return &cross;
}

/* BSP clusters (td's environment tier) */

// Separate from the model cache on purpose rather than sharing a key space: the two are keyed by
// different things (render model datum vs bsp+cluster index) and a shared map would need a
// discriminator bit whose only job is to prevent a collision that separate maps cannot have.
static std::unordered_map<uint32, s_stencil_shadow_section> g_stencil_shadow_cluster_cache;

static bool g_stencil_shadow_warned_no_cluster_geometry = false;
static bool g_stencil_shadow_warned_cluster_bounds = false;
static bool g_stencil_shadow_warned_cluster_cap = false;
static int32 g_stencil_shadow_logged_clusters = 0;

// MAXIMUM_CLUSTERS_PER_STRUCTURE is 512 and MAXIMUM_STRUCTURE_BSPS_PER_SCENARIO is 16, so 16 bits of
// cluster and 8 of bsp index is comfortably wide. The whole cache is cleared on map unload, so a bsp
// index cannot alias across scenarios.
static uint32 stencil_shadow_cluster_key(int16 bsp_index, int32 cluster_index)
{
	return (((uint32)(uint16)bsp_index) << 16) | ((uint32)cluster_index & 0xFFFF);
}

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
	if (!bsp || cluster_index < 0 || cluster_index >= bsp->clusters.count)
	{
		return NULL;
	}

	const uint32 key = stencil_shadow_cluster_key(bsp_index, cluster_index);
	auto found = g_stencil_shadow_cluster_cache.find(key);
	if (found != g_stencil_shadow_cluster_cache.end())
	{
		return found->second.valid ? &found->second : NULL;
	}

	// MEMORY BOUND, and the reason it exists here but not on the model cache.
	//
	// A section's shadow VB is 32 bytes per welded vertex (each doubled, 16 bytes each). A model
	// section is hundreds of vertices; a BSP cluster is tens of thousands, so ONE cluster can cost
	// more than every model on the map put together. Cartographer is a 32-bit process, and a large
	// map walked end to end with dynamic lights on would otherwise accumulate cluster VBs until an
	// allocation fails — which presents as a crash, not as a missing shadow.
	//
	// A hard count cap rather than an LRU eviction: eviction needs use-tracking and a policy, and
	// both are ways to get this wrong silently. Refusing to build past the cap loses environment
	// shadows in the rooms visited last, says so once, and cannot corrupt anything.
	if ((int32)g_stencil_shadow_cluster_cache.size() >= k_stencil_shadow_environment_max_cached_clusters)
	{
		if (!g_stencil_shadow_warned_cluster_cap)
		{
			g_stencil_shadow_warned_cluster_cap = true;
			LOG_INFO_GAME("stencil cluster: cache holds {} clusters (the cap) — no further clusters will be built this map. Raise k_stencil_shadow_environment_max_cached_clusters if environment shadows are missing in later areas.",
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

	// Same transient-vs-permanent split as the model path (it. 329): a cluster whose geometry is not
	// resident YET must not be negatively cached, or it never casts again for the rest of the map.
	// The engine's own accessor (structure_cluster_get_geometry_section, halo2.exe 0x450B41) does
	// exactly this — preload, and just return NULL on failure, fresh every call.
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
			LOG_INFO_GAME("stencil cluster: cluster {} has no cluster_data — casts nothing", cluster_index);
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
				LOG_INFO_GAME("stencil cluster: cluster {} is declaration 3 (compressed) with NO compression_info — skipped to avoid a null bounds deref in the engine decoder",
					cluster_index);
			}
			return NULL;
		}
	}

	// MEASUREMENT, not decoration — the three unknowns docs/13 listed before this tier could start.
	// A cluster is far larger than a model section, so the caps and the declaration are the things
	// that can make this tier silently wrong, and they are cheap to state.
	if (g_stencil_shadow_logged_clusters < 8)
	{
		g_stencil_shadow_logged_clusters++;
		const rasterizer_vertex_buffer* position_buffer = geometry->vertex_buffers.count > 0 ? geometry->vertex_buffers[0] : NULL;

		UNREFERENCED_PARAMETER(position_buffer);

		LOG_INFO_GAME("stencil cluster: [{}] verts={} tris={} shadow_parts={}/{} shadow_tris={} class={} decl={} bounds={} (caps: {} planes / {} quads)",
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

	// it. 647: probe the first four clusters only — the answer is a property of the FORMAT, so a
	// handful of samples settles it and more would just be log noise.
	if (g_stencil_shadow_logged_clusters <= 4)
	{
		stencil_shadow_probe_normal_stream(geometry, cluster->geometry_section_info.total_vertex_count);
	}

	// Cluster geometry is WORLDSPACE — already in world space, no nodes, no skinning. That is
	// classification 0, the simplest path the builder has, and the one td's shader 137 exists for
	// (it needs no transform at all). Forced rather than read: a cluster's own
	// `geometry_classification` describes its VERTEX FORMAT, and the draw-time question is whether a
	// node matrix applies. For structure geometry it never does.
	s_stencil_shadow_build_input input = {};
	input.section_info = &cluster->geometry_section_info;
	input.geometry = geometry;
	input.point_data = NULL;			// absent from the format entirely — see the struct's note
	input.node_map = NULL;
	input.node_map_count = 0;
	input.global_geometry_classification = _geometry_classification_worldspace;
	input.rigid_node = NONE;
	input.position_bounds = position_bounds;
	// it. 648: 0 = OFF for clusters, reverting it. 646's 50%. That threshold was set from a ratio I
	// had misread — see the field's note — and it rejected every cluster on every map tested, so the
	// tier drew nothing at all. Off until the real unpaired fraction is understood.
	input.boundary_reject_percent = 0;
	input.select_parts_by_type = true;		// td's CLUSTER rule — see the field's note

	if (!stencil_shadow_build_from_geometry(&input, &slot))
	{
		LOG_INFO_GAME("stencil cluster: [{}] BUILD FAILED — casts no shadow (reason={})",
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

	// Refresh this module's diagnostic budgets so each map gets its own samples (it. 310). Without
	// this they are process-lifetime caps and only the first map loaded is ever described.
	//
	// The draw-side latches are reset by stencil_shadow_cache_clear, which calls this. Split by
	// OWNERSHIP: a latch belongs to whichever module emits it, so a new latch added here must be
	// reset here, and one added to the rasterizer must be reset there.
	g_stencil_shadow_logged_point_data = 0;
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
	g_stencil_shadow_probed_render_only_used = false;
	g_stencil_shadow_warned_bad_strip = false;
	g_stencil_shadow_render_only_flags = NULL;
	g_stencil_shadow_render_only_node_count = 0;
}
