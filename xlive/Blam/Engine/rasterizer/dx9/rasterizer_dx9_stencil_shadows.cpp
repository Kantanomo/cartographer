#include "stdafx.h"
#include "rasterizer_dx9_stencil_shadows.h"

#include "rasterizer_dx9.h"
#include "rasterizer_dx9_main.h"
#include "cache/cache_files.h"
#include "cache/pc_geometry_cache.h"
#include "objects/objects.h"
#include "objects/object_definition.h"
#include "memory/data.h"
#include "math/matrix_math.h"		// matrix4x3_multiply — skinning = node_world x inverse_bind
#include "main/interpolator.h"		// it. 471 interp probe — needs the interpolator's bool result
#include "rasterizer/rasterizer_globals.h"
#include "render/render.h"
#include "render/render_lights.h"
#include "render/render_lod_new.h"
#include "Util/Hooks/Hook.h"
#include "H2MOD/Modules/h2log/h2log.h"
#include "vertex_shaders_dx9/preprocessed_hlsl_from_tool/compiled/shadow_extrude.h"
#include "vertex_shaders_dx9/preprocessed_hlsl_from_tool/compiled/shadow_extrude_vs30.h"
#include "vertex_shaders_dx9/preprocessed_hlsl_from_tool/compiled/shadow_solid.h"
#include "vertex_shaders_dx9/preprocessed_hlsl_from_tool/compiled/shadow_stipple.h"

#include <xmmintrin.h>
#include <unordered_map>
#include <vector>

/* globals */

static IDirect3DVertexShader9* g_stencil_shadow_vertex_shader = NULL;
static IDirect3DPixelShader9* g_stencil_shadow_pixel_shader = NULL;
static IDirect3DVertexDeclaration9* g_stencil_shadow_vertex_declaration = NULL;
static IDirect3DIndexBuffer9* g_stencil_shadow_index_buffer = NULL;
// SM3 pair for td's stipple-density fade (screen-door clip in the ps; vs_3_0 required
// because SM3 shaders cannot pair with other model versions). NULL when unsupported.
static IDirect3DVertexShader9* g_stencil_shadow_vertex_shader_sm3 = NULL;
static IDirect3DPixelShader9* g_stencil_shadow_stipple_shader = NULL;

enum
{
	// silhouette quads expand to 6 indices each; sized for the worst case section
	k_stencil_shadow_index_buffer_capacity = k_stencil_shadow_maximum_quads_per_section * 6
};

// High constant registers (shader binds c250+) so the engine's own constants — e.g. the
// 2D text transform in c0-c3 — are never clobbered (state blocks proved unreliable at
// restoring constants on this device).
enum
{
	k_stencil_shadow_light_constant = 254,
	k_stencil_shadow_extrusion_distance_constant = 255,
	k_stencil_shadow_tint_constant = 31	// ps_2_0 highest constant register
};

// Dirty the engine's gpu frontend cache (g_gpu_frontend @ engine 0xE4B040) after raw device
// binds — the engine's own post-mutation resync primitive (c_gpu_frontend::reset_state,
// engine 0x66E7D7). Covers ONLY streams + SetIndices(NULL) + field_FC.
static void stencil_shadow_reset_engine_gpu_state(void)
{
	INVOKE_TYPE(0x26E7D7, 0x0, void(__fastcall*)(void*, void*),
		Memory::GetAddress<void*>(0xA4B040), NULL);
}

// Hand the pipeline back to the engine after our raw binds. reset_state does NOT cover the
// cached vertex shader / declaration pointers (verified: c_gpu_frontend layout), so the engine
// would compare against stale pointers and SKIP re-binding — its draws would then run with OUR
// shader (this was the HUD-text killer). NULL the device binds and write matching NULL + dirty
// into the frontend cache fields so the next engine resolve re-binds everything.
static void stencil_shadow_release_pipeline(IDirect3DDevice9Ex* device)
{
	device->SetVertexShader(NULL);
	device->SetPixelShader(NULL);
	device->SetVertexDeclaration(NULL);
	device->SetStreamSource(0, NULL, 0, 0);

	*Memory::GetAddress<IDirect3DVertexShader9**>(0xA4B044) = NULL;			// m_vertex_shader
	*Memory::GetAddress<bool*>(0xA4B048) = true;							// m_vertex_shader_assigned (dirty)
	*Memory::GetAddress<void**>(0xA4B04C) = NULL;							// m_vertex_declaration[0]
	*Memory::GetAddress<bool*>(0xA4B078) = true;							// m_vertex_declaration_dirty
	*Memory::GetAddress<IDirect3DVertexDeclaration9**>(0xA4B140) = NULL;	// m_d3d_vertex_declaration
	*Memory::GetAddress<int32*>(0xA4B144) = NONE;							// m_cached_declaration_key

	stencil_shadow_reset_engine_gpu_state();
}

// The engine's rasterizer_dx9_set_render_state is a redundancy cache: it skips the device
// call when its cached value matches. Mid-scene (the volumes-pass position) the device can
// diverge from that cache (state blocks / raw sets inside the layer machinery), silently
// dropping our states. Force the device AND record through the wrapper so the cache stays
// truthful for the engine's next draw.
static void stencil_shadow_force_render_state(D3DRENDERSTATETYPE state, DWORD value)
{
	rasterizer_dx9_set_render_state(state, value);
	IDirect3DDevice9Ex* force_device = rasterizer_dx9_device_get_interface();
	if (force_device)
	{
		force_device->SetRenderState(state, value);
	}
}

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

// REGISTER LAYOUT IS OURS, NOT td's (clarified it. 285). The c[15] / c[20].w above describe td's
// NV2A microcode. Our HLSL uses its own high registers -- light in c254 (xyz = position,
// w = 1 point / 0 directional, the SAME packing as td's c[15]) and extrusion in c255**.x**, not .w.
// The value is identical (2.0, confirmed live as `extrusion=2.000`); only the register and lane
// differ, and our shader declares them to match. Do not "fix" the upload to c[15]/c[20].w to match
// the comment above -- that would unbind the constants our own shader reads.
// F6 diagnostic: cycle extrusion distance to isolate rasterization failures at extreme
// magnitudes (0 = k_stencil_shadow_extrusion_distance).
static real32 g_stencil_shadow_extrusion_override = 0.f;

// A/B TOGGLE for the silhouette quad winding (it. 341). Default false == CURRENT behaviour, so this
// changes nothing until it is set. It exists because the question it settles is a one-bit experiment that
// is otherwise a rebuild: **set it to true from the debugger** and compare interior parity of the same
// shadow on the same scene.
//
// What it decides: td emits `(2b, 2a, 2a+1, 2b+1)` when the LEFT triangle faces
// (`rasterizer_stencilshadow_shadows_model_section_draw`, td 0x1A16B0); we emit the reverse cycle, so our
// side sheets are wound INWARD where td's are OUTWARD (derivation and worked example in td-caps-draw.md,
// it. 340/341). Our caps are NOT inverted, and our z-fail ops are inverted GLOBALLY -- so sheets and caps
// plausibly disagree in sign.
//
// Why it has not simply been changed: it is on the path every caster uses, two transform fixes
// (it. 317/335) are already awaiting validation, and landing a third unvalidated change would confound
// attribution. Setting this flag reproduces td's ordering exactly in both facing cases.
//
// Expected signature if the flag FIXES something: interior holes / patchy parity inside an otherwise
// correctly shaped and sized shadow disappear. Those survive the transform fixes, so they are
// distinguishable from symptom 1.
//
// LATENT INTERACTION with `quad_same_winding_bits`, harmless today: the swap is suppressed for
// same-winding source pairs (`!same_winding`), and that suppression was defined against the
// `right_faces` condition. `quad_same_winding_bits` is never written (see td-do-not-fix.md), so
// `same_winding` is always false and the two features cannot currently interact. **If same-winding
// pairing is ever implemented (td-same-winding-pairs.md), re-derive which side the suppression belongs
// on for the flipped case** -- it does not follow automatically that suppressing on `left_faces` is
// right just because the swap moved there.
static bool g_stencil_shadow_quad_winding_flip = false;
// td's global shadow darkness. render_layer_lightmap_diffuse_setup (td 0x10D740) builds the
// apply-pass constant colour as ARGB(255, V, V, V) with V = (1 - flt_53A674) * 255 and blends
// SRCBLEND=ZERO / DESTBLEND=SRC_COLOR, i.e. dst *= V/255. Ours is SRCALPHA/INVSRCALPHA against
// black, dst *= (1 - a), so a == flt_53A674 exactly.
// flt_53A674 read from the shipped image (td 0x53A674) = 0x3ECCCCCD = 0.4.
static const real32 k_stencil_shadow_darkness = 0.4f;			// == td flt_53A674


// Draw mode (F7 cycles): 0 = real shadows (MASKING architecture: volumes between the
// lightmap-indirect and SH-PRT layers; the PRT draw is stencil-masked — tag-debug passes
// 6/7), 1 = translucent red volume visualization, 2 = stencil plumbing diagnostic.
static int32 g_stencil_shadow_draw_mode = 0;
static bool g_stencil_shadow_active = false;	// F8 latch (set in the UI hook, read in the render hook)
static bool g_stencil_shadow_masking_pass = false;	// inside the volumes pass (mode 0)
static bool g_stencil_shadow_mask_pending = false;	// volumes counted; PRT layer should mask
static bool g_stencil_shadow_saved_disable_stencil = false;	// engine lock flag save (mask scope)

// Build-time diagnostic sample budgets. FILE SCOPE on purpose (it. 310): as function-local statics
// these capped per PROCESS, so whichever map loaded first consumed them and later maps were never
// described — the reason it. 226 found `vbuf:` data only in a previous session's log. They are now
// zeroed by stencil_shadow_cache_clear, which runs on map unload AND before device reset, so each
// map gets a fresh budget.
static int32 g_stencil_shadow_logged_point_data = 0;		// caps `stencil pointdata:` at 8 sections
static uint8 g_stencil_shadow_logged_classification[8] = {};	// caps `stencil vbuf:` at 4 per class
// Also per-map: this latch is what NAMES an unhandled vertex declaration in the degenerate-weld
// warning, and that is how declaration 6 was found (it. 226). Process-lifetime suppression would
// hide a *different* unhandled format on a second map. The other one-shot latches stay
// process-lifetime on purpose — they report a condition once and would otherwise spam.
static bool g_stencil_shadow_warned_degenerate_weld = false;
// Per-map one-shot latches, reset by stencil_shadow_cache_clear alongside the one above. These MUST
// NOT be function-static: iteration 310 established that a process-lifetime latch describes only the
// FIRST map loaded and then goes silent for every map after it. Four latches added in it. 317-329 were
// function-static and had exactly that defect; moved to file scope in it. 330.
static bool g_stencil_shadow_warned_no_apply = false;
static bool g_stencil_shadow_warned_no_bind = false;
static bool g_stencil_shadow_warned_no_definition = false;
static bool g_stencil_shadow_warned_no_section_data = false;
static bool g_stencil_shadow_warned_no_static_bind = false;
static bool g_stencil_shadow_warned_no_bounds = false;
static bool g_stencil_shadow_warned_shadows_off = false;
static bool g_stencil_shadow_warned_cross_draw_failed = false;
static bool g_stencil_shadow_warned_section_bounds = false;
// Per-map one-shot latches, swept out of function scope in it. 353. FOUR of these report a CAPACITY
// CAP biting (plane cap, index-buffer overflow x2, the 64-section stitch cap and the 64-caster cap);
// as function-statics they could only ever describe the FIRST map loaded, so a cap that bit harder on
// a later map was silent -- while td-INDEX.md lists "did a capacity cap ever bite?" as a question the
// next run is supposed to answer.
static bool g_stencil_shadow_logged_authored_weld = false;
static bool g_stencil_shadow_warned_no_shadow_parts = false;
static bool g_stencil_shadow_warned_unwelded = false;
static bool g_stencil_shadow_warned_plane_cap = false;
static bool g_stencil_shadow_warned_index_overflow_volume = false;
static bool g_stencil_shadow_warned_index_overflow_cross = false;
static bool g_stencil_shadow_warned_cross_cap = false;
static bool g_stencil_shadow_warned_caster_cap = false;
static bool g_stencil_shadow_warned_no_node_map = false;
// it. 494: the 4-bone payload's own node_map fallback — separate latch from the single-node one above
// so both can report; they cover different branches of the same defect.
static bool g_stencil_shadow_warned_bone_no_map = false;
static bool g_stencil_shadow_warned_normalized_no_bounds = false;
static bool g_stencil_shadow_probed_interpolation = false;
// it. 472: WHY the last section build failed. `stencil_shadow_section_build` has EIGHT distinct
// `return false` paths and six of them were silent, so the `BUILD FAILED` line could not be
// attributed — and the it. 422 plan was to COUNT those lines as the gate's cost. Set on every
// failure path, printed by the BUILD FAILED line, so the tally can be filtered by cause.
static const char* g_stencil_shadow_build_fail = "unset";
// it. 477: sections dropped for classification > skinned. td casts from class 4; we do not. Latched
// per class value so output is bounded, with a running count for magnitude.
static uint32 g_stencil_shadow_skipped_class_mask = 0;
static uint32 g_stencil_shadow_skipped_class_count = 0;
// it. 480: BOUNDED latches ("log the first N"), moved here from function scope. They are latches, not
// throttles — they stop firing forever — so under the per-map rule they must be file-scope and reset
// in stencil_shadow_cache_clear. The documented `static bool` sweep could not see them because they
// are int32; see the widened check in td-do-not-fix.md.
static int32 g_stencil_shadow_logged_lod_models = 0;
static int32 g_stencil_shadow_logged_manifold_models = 0;
// it. 471: the interp probe samples ACROSS FRAMES rather than once — see the probe for why a
// single sample cannot distinguish "no lag" from "sampled at the wrong instant".
static uint32 g_stencil_shadow_interp_samples = 0;
static uint32 g_stencil_shadow_interp_ran = 0;
static real32 g_stencil_shadow_interp_max_pos = 0.f;
static real32 g_stencil_shadow_interp_max_fwd = 0.f;
static bool g_stencil_shadow_warned_bad_strip = false;

// Runtime replacement for the tool-time ISQ/DSQ generation (see stencil-shadows-port-design.md).
// Generation rules confirmed against tool.exe connected_geometry:
//  - weld: group by position (exact bit match in P1), final weld requires identical skinning
//  - edges: keyed by unordered welded-index pair; second triangle registers reversed winding
//  - silhouette quad per interior edge {vA, vB, triLeft, triRight}; boundary edges get a
//    sentinel partner that never faces the light

/* private code */

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

static bool stencil_shadow_shaders_initialize(IDirect3DDevice9Ex* device)
{
	// The guard must cover EVERY resource the callers then dereference, not just the first two
	// created (it. 302). It previously tested only the vertex shader and declaration, which are
	// created BEFORE the index buffer: if CreateIndexBuffer failed once — transient OOM, or a lost
	// device — this function returned false that time, but every later call early-returned **true**
	// with g_stencil_shadow_index_buffer still NULL. The draw paths then do
	// `g_stencil_shadow_index_buffer->Lock(...)` unconditionally, so a single transient failure
	// became a permanent null dereference.
	//
	// The SM3 pair is deliberately NOT part of this test: it is optional by design (failures leave
	// the pointers NULL and faded casters simply draw full-strength), and every use site checks
	// them. Only the mandatory trio belongs here.
	if (g_stencil_shadow_vertex_shader
		&& g_stencil_shadow_vertex_declaration
		&& g_stencil_shadow_index_buffer)
	{
		return true;
	}

	// Offsets come from `offsetof`, NOT literals (it. 358). `ASSERT_STRUCT_SIZE(s_stencil_shadow_vertex,
	// 16)` pins only the STRIDE, so swapping the two members would keep the assert happy and still
	// compile while making the shader read `extrude` as `position.x` -- wildly wrong volumes with no
	// build error and nothing in the log. Deriving the offsets makes that divergence impossible instead
	// of merely documented. These expand to the same 0 and 12 the literals held, so this is provably
	// behaviour-preserving.
	//
	// The types must still agree with the shader's declarations by hand: the extrusion blob declares
	// `dcl_position v0` (read as `v0.xyz`) and `dcl_texcoord v1` (read as `v1.x`) -- verified against the
	// compiled disassembly in it. 355-357.
	const D3DVERTEXELEMENT9 elements[] =
	{
		{ 0, (WORD)offsetof(s_stencil_shadow_vertex, position),
			D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
		{ 0, (WORD)offsetof(s_stencil_shadow_vertex, extrude),
			D3DDECLTYPE_FLOAT1, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
		D3DDECL_END()
	};
	// belt and braces: if the members are ever reordered, fail at COMPILE time rather than shipping a
	// silently-wrong declaration.
	static_assert(offsetof(s_stencil_shadow_vertex, position) == 0,
		"shadow vertex POSITION must be first; the extrusion shader reads it as v0.xyz");
	static_assert(offsetof(s_stencil_shadow_vertex, extrude) == 12,
		"shadow vertex extrude flag must follow position; the shader reads it as v1.x");
	if (FAILED(device->CreateVertexDeclaration(elements, &g_stencil_shadow_vertex_declaration)))
	{
		return false;
	}

	// vs_2_0 on purpose: the non-debug stencil-only path runs without a pixel shader,
	// and D3D9 forbids a SM3 vertex shader with fixed-function pixel processing.
	if (FAILED(device->CreateVertexShader((const DWORD*)k_shadow_extrude_vs_2_0, &g_stencil_shadow_vertex_shader)))
	{
		return false;
	}
	if (FAILED(device->CreatePixelShader((const DWORD*)k_shadow_solid_ps_2_0, &g_stencil_shadow_pixel_shader)))
	{
		return false;
	}

	if (FAILED(device->CreateIndexBuffer(
		k_stencil_shadow_index_buffer_capacity * sizeof(uint16),
		D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY,
		D3DFMT_INDEX16,
		D3DPOOL_DEFAULT,
		&g_stencil_shadow_index_buffer,
		NULL)))
	{
		return false;
	}

	// optional SM3 pair for td's stipple fade — failures leave the pointers NULL and
	// faded objects simply cast full-strength shadows
	if (rasterizer_globals_get()->d3d9_sm3_supported)
	{
		if (FAILED(device->CreateVertexShader((const DWORD*)k_shadow_extrude_vs_3_0,
			&g_stencil_shadow_vertex_shader_sm3)))
		{
			g_stencil_shadow_vertex_shader_sm3 = NULL;
		}
		if (FAILED(device->CreatePixelShader((const DWORD*)k_shadow_stipple_ps_3_0,
			&g_stencil_shadow_stipple_shader)))
		{
			g_stencil_shadow_stipple_shader = NULL;
		}
	}
	return true;
}

// c[n] = row n of the model->clip composite so the shader can do oPos.n = dot(float4(pos,1), c[n]).
// model_matrix: model->world (NULL = identity). Projection convention CONFIRMED in-game:
// engine projection_matrix is row-vector; its transpose is the column-vector form.
// The engine's world->clip, straight from its CPU vertex-constant mirror: c0..c3 hold the
// dp4 rows its own shaders draw scene geometry with. Derived-convention composites from
// s_render matrices produced negative-w clips (probe 2026-08-15) — the mirror is authoritative
// and self-updating per view (splitscreen, FOV, reflections).
// CURRENTLY UNREACHABLE, transitively (labelled it. 364). Its only caller is
// `stencil_shadow_compute_screen_rect`, which is itself unreferenced since the per-object scissored
// darken was removed for P1-1 td parity. A plain reference count does NOT reveal this -- this function
// has two references (its definition and that one call), so it looks live; only a transitive
// reachability pass finds it. `/wd4505` also suppresses the compiler warning that would otherwise flag
// the caller.
//
// Kept because it is the working, in-game-verified world->clip composite and a scissored debug pass
// would need it. Note what it does NOT do: it writes into a caller buffer and never uploads anything,
// so the extrusion shader's claim that `c0-c3` are inherited from the engine and never set by us stays
// true (verified: nothing in this file uploads vertex constant register 0). See entry 13 in
// td-do-not-fix.md for why the `out_constants` argument must stay non-NULL if this is ever revived.
static void stencil_shadow_get_world_to_clip(real32 out[4][4])
{
	// engine composer (halo2.exe 0x661a0d): out row j = [f.col_j, l.col_j, u.col_j,
	// pos.col_j + proj[3][j]] from global_window_parameters.projection, x projection_scale.
	// The out-buffer path computes fresh and commits nothing to the device.
	typedef void(__cdecl* t_set_wvp_constants)(bool apply_scale, bool refresh_viewport,
		void* projection, real32* out_constants);
	Memory::GetAddress<t_set_wvp_constants>(0x261a0d)(true, false, NULL, &out[0][0]);
}

// Per-object node transform through the ENGINE's own upload (halo2.exe 0x662bd4): packs the
// engine row convention (row r = forward[r], left[r], up[r], position[r]) into c50-c52 and
// keeps the engine transform cache coherent — the same path
// render_visible_section_set_transform_constants (0x680a68) uses for rigid sections. The
// volume shader then transforms exactly like an engine model shader: node rows -> world,
// inherited c0-c3 -> clip (td-vista-render-design-map.md section 2).
static void stencil_shadow_set_node_constants(const real_matrix4x3* model_matrix)
{
	real_vector4d rows[3];
	if (model_matrix)
	{
		for (int32 row = 0; row < 3; row++)
		{
			// SCALE **IS** APPLIED, to the basis only — and this is verified against the engine
			// path whose INPUT matches ours (see the warning below; it. 314 briefly removed it and
			// that was wrong).
			//
			// `model_skinning_matrix_from_real_matrix4x3` (td 0xA6720, model_skinning.cpp:417) packs
			// a RAW real_matrix4x3 -- the same kind object_get_node_matrix hands us -- into a pool
			// entry exactly like this:
			//     row[r].basis = { forward[r], left[r], up[r] } * scale;   // basis SCALED
			//     row[r].w     = position[r];                             // position UNSCALED
			// which is term-for-term what the four lines below do.
			//
			// WARNING -- DO NOT "correct" this against render_visible_section_set_transform_constants
			// (halo2.exe 0x680A68). That function reads its matrix at +4..+48 with no scale multiply,
			// which looks like proof that scale is ignored. It is not: its input is
			// `section->matrix`, a matrix stored in the visible-section record that is **already
			// pre-scaled**. Ours is a raw node matrix with a live scale field. Two engine paths,
			// two different input conventions, same end result -- comparing against the wrong one
			// removes a needed multiply and shrinks every volume whose node carries scale != 1.
			// (it. 316 caught this after it. 315 read the skinning packer.)
			rows[row].i = model_matrix->vectors.forward.n[row] * model_matrix->scale;
			rows[row].j = model_matrix->vectors.left.n[row] * model_matrix->scale;
			rows[row].k = model_matrix->vectors.up.n[row] * model_matrix->scale;
			rows[row].l = model_matrix->position.n[row];
		}
	}
	else
	{
		memset(rows, 0, sizeof(rows));
		rows[0].i = 1.f;
		rows[1].j = 1.f;
		rows[2].k = 1.f;
	}
	typedef HRESULT(__cdecl* t_set_transform_constants)(const real_vector4d*);
	Memory::GetAddress<t_set_transform_constants>(0x262bd4)(rows);
}

// Rotate a world-space DIRECTION into model space: d_model = R^T * d_world
// (columns of R are the matrix4x3 vectors; uniform scale does not affect facing signs).
//
// DIRECTIONS ONLY — this is not a general world->model transform (it. 291). There is no
// translation and no scale division, which is correct for a direction and WRONG for a position.
//
// That matters because `stencil_shadow_build_facing_bitvector` accepts a `point_light` flag, and
// the whole point-light path is currently **unused**: every caster-loop call passes `false`, since
// the lightmap tier's fake light is directional (td-fake-light.md). If point lights are ever
// enabled, this function must NOT simply be reused -- a light POSITION needs the full inverse:
//
//     p_model = R^T * (p_world - T) / scale
//
// Feeding a position through the rotation-only path would place the light at the wrong point
// (translation dropped entirely), and the facing bits would be wrong in a way that looks like a
// silhouette-generation bug rather than a transform bug. The point-light branch inside the facing
// test (which subtracts plane->d) is correct on its own; it is the *input* that would be wrong.
static void stencil_shadow_direction_to_model_space(
	const real_matrix4x3* model_matrix,
	const real_point3d* world_direction,
	real_point3d* out_model_direction)
{
	const real_vector3d* forward = &model_matrix->vectors.forward;
	const real_vector3d* left = &model_matrix->vectors.left;
	const real_vector3d* up = &model_matrix->vectors.up;
	out_model_direction->x = forward->i * world_direction->x + forward->j * world_direction->y + forward->k * world_direction->z;
	out_model_direction->y = left->i * world_direction->x + left->j * world_direction->y + left->k * world_direction->z;
	out_model_direction->z = up->i * world_direction->x + up->j * world_direction->y + up->k * world_direction->z;
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

/* public code */

bool stencil_shadow_section_build(
	const render_model_section* section,
	render_model_section_data* resident_data,
	const real32* position_bounds,
	s_stencil_shadow_section* out_shadow)
{
	memset(out_shadow, 0, sizeof(*out_shadow));
	g_stencil_shadow_build_fail = "unknown";	// it. 472 — overwritten by every failure path below

	if (!resident_data)
	{
		g_stencil_shadow_build_fail = "no-resident-data";
		return false;
	}

	geometry_section* geometry = &resident_data->section;
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
		if (g_stencil_shadow_logged_point_data < 8)
		{
			g_stencil_shadow_logged_point_data++;
			LOG_INFO_GAME("stencil pointdata: raw_points={} runtime_point_data={} rigid_groups={} vertex_point_indices={} class={} verts={}",
				resident_data->point_data.raw_points.count,
				resident_data->point_data.runtime_point_data.size,
				resident_data->point_data.rigid_point_groups.count,
				resident_data->point_data.vertex_point_indices.count,
				(int32)section->section_info.geometry_classification,
				section->section_info.total_vertex_count);
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
		int32 classification = section->global_geometry_classification;
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
				classification, (int32)section->rigid_node, geometry->vertex_buffers.count,
				section->section_info.total_vertex_count);
			for (int32 buffer_index = 0; buffer_index < geometry->vertex_buffers.count; buffer_index++)
			{
				const rasterizer_vertex_buffer* buffer = geometry->vertex_buffers[buffer_index];
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
				for (int32 v = 0; v < section->section_info.total_vertex_count; v++)
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
					min_sum, max_sum, max_index, resident_data->node_map.count);
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
	uint16 weld_vertex_count = section->section_info.opaque_vertex_count;
	if (weld_vertex_count == 0 || weld_vertex_count > section->section_info.total_vertex_count)
	{
		weld_vertex_count = section->section_info.total_vertex_count;
	}

	std::unordered_map<s_position_key, uint16, s_position_key_hasher> weld_lookup;
	// sized by the TOTAL so any vertex index a triangle names is in range; entries past
	// weld_vertex_count stay k_unwelded and are rejected at triangle emission
	static const uint16 k_unwelded = 0xFFFF;
	std::vector<uint16> weld_map(section->section_info.total_vertex_count, k_unwelded);
	std::vector<real_point3d> welded_positions;
	std::vector<uint8> welded_nodes;
	welded_positions.reserve(section->section_info.total_vertex_count);
	welded_nodes.reserve(section->section_info.total_vertex_count);

	const uint8* node_map = resident_data->node_map.count > 0 ? resident_data->node_map[0] : NULL;
	int32 node_map_count = resident_data->node_map.count;
	bool mixed_nodes = false;

	// P1-3: full 4-bone skinning payload, captured per welded vertex when the position stream
	// is declaration 4. td does exactly this blend in section_skin_from_rigid_point_groups
	// (td 0x19EAF0): P' = sum(w_i * (P . M_i)).
	std::vector<uint8> welded_bone_indices;
	std::vector<real32> welded_bone_weights;
	bool has_bone_weights = stencil_shadow_get_skinned_vertex(geometry, 0) != NULL;
	if (has_bone_weights)
	{
		welded_bone_indices.reserve(section->section_info.total_vertex_count * 4);
		welded_bone_weights.reserve(section->section_info.total_vertex_count * 4);
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
	if (resident_data->point_data.vertex_point_indices.count == section->section_info.total_vertex_count
		&& section->section_info.total_vertex_count > 0)
	{
		authored_point_indices = resident_data->point_data.vertex_point_indices[0];
	}
	const bool use_authored_weld = authored_point_indices != NULL;
	std::unordered_map<uint16, uint16> authored_lookup;
	{
		if (!g_stencil_shadow_logged_authored_weld)
		{
			g_stencil_shadow_logged_authored_weld = true;
			LOG_INFO_GAME("stencil weld: authored vertex_point_indices {} (count={} verts={})",
				use_authored_weld ? "IN USE" : "absent — using exact-match heuristic",
				resident_data->point_data.vertex_point_indices.count,
				section->section_info.total_vertex_count);
		}
	}

	for (uint16 vertex_index = 0; vertex_index < weld_vertex_count; vertex_index++)
	{
		real_point3d position;
		int32 local_node = 0;
		stencil_shadow_get_vertex(geometry, position_bounds,
			(uint16)section->section_info.geometry_compression_flags, vertex_index, &position, &local_node);

		// the per-vertex detail byte is a NODE index only on rigid_boned/skinned sections;
		// on plain rigid sections it can carry unrelated data — trusting it there
		// misclassified rigid sections as articulated and transformed their verts by
		// garbage bone matrices (vertices flung to wrong positions: streak fins)
		uint8 model_node = 0;
		if (section->global_geometry_classification >= _geometry_classification_rigid_boned)
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
					(int32)section->global_geometry_classification, node_map_count, local_node);
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
							(int32)section->global_geometry_classification,
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
	if (welded_positions.size() <= 1 && section->section_info.shadow_casting_triangle_count > 0)
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
				(uint32)welded_positions.size(), section->section_info.total_vertex_count,
				(int32)section->global_geometry_classification, decl, stride);
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
	int32 casting_part_count = section->section_info.shadow_casting_part_count;
	if (casting_part_count > geometry->parts.count)
	{
		casting_part_count = geometry->parts.count;
	}
	// SAFETY on an untested assumption: this whole scheme rests on Vista's cache populating
	// shadow_casting_part_count AND on the tool having sorted casting parts to the front. If
	// the count reads 0 while type-based selection can still find casters, taking td's route
	// verbatim would silently build nothing at all -- no triangles, no shadow, no error.
	// Fall back to the type test in that case and say so, rather than fail to a blank screen.
	bool use_type_filter = false;
	if (casting_part_count <= 0)
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
						weld_vertex_count, section->section_info.total_vertex_count);
				}
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
		g_stencil_shadow_build_fail = section->section_info.shadow_casting_triangle_count == 0
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
	// primitives (stencil_shadow_bind_position, stencil_shadow_seam_key) are still present.
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
	if (boundary_edge_count * 10 > (uint32)edges.size())
	{
		LOG_INFO_GAME("stencil shadows: section rejected (non-manifold: {}/{} boundary edges)",
			boundary_edge_count, edges.size());
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
		|| section->global_geometry_classification == _geometry_classification_skinned;

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
		LOG_INFO_GAME("stencil build: verts={} planes={} (expect {}) parts={}/{} (by_type {}, shadow_only {}) class={} extent=({:.3f} x {:.3f} x {:.3f}) quads={} boundary={}/{} samewind={} weld={} decomp={}",
			out_shadow->welded_vertex_count, out_shadow->plane_count,
			section->section_info.shadow_casting_triangle_count,
			casting_part_count, geometry->parts.count, type_selected_parts, shadow_only_parts,
			(int32)section->global_geometry_classification,
			hi.x - lo.x, hi.y - lo.y, hi.z - lo.z, out_shadow->quad_count,
			boundary_edge_count, (uint32)edges.size(), same_winding_candidates,
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
			((uint16)section->section_info.geometry_compression_flags & 1) == 0
				? "not-normalized"
				: (position_bounds ? "applied" : "MISSING-BOUNDS"));
	}
	stencil_shadow_section_validate(out_shadow);
	return true;
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
	delete[] shadow->world_positions;
	memset(shadow, 0, sizeof(*shadow));
}

// P3: transform an articulated section's welded verts into WORLD space by each vertex's
// node matrix, refresh the doubled dynamic VB, and recompute the facing planes in place —
// tag-debug's soft-group recompute (rasterizer_stencilshadow soft planes). After this the
// section draws with IDENTITY node constants and the facing test takes the WORLD-space light.
//
// SKINNING IS FULL-WEIGHT, NOT DOMINANT-NODE. This comment said "dominant-node" long after
// the weighted blend landed below (the `vertex_bone_weights` branch, td's
// section_skin_from_rigid_point_groups). That matters beyond tidiness: the note that disabled
// cross-section stitching gives "ours cannot until full-weight skinning lands" as the reason,
// so a reader trusting this header would conclude the blocker still stands when it does not.
// See td-isq-generation.md it. 468 -- what still blocks stitching is the removed producer,
// not the skinning.
static bool stencil_shadow_section_animate(
	s_stencil_shadow_section* shadow,
	datum object_index,
	const render_model_definition* render_model)
{
	// render_scene runs several times per frame — skip the re-skin when this object's pose
	// is already current (re-animates whenever a different object of the same model draws)
	uint32 frame = *global_frame_index_get();
	if (shadow->last_animated_object == object_index && shadow->last_animated_frame == frame)
	{
		return true;
	}

	// SKINNING MATRIX = node_world x INVERSE BIND, never node_world alone.
	//
	// `render_model_build_skinning` (td 0xA68D0, models\model_skinning.cpp) builds every entry of
	// the engine's skinning pool as exactly this product:
	//     matrix4x3_multiply(object_node_matrix, render_model_node + 68, out);
	//     model_skinning_matrix_from_real_matrix4x3(pool_slot, out);
	// where `render_model_node + 68` is td's inverse bind field == Vista's
	// `render_model_node::default_inverse_matrix`.
	//
	// The field offset is confirmed against RETAIL, not inferred: Vista's
	// `lightmap_raycast_resolve_object_hit` (halo2.exe 0x4B2CD4) indexes the nodes block as
	// `96 * node_index + nodes_base + 40` and hands that straight to
	// `matrix4x3_inverse_transform_point` — stride 96, inverse-bind matrix at +40, which is
	// term-for-term our `render_model_node` layout.
	//
	// Why omitting it looked like a *scale* bug rather than a skinning bug: a bind-pose vertex
	// already sits at its bone's bind position, so transforming it by `node_world` alone applies
	// that bone's whole parent chain translation a SECOND time. Bone translations point outward
	// from the pelvis along each limb, so every vertex is displaced radially outward, roughly in
	// proportion to its distance from the root — indistinguishable at a glance from the model
	// being scaled up about its origin. On a biped it turns limbs into rays: the volume reads as
	// a starfish centred on the object, much larger than the caster, with the torso's overlapping
	// sheets cancelling to an unshadowed hole in the middle. The error is exactly ZERO at bind
	// pose, which is why static props looked fine and only animated casters showed it.
	//
	// `matrix4x3_multiply` (td 0x6F050) leaves the product's scale in the SCALE FIELD
	// (`result->scale = a->scale * b->scale`; basis is the unscaled 3x3 product), so the composed
	// matrix is consumed exactly like a raw one — the `* m->scale` terms below stay. See the
	// scale trap in td-do-not-fix.md before touching them.
	static real_matrix4x3 composed_cache[256];		// node_world x inverse_bind, per node
	const real_matrix4x3* node_matrix_cache[256] = {};	// doubles as the validity flag

	// ONE lookup shared by BOTH the weighted and the dominant-node branch. Each branch used to
	// carry its own copy of this, which is precisely how a transform fix can land in one and not
	// the other — presenting as "only some articulated casters are wrong". Keep it shared.
	auto get_node_matrix = [&](uint8 node) -> const real_matrix4x3*
	{
		if (node_matrix_cache[node])
		{
			return node_matrix_cache[node];
		}
		const real_matrix4x3* world = object_get_node_matrix(object_index, node);
		if (!world || !render_model || (int32)node >= render_model->nodes.count)
		{
			if (!g_stencil_shadow_warned_no_bind)
			{
				g_stencil_shadow_warned_no_bind = true;
				LOG_INFO_GAME("stencil WARNING: no bind matrix for node {} (nodes={}, world={}) — articulated section dropped",
					(int32)node, render_model ? render_model->nodes.count : -1, world ? 1 : 0);
			}
			return NULL;
		}
		// it. 431 PROBE — DIAGNOSTIC ONLY. The interpolated matrix is deliberately NOT used;
		// this measures a gap, it does not close it.
		//
		// it. 419 established that the engine renders objects from INTERPOLATED node matrices
		// (`render_objects.cpp:56` tries `halo_interpolator_interpolate_object_node_matrices`
		// first and only falls back to the raw array), while we build volumes from the RAW tick
		// pose here. So the shadow can be generated from a different pose than the model that is
		// drawn. What nobody has measured is HOW FAR APART the two actually are, and that decides
		// whether the ledger item is worth acting on:
		//
		//   pos_delta ~= 0 and fwd_delta ~= 0  -> interpolation is inactive or irrelevant on this
		//                                         content; it. 419 is a NO-OP and can be closed
		//                                         with no code change at all
		//   either materially non-zero         -> the pose mismatch is real, and its size bounds
		//                                         how visible the lag can be; apply
		//                                         `object_try_get_node_matrix_interpolated`
		//                                         AFTER the geometry fixes are confirmed
		//
		// Measured against the depth/scale references already established: the caster is ~0.705 wu
		// tall (it. 415), so a pos_delta of ~1e-3 wu is negligible and ~1e-2 wu is a visible limb
		// offset.
		//
		// it. 471 REDESIGNED THIS PROBE. The original was ONE-SHOT and compared
		// `object_try_get_node_matrix_interpolated` against `world`. That cannot answer it. 419,
		// because THREE different situations all produce a ~0 delta and the log could not tell
		// them apart:
		//
		//   1. the interpolator DECLINED. `object_try_get_node_matrix_interpolated` falls back to
		//      `*object_get_node_matrix(...)` — literally the same matrix as `world` — whenever
		//      `halo_interpolator_interpolate_object_node_matrix` returns false, which happens if
		//      the object cannot interpolate at all, or if previous->target moved further than
		//      `k_interpolation_distance_cutoff` (900.0, the teleport guard). Delta is then
		//      identically 0 BY CONSTRUCTION and says nothing about interpolation.
		//   2. we sampled at the wrong instant. The interpolator returns
		//      lerp(previous, target, g_interpolator_delta), and `world` is effectively the target,
		//      so the gap is (1 - g_interpolator_delta) * (target - previous). At the end of a tick
		//      that is ~0 even when interpolation is fully active and large mid-tick.
		//   3. the object genuinely is not moving — the only reading that closes it. 419.
		//
		// So: call the interpolator DIRECTLY to capture its bool, and sample ACROSS FRAMES keeping
		// the MAXIMUM, which survives cases 2 and 3. `ran` separates case 1 from the rest.
		//
		// Reading the summary line:
		//   ran=0                      -> interpolation never engaged for this caster; it. 419 is
		//                                 moot HERE, but the reason is the interpolator declining,
		//                                 NOT that poses agree. Do not close the item on this alone.
		//   ran>0 and max_pos ~0       -> interpolation ran and the poses genuinely track. CLOSE it. 419.
		//   ran>0 and max_pos >~1e-2wu -> real pose lag, bounded by max_pos. Apply the interpolated
		//                                 matrix AFTER the geometry fixes are confirmed.
		// Latches reset in stencil_shadow_cache_clear.
		if (!g_stencil_shadow_probed_interpolation)
		{
			real_matrix4x3 interpolated;
			bool ran = halo_interpolator_interpolate_object_node_matrix(
				object_index, (int16)node, &interpolated);
			if (ran)
			{
				g_stencil_shadow_interp_ran++;
				real32 dx = interpolated.position.x - world->position.x;
				real32 dy = interpolated.position.y - world->position.y;
				real32 dz = interpolated.position.z - world->position.z;
				real32 pos_delta = sqrtf(dx * dx + dy * dy + dz * dz);
				real32 cos_theta = interpolated.vectors.forward.i * world->vectors.forward.i
					+ interpolated.vectors.forward.j * world->vectors.forward.j
					+ interpolated.vectors.forward.k * world->vectors.forward.k;
				if (cos_theta > 1.f) { cos_theta = 1.f; }
				if (cos_theta < -1.f) { cos_theta = -1.f; }
				real32 fwd_delta =
					(real32)(acos((real64)cos_theta) * (180.0 / 3.14159265358979323846));
				if (pos_delta > g_stencil_shadow_interp_max_pos) { g_stencil_shadow_interp_max_pos = pos_delta; }
				if (fwd_delta > g_stencil_shadow_interp_max_fwd) { g_stencil_shadow_interp_max_fwd = fwd_delta; }
			}
			// ~4 seconds of samples at 60fps before reporting, so a moving caster is very likely
			// to have been observed mid-tick at least once.
			if (++g_stencil_shadow_interp_samples >= 240)
			{
				g_stencil_shadow_probed_interpolation = true;
				LOG_INFO_GAME("stencil interp probe: samples={} ran={} max_pos_delta={:.5f}wu max_fwd_delta={:.3f}deg (it. 419/471 — ran=0 means the interpolator DECLINED, which is NOT evidence the poses agree)",
					g_stencil_shadow_interp_samples,
					g_stencil_shadow_interp_ran,
					g_stencil_shadow_interp_max_pos,
					g_stencil_shadow_interp_max_fwd);
			}
		}

		matrix4x3_multiply(world, &render_model->nodes[node]->default_inverse_matrix,
			&composed_cache[node]);
		node_matrix_cache[node] = &composed_cache[node];
		return &composed_cache[node];
	};

	for (uint32 welded_index = 0; welded_index < shadow->welded_vertex_count; welded_index++)
	{
		const real_point3d* base = &shadow->base_positions[welded_index];
		real_point3d* world = &shadow->world_positions[welded_index];

		if (shadow->vertex_bone_weights)
		{
			// P1-3: td's blend, section_skin_from_rigid_point_groups (td 0x19EAF0):
			//   P' = sum over bones of w_i * (P . M_i)
			// Dominant-node snapping tore silhouettes exactly at the joints, which is where
			// the silhouette edge usually lives.
			const uint8* bone_indices = &shadow->vertex_bone_indices[welded_index * 4];
			const real32* bone_weights = &shadow->vertex_bone_weights[welded_index * 4];
			real32 x = 0.f, y = 0.f, z = 0.f;
			for (int32 bone = 0; bone < 4; bone++)
			{
				real32 weight = bone_weights[bone];
				if (weight <= 0.f)
				{
					continue;
				}
				uint8 node = bone_indices[bone];
				const real_matrix4x3* m = get_node_matrix(node);
				if (!m)
				{
					return false;
				}
				x += weight * ((m->vectors.forward.i * base->x + m->vectors.left.i * base->y
					+ m->vectors.up.i * base->z) * m->scale + m->position.x);
				y += weight * ((m->vectors.forward.j * base->x + m->vectors.left.j * base->y
					+ m->vectors.up.j * base->z) * m->scale + m->position.y);
				z += weight * ((m->vectors.forward.k * base->x + m->vectors.left.k * base->y
					+ m->vectors.up.k * base->z) * m->scale + m->position.z);
			}
			world->x = x;
			world->y = y;
			world->z = z;
			continue;
		}

		// no weight payload (rigid_boned, or a position format without skinning): single bone.
		// Same composed matrix as the weighted branch above — a bind-pose vertex needs its bind
		// transform undone whether one bone moves it or four.
		uint8 node = shadow->vertex_nodes[welded_index];
		const real_matrix4x3* m = get_node_matrix(node);
		if (!m)
		{
			return false;
		}
		world->x = (m->vectors.forward.i * base->x + m->vectors.left.i * base->y
			+ m->vectors.up.i * base->z) * m->scale + m->position.x;
		world->y = (m->vectors.forward.j * base->x + m->vectors.left.j * base->y
			+ m->vectors.up.j * base->z) * m->scale + m->position.y;
		world->z = (m->vectors.forward.k * base->x + m->vectors.left.k * base->y
			+ m->vectors.up.k * base->z) * m->scale + m->position.z;
	}

	s_stencil_shadow_vertex* vb_data = NULL;
	if (!shadow->shadow_vb
		|| FAILED(shadow->shadow_vb->Lock(0, 0, (void**)&vb_data, D3DLOCK_DISCARD)))
	{
		return false;
	}
	for (uint32 welded_index = 0; welded_index < shadow->welded_vertex_count; welded_index++)
	{
		vb_data[welded_index * 2].position = shadow->world_positions[welded_index];
		vb_data[welded_index * 2].extrude = 0.f;
		vb_data[welded_index * 2 + 1].position = shadow->world_positions[welded_index];
		vb_data[welded_index * 2 + 1].extrude = 1.f;
	}
	shadow->shadow_vb->Unlock();

	// plane recompute from world positions (same math as build)
	for (uint32 triangle_index = 0; triangle_index < shadow->plane_count; triangle_index++)
	{
		const uint16* triangle = &shadow->triangles[triangle_index * 3];
		const real_point3d* p0 = &shadow->world_positions[triangle[0]];
		const real_point3d* p1 = &shadow->world_positions[triangle[1]];
		const real_point3d* p2 = &shadow->world_positions[triangle[2]];
		real_vector3d edge_1 = { p0->x - p1->x, p0->y - p1->y, p0->z - p1->z };
		real_vector3d edge_2 = { p0->x - p2->x, p0->y - p2->y, p0->z - p2->z };
		real_plane3d* plane = &shadow->planes[triangle_index];
		plane->n.i = edge_2.k * edge_1.j - edge_1.k * edge_2.j;
		plane->n.j = edge_1.k * edge_2.i - edge_2.k * edge_1.i;
		plane->n.k = edge_2.j * edge_1.i - edge_1.j * edge_2.i;
		plane->d = plane->n.i * p0->x + plane->n.j * p0->y + plane->n.k * p0->z;
	}
	stencil_shadow_planes_fill_soa(shadow);

	// world-position verification (user-requested): every transformed vertex must stay
	// near its object; an outlier means a wrong node assignment scattered geometry
	// (streak fins). Logged throttled with the offending node.
	//
	// PLUS a measured INFLATION RATIO (it. 319). The outlier threshold below is
	// `radius * 10 + 5` — deliberately loose, so it only catches geometry flung across the map.
	// It therefore could NOT see the bug that actually shipped: the missing inverse-bind term
	// inflated skinned casters by roughly 2x, which is enormous visually (a biped became a
	// starfish) yet nowhere near a 10x threshold. Nothing numeric contradicted the code for
	// several iterations, and the symptom had to be diagnosed from screenshots instead.
	//
	// `max_dist / object.radius` is the sensitive test the loose bound is not. `object.radius` is
	// the engine's own bounding radius for this object, so a correctly skinned section should
	// measure at or a little under 1.0. A ratio near 2 means every vertex is being displaced
	// outward from the root — i.e. the bind transform is being applied twice. This is logged
	// unconditionally (throttled) rather than only on failure, so there is always a baseline to
	// compare against instead of silence meaning "fine".
	{
		const object_datum* object = object_try_and_get(object_index);
		if (object)
		{
			real32 sane_radius = object->object.radius * 10.f + 5.f;
			uint32 outliers = 0;
			uint8 outlier_node = 0;
			real32 max_dist_sq = 0.f;
			for (uint32 i = 0; i < shadow->welded_vertex_count; i++)
			{
				real32 dx = shadow->world_positions[i].x - object->object.position.x;
				real32 dy = shadow->world_positions[i].y - object->object.position.y;
				real32 dz = shadow->world_positions[i].z - object->object.position.z;
				real32 dist_sq = dx * dx + dy * dy + dz * dz;
				if (dist_sq > max_dist_sq)
				{
					max_dist_sq = dist_sq;
				}
				if (dist_sq > sane_radius * sane_radius)
				{
					if (!outliers)
					{
						outlier_node = shadow->vertex_nodes[i];
					}
					outliers++;
				}
			}
			static uint32 dbg_outlier_log = 0;
			if (outliers && ++dbg_outlier_log % 300 == 1)
			{
				LOG_INFO_GAME("stencil VERTEX OUTLIERS: {}/{} world verts beyond {:.1f}wu of object (first node={})",
					outliers, shadow->welded_vertex_count, sane_radius, outlier_node);
			}

			// INFLATION RATIO — normalised against the section's own BIND-POSE extent, not
			// against object.radius (corrected it. 331).
			//
			// The first version divided by `object->object.radius` and asserted "expect ~1.0". That
			// was unverified: `radius` is the object's own bounding/collision radius and there is no
			// established relationship between it and a section's vertex spread, so the baseline
			// could sit anywhere. A diagnostic whose "correct" value is unknown is worse than none —
			// it invites reading a healthy 2.0 as the very bug it was written to find.
			//
			// `world_extent / bind_extent` has a baseline that follows from the geometry instead of
			// from a convention: skinning is a rigid-ish rearrangement of the SAME vertices, so an
			// animated pose moves the extent somewhat but cannot double it. Re-applying each bone's
			// bind translation displaces every vertex outward roughly in proportion to its distance
			// from the root, which IS a doubling. Both extents are measured about their own centroid
			// so a translated object reads the same as one at the origin.
			static uint32 dbg_inflation_log = 0;
			if (shadow->welded_vertex_count > 0 && (dbg_inflation_log++ % 600) == 0)
			{
				real_point3d bind_centre = { 0.f, 0.f, 0.f };
				real_point3d world_centre = { 0.f, 0.f, 0.f };
				const real32 inv_count = 1.f / (real32)shadow->welded_vertex_count;
				for (uint32 i = 0; i < shadow->welded_vertex_count; i++)
				{
					bind_centre.x += shadow->base_positions[i].x * inv_count;
					bind_centre.y += shadow->base_positions[i].y * inv_count;
					bind_centre.z += shadow->base_positions[i].z * inv_count;
					world_centre.x += shadow->world_positions[i].x * inv_count;
					world_centre.y += shadow->world_positions[i].y * inv_count;
					world_centre.z += shadow->world_positions[i].z * inv_count;
				}
				real32 bind_max_sq = 0.f, world_max_sq = 0.f;
				for (uint32 i = 0; i < shadow->welded_vertex_count; i++)
				{
					real32 bx = shadow->base_positions[i].x - bind_centre.x;
					real32 by = shadow->base_positions[i].y - bind_centre.y;
					real32 bz = shadow->base_positions[i].z - bind_centre.z;
					real32 b = bx * bx + by * by + bz * bz;
					if (b > bind_max_sq) { bind_max_sq = b; }
					real32 wx = shadow->world_positions[i].x - world_centre.x;
					real32 wy = shadow->world_positions[i].y - world_centre.y;
					real32 wz = shadow->world_positions[i].z - world_centre.z;
					real32 w = wx * wx + wy * wy + wz * wz;
					if (w > world_max_sq) { world_max_sq = w; }
				}
				real32 bind_extent = (real32)sqrt((real64)bind_max_sq);
				real32 world_extent = (real32)sqrt((real64)world_max_sq);
				// it. 473 CORRECTED THIS LINE'S MEANING. It used to read
				// "expect ~1.0; >=1.8 = bind applied twice", which is unfounded — and "applied
				// twice" is precisely the case this metric CANNOT see.
				//
				// `ratio` compares max-distance-from-centroid before and after transforming. A
				// RIGID transform preserves distances up to its scale factor, and the centroid maps
				// to the centroid, so for a single-node section:
				//
				//     ratio == m->scale     EXACTLY, and identically so whether the inverse bind was
				//                           applied once, not at all, or twice
				//
				// because composing rigid transforms is still one rigid transform. Only the SCALE
				// factors differ between those cases, and the inverse binds measured live are
				// orthonormal with scale 1 (it. 413) — so all three read ~1.0. On the static path
				// this number is a tautology, not a confirmation.
				//
				// It is only informative on the SKINNED branch, where vertices receive DIFFERENT
				// matrices and the cloud genuinely deforms. Even there it does not isolate a defect:
				// a biped mid-animation legitimately has a different extent than its bind pose. What
				// it does bound is the starfish — a missing inverse bind scatters vertices by
				// per-node amounts and inflates the extent far beyond any real pose.
				//
				//   branch=dominant-node  -> ratio is m->scale. Carries NO information about the bind.
				//   branch=weighted, ratio ~1-2   -> ordinary animated deformation
				//   branch=weighted, ratio >>2    -> vertices flying apart: the it. 317 starfish
				//
				// Use `stencil bindprobe:` (bind_offset / bind_rot) to ask whether composing the bind
				// MATTERS, and it. 412's bit-exact check for whether it HAPPENS. This line answers
				// neither; it measures how far the volume is spread.
				LOG_INFO_GAME("stencil inflation: obj={} bind_extent={:.3f}wu world_extent={:.3f}wu ratio={:.2f} (it. 473: on branch=dominant-node this is just m->scale and means NOTHING; on branch=weighted >>2 = starfish) verts={} branch={} obj_radius={:.3f} max_from_origin={:.3f}",
					(uint32)object_index, bind_extent, world_extent,
					bind_extent > 0.0001f ? world_extent / bind_extent : -1.f,
					shadow->welded_vertex_count,
					shadow->vertex_bone_weights ? "weighted" : "dominant-node",
					object->object.radius, (real32)sqrt((real64)max_dist_sq));
			}
		}
	}

	shadow->last_animated_object = object_index;
	shadow->last_animated_frame = frame;
	return true;
}

void stencil_shadow_build_facing_bitvector(
	const s_stencil_shadow_section* shadow,
	const real_point3d* light_position,
	bool point_light,
	uint32* out_bitvector)
{
	// Port of tag-debug rasterizer_stencilshadow_build_bitvector_from_rigid_groups:
	// sign of dot(light, N) (- d for point lights) packed LSB-first in 4-bit groups via
	// SSE movemask over the SoA 4-blocks; completed words are inverted so bit == 1 means
	// the triangle FACES the light — the original's exact fast path.
	if (shadow->planes_soa)
	{
		__m128 light_x = _mm_set1_ps(light_position->x);
		__m128 light_y = _mm_set1_ps(light_position->y);
		__m128 light_z = _mm_set1_ps(light_position->z);
		uint32 block_count = (shadow->plane_count + 3) / 4;
		uint32 word = 0;
		uint32 bit_position = 0;
		uint32* out_word = out_bitvector;
		for (uint32 block = 0; block < block_count; block++)
		{
			const real32* soa = &shadow->planes_soa[block * 16];
			__m128 dot = _mm_mul_ps(_mm_loadu_ps(soa), light_x);
			dot = _mm_add_ps(dot, _mm_mul_ps(_mm_loadu_ps(soa + 4), light_y));
			dot = _mm_add_ps(dot, _mm_mul_ps(_mm_loadu_ps(soa + 8), light_z));
			if (point_light)
			{
				dot = _mm_sub_ps(dot, _mm_loadu_ps(soa + 12));
			}
			word |= (uint32)_mm_movemask_ps(dot) << bit_position;
			bit_position += 4;
			if (bit_position == 32)
			{
				*out_word++ = ~word;
				word = 0;
				bit_position = 0;
			}
		}
		if (bit_position != 0)
		{
			*out_word = ~word;
		}
		return;
	}

	// SCALAR REFERENCE PATH — CURRENTLY UNREACHABLE (verified it. 248).
	//
	// The SSE block above is guarded by `if (shadow->planes_soa)` and returns unconditionally, and
	// `planes_soa` is allocated with `new real32[...]` in the builder, which throws on failure and
	// never yields NULL. So a validly built section always takes the SSE path and never arrives
	// here. (There is no non-SSE target to fall back to either: the project builds /arch:SSE2.)
	//
	// Kept deliberately: it is the readable statement of what the SSE block computes, and the two
	// were checked equivalent — SSE takes `_mm_movemask_ps` sign bits (1 == negative) and inverts
	// the word; this sets a bit on `dot < 0` and inverts the same way. Both end with bit == 1
	// meaning `dot >= 0`, i.e. the triangle FACES the light.
	//
	// If it is ever revived, note the tail write below is unconditional where the SSE path guards
	// its own with `if (bit_position != 0)`. That is harmless at present (an exact multiple of 32
	// planes writes index plane_count/32 <= 1023, inside the 1024-word bitvector) but it is a
	// redundant write and would need the same guard if the plane cap ever rose.
	uint32 word = 0;
	uint32 bit_position = 0;
	uint32* out_word = out_bitvector;

	for (uint32 plane_index = 0; plane_index < shadow->plane_count; plane_index++)
	{
		const real_plane3d* plane = &shadow->planes[plane_index];
		real32 dot = plane->n.i * light_position->x
			+ plane->n.j * light_position->y
			+ plane->n.k * light_position->z;
		if (point_light)
		{
			dot -= plane->d;
		}

		if (dot < 0.f)
		{
			word |= 1u << bit_position;
		}
		if (++bit_position == 32)
		{
			*out_word++ = ~word;
			word = 0;
			bit_position = 0;
		}
	}
	*out_word = ~word;
}

void stencil_shadow_section_draw(
	const s_stencil_shadow_section* shadow,
	const uint32* facing_bitvector,
	const real_point3d* light_position,
	bool point_light,
	const real_matrix4x3* model_matrix,
	real32 extrusion_distance,
	real32 opacity)
{
	// Full z-fail volume: silhouette quads (as triangle pairs) + near and far caps, drawn with
	// two-sided stencil. The old "P2 TODO: vertex shader constants, caps for z-fail,
	// cross-section stitching" note described none of this -- all three have been in place for
	// some time; cross-section stitching lives in the caller (s_stencil_shadow_cross_quad).
	//
	// Cap construction matches tag-debug's caps_internal (td 0x9C700), which draws two batches
	// from two separate index lists: the near batch with v11.x = 0 (no extrusion) and the far
	// batch with v11.x = 1 (full extrusion). We encode the same distinction in the indices
	// themselves -- 2v selects the unextruded copy, 2v+1 the extruded one -- so one draw does
	// what td does in two.
	IDirect3DDevice9Ex* device = rasterizer_dx9_device_get_interface();
	if (!device || !shadow->valid)
	{
		return;
	}

	// silhouette index generation (doubled indices: 2v = original, 2v+1 = extruded)
	std::vector<uint16> indices;
	indices.reserve(shadow->quad_count * 6);
	for (uint32 quad_index = 0; quad_index < shadow->quad_count; quad_index++)
	{
		const s_stencil_shadow_quad* quad = &shadow->quads[quad_index];
		if (quad->tri_right == k_stencil_shadow_matched_boundary)
		{
			continue;	// seam bridged by the model's cross-quad pass
		}
		bool left_faces = (facing_bitvector[quad->tri_left >> 5] >> (quad->tri_left & 31)) & 1;
		bool right_faces = quad->tri_right != k_stencil_shadow_boundary_triangle
			&& ((facing_bitvector[quad->tri_right >> 5] >> (quad->tri_right & 31)) & 1);

		if (left_faces == right_faces)
		{
			continue;
		}

		// order the quad so its front faces away from the lit triangle; same-winding
		// source pairs never swap (both sides want the stored orientation)
		uint16 vert_a = quad->vert_a;
		uint16 vert_b = quad->vert_b;
		bool same_winding = shadow->quad_same_winding_bits
			&& ((shadow->quad_same_winding_bits[quad_index >> 5] >> (quad_index & 31)) & 1);
		// Swap side selects the quad's winding. `right_faces` is ours; `left_faces` reproduces td's
		// ordering exactly (both arms checked against td 0x1A16B0). See the flag's comment.
		bool swap_side = g_stencil_shadow_quad_winding_flip ? left_faces : right_faces;
		if (swap_side && !same_winding)
		{
			uint16 swap = vert_a;
			vert_a = vert_b;
			vert_b = swap;
		}

		uint16 a0 = (uint16)(vert_a * 2), a1 = (uint16)(vert_a * 2 + 1);
		uint16 b0 = (uint16)(vert_b * 2), b1 = (uint16)(vert_b * 2 + 1);
		// quad (a0, b0, b1, a1) as two triangles
		indices.push_back(a0); indices.push_back(b0); indices.push_back(b1);
		indices.push_back(a0); indices.push_back(b1); indices.push_back(a1);
	}

	// caps close the volume for z-fail counting (tag-debug convention: front cap =
	// light-facing triangles at original positions; back cap = NON-facing triangles at
	// extruded positions — their winding already faces outward at the far end)
	for (uint32 triangle_index = 0; triangle_index < shadow->plane_count; triangle_index++)
	{
		const uint16* triangle = &shadow->triangles[triangle_index * 3];
		bool faces = (facing_bitvector[triangle_index >> 5] >> (triangle_index & 31)) & 1;
		if (faces)
		{
			indices.push_back((uint16)(triangle[0] * 2));
			indices.push_back((uint16)(triangle[1] * 2));
			indices.push_back((uint16)(triangle[2] * 2));
		}
		else
		{
			indices.push_back((uint16)(triangle[0] * 2 + 1));
			indices.push_back((uint16)(triangle[1] * 2 + 1));
			indices.push_back((uint16)(triangle[2] * 2 + 1));
		}
	}

	if (indices.empty())
	{
		return;
	}

	if (!stencil_shadow_shaders_initialize(device))
	{
		return;
	}

	// NO state blocks: on this device D3DSBT_ALL capture is unreliable (pure-device/9Ex)
	// and its Apply() corrupts live state (bisected in-game). Instead: render states go
	// through the engine-cache-aware wrapper, and after our raw binds we call
	// stencil_shadow_release_pipeline so the engine re-binds on its next draw.
	if (g_stencil_shadow_draw_mode == 1)
	{
		// red volume visualization: color writes on, no stencil writes
		stencil_shadow_force_render_state(D3DRS_COLORWRITEENABLE,
			D3DCOLORWRITEENABLE_RED | D3DCOLORWRITEENABLE_GREEN | D3DCOLORWRITEENABLE_BLUE);
		stencil_shadow_force_render_state(D3DRS_ALPHABLENDENABLE, TRUE);
		stencil_shadow_force_render_state(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
		stencil_shadow_force_render_state(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
		stencil_shadow_force_render_state(D3DRS_BLENDOP, D3DBLENDOP_ADD);
		stencil_shadow_force_render_state(D3DRS_FOGENABLE, FALSE);
		stencil_shadow_force_render_state(D3DRS_ALPHATESTENABLE, FALSE);
		stencil_shadow_force_render_state(D3DRS_STENCILENABLE, FALSE);
	}
	else
	{
		// real shadows: stencil count only, Z-FAIL (Carmack's reverse) over CLOSED volumes —
		// correct from any camera position including inside the volume
		stencil_shadow_force_render_state(D3DRS_COLORWRITEENABLE, 0);
		stencil_shadow_force_render_state(D3DRS_ALPHABLENDENABLE, FALSE);
		// Alpha test runs BEFORE the stencil stage, so an inherited enabled alpha test would
		// discard volume fragments before any count is written -- no shadow at all, and no
		// error. Our fragments have no meaningful alpha (colour writes are off), so we must not
		// let the engine's state decide. td disables it at layer open for the same reason
		// (0x3C ALPHA_TEST = 0, td-shadow-layer-state.md). Previously this was set only in the
		// mode-1 debug branch below, leaving the shipping path dependent on inherited state.
		stencil_shadow_force_render_state(D3DRS_ALPHATESTENABLE, FALSE);
		stencil_shadow_force_render_state(D3DRS_STENCILENABLE, TRUE);
		stencil_shadow_force_render_state(D3DRS_TWOSIDEDSTENCILMODE, TRUE);
		stencil_shadow_force_render_state(D3DRS_STENCILREF, 0);
		stencil_shadow_force_render_state(D3DRS_STENCILMASK, 0xFFFFFFFF);
		stencil_shadow_force_render_state(D3DRS_STENCILWRITEMASK, 0xFFFFFFFF);
		stencil_shadow_force_render_state(D3DRS_STENCILFUNC, D3DCMP_ALWAYS);
		stencil_shadow_force_render_state(D3DRS_STENCILPASS, D3DSTENCILOP_KEEP);
		stencil_shadow_force_render_state(D3DRS_STENCILZFAIL, D3DSTENCILOP_INCR);
		stencil_shadow_force_render_state(D3DRS_STENCILFAIL, D3DSTENCILOP_KEEP);
		stencil_shadow_force_render_state(D3DRS_CCW_STENCILFUNC, D3DCMP_ALWAYS);
		stencil_shadow_force_render_state(D3DRS_CCW_STENCILPASS, D3DSTENCILOP_KEEP);
		stencil_shadow_force_render_state(D3DRS_CCW_STENCILZFAIL, D3DSTENCILOP_DECR);
		stencil_shadow_force_render_state(D3DRS_CCW_STENCILFAIL, D3DSTENCILOP_KEEP);
	}
	stencil_shadow_force_render_state(D3DRS_ZWRITEENABLE, FALSE);
	stencil_shadow_force_render_state(D3DRS_ZENABLE, TRUE);
	stencil_shadow_force_render_state(D3DRS_CULLMODE, D3DCULL_NONE);
	// never inherit these from the surrounding pass: a stale scissor rect or an active user
	// clip plane silently discards every volume fragment
	stencil_shadow_force_render_state(D3DRS_SCISSORTESTENABLE, FALSE);
	stencil_shadow_force_render_state(D3DRS_CLIPPLANEENABLE, 0);
	stencil_shadow_force_render_state(D3DRS_FILLMODE, D3DFILL_SOLID);

	// D14 — depth func and bias are LOAD-BEARING for z-fail counting, not cosmetic.
	// td's shipping path: rasterizer_stencilshadow_shadows_begin (td 0x21D820) sets Halo
	// render state 0x39, which the token table at td 0x4351F0 maps to NV2A method 0x354
	// (DEPTH_FUNC), to 0x201 = GL_LESS -- and never inverts it (the GEQUAL variant lives only
	// in the byte_53A685 alternate mode). Counting is on state 0x44 = method 0x374 =
	// STENCIL_OP_ZFAIL, with 0x45 = 0x378 = STENCIL_OP_ZPASS held at KEEP.
	//
	// With LESS, a near-cap fragment sitting exactly at its caster's surface depth FAILS the
	// depth test and therefore COUNTS. td relies on that: it draws caps from the model's own
	// vertex buffer at the model's own indices, so the depths are bit-identical and the
	// coincidence is deliberate.
	//
	// We previously used LESSEQUAL plus a toward-camera depth bias chosen to make exactly
	// those fragments PASS -- which silently removed the light cap's entire contribution and
	// left the volume open at the light end, so counts never cancelled outside it. That is a
	// shadow far larger than its caster and completely insensitive to extrusion distance.
	stencil_shadow_force_render_state(D3DRS_ZFUNC, D3DCMP_LESS);
	stencil_shadow_force_render_state(D3DRS_DEPTHBIAS, 0);
	stencil_shadow_force_render_state(D3DRS_SLOPESCALEDEPTHBIAS, 0);

	stencil_shadow_set_node_constants(model_matrix);

	// Convention (matches tag debug 0x82B954 usage): light_position is the point light position,
	// or for directional lights the vector TOWARD the light. Shader extrudes along
	// pos*c4.w - c4.xyz, so both cases pass through unchanged (directional: -toward = away).
	real32 light_constant[4];
	light_constant[0] = light_position->x;
	light_constant[1] = light_position->y;
	light_constant[2] = light_position->z;
	light_constant[3] = point_light ? 1.f : 0.f;
	real32 extrusion_constant[4] = { extrusion_distance, 0.f, 0.f, 0.f };
	device->SetVertexShaderConstantF(k_stencil_shadow_light_constant, light_constant, 1);
	device->SetVertexShaderConstantF(k_stencil_shadow_extrusion_distance_constant, extrusion_constant, 1);

	uint32 index_count = (uint32)indices.size();
	if (index_count > k_stencil_shadow_index_buffer_capacity)
	{
		// Reachable in principle: the plane cap admits 32767 triangles, and a mesh with no
		// shared edges yields up to 3T edges -> 3 * 32767 * 6 indices, which exceeds the
		// buffer. Closed meshes give ~1.5T and stay well under, so this should never fire --
		// but truncating silently draws a PARTIAL volume that reads as a correctness bug
		// rather than a capacity one. Say it once.
		if (!g_stencil_shadow_warned_index_overflow_volume)
		{
			g_stencil_shadow_warned_index_overflow_volume = true;
			LOG_INFO_GAME("stencil WARNING: index buffer overflow — {} indices truncated to {} (partial volume)",
				index_count, (uint32)k_stencil_shadow_index_buffer_capacity);
		}
		index_count = k_stencil_shadow_index_buffer_capacity;
	}
	void* ib_data = NULL;
	if (FAILED(g_stencil_shadow_index_buffer->Lock(0, index_count * sizeof(uint16), &ib_data, D3DLOCK_DISCARD)))
	{
		return;
	}
	memcpy(ib_data, indices.data(), index_count * sizeof(uint16));
	g_stencil_shadow_index_buffer->Unlock();

	device->SetVertexDeclaration(g_stencil_shadow_vertex_declaration);
	// td stipple fade: fragments screen-door-clipped at the object's shadow opacity so
	// faded objects mark proportionally fewer stencil pixels (SM3 pair required)
	bool stipple = g_stencil_shadow_draw_mode != 1 && opacity < 0.995f
		&& g_stencil_shadow_vertex_shader_sm3 && g_stencil_shadow_stipple_shader;
	device->SetVertexShader(stipple
		? g_stencil_shadow_vertex_shader_sm3 : g_stencil_shadow_vertex_shader);
	if (g_stencil_shadow_draw_mode == 1)
	{
		const real32 tint[4] = { 1.f, 0.125f, 0.125f, 0.375f };
		device->SetPixelShader(g_stencil_shadow_pixel_shader);
		device->SetPixelShaderConstantF(k_stencil_shadow_tint_constant, tint, 1);
	}
	else if (stipple)
	{
		const real32 stipple_constant[4] = { opacity, 0.f, 0.f, 0.f };
		device->SetPixelShader(g_stencil_shadow_stipple_shader);
		device->SetPixelShaderConstantF(30, stipple_constant, 1);
	}
	else
	{
		device->SetPixelShader(NULL);
	}
	device->SetStreamSource(0, shadow->shadow_vb, 0, sizeof(s_stencil_shadow_vertex));
	device->SetIndices(g_stencil_shadow_index_buffer);

	HRESULT draw_result = device->DrawIndexedPrimitive(
		D3DPT_TRIANGLELIST,
		0,
		0,
		shadow->welded_vertex_count * 2,
		0,
		index_count / 3);
	stencil_shadow_force_render_state(D3DRS_DEPTHBIAS, 0);

	if (FAILED(draw_result))
	{
		LOG_INFO_GAME("stencil draw FAILED: hr={:#x} indices={} verts={} mode={}",
			(uint32)draw_result, index_count, shadow->welded_vertex_count * 2,
			g_stencil_shadow_draw_mode);
	}

	stencil_shadow_release_pipeline(device);
}

// Project a world-space sphere to a clip-space screen rect (conservative: 8 box corners
// through the world->clip composite). Returns false when the whole sphere is behind the
// camera.
// CURRENTLY UNREFERENCED: its only caller was the per-object scissored darken, removed for
// td parity (P1-1 -- td's application is one unscissored fullscreen quad). Kept as a working
// projection helper for scissored debug passes.
static bool stencil_shadow_compute_screen_rect(
	const real_point3d* center,
	real32 radius,
	RECT* out_rect)
{
	real32 world_to_clip[4][4];
	stencil_shadow_get_world_to_clip(world_to_clip);

	D3DVIEWPORT9 viewport;
	if (FAILED(rasterizer_dx9_device_get_interface()->GetViewport(&viewport)))
	{
		return false;
	}

	real32 min_x = 1.f, min_y = 1.f, max_x = -1.f, max_y = -1.f;
	bool any_in_front = false;
	for (int32 corner = 0; corner < 8; corner++)
	{
		real32 world[4] =
		{
			center->x + ((corner & 1) ? radius : -radius),
			center->y + ((corner & 2) ? radius : -radius),
			center->z + ((corner & 4) ? radius : -radius),
			1.f
		};
		real32 clip[4];
		for (int32 row = 0; row < 4; row++)
		{
			clip[row] = world_to_clip[row][0] * world[0] + world_to_clip[row][1] * world[1]
				+ world_to_clip[row][2] * world[2] + world_to_clip[row][3];
		}
		if (clip[3] <= 0.001f)
		{
			// corner behind the camera: the rect is unbounded on some side — go fullscreen
			min_x = min_y = -1.f;
			max_x = max_y = 1.f;
			any_in_front = true;
			break;
		}
		any_in_front = true;
		real32 ndc_x = clip[0] / clip[3];
		real32 ndc_y = clip[1] / clip[3];
		if (ndc_x < min_x) min_x = ndc_x;
		if (ndc_x > max_x) max_x = ndc_x;
		if (ndc_y < min_y) min_y = ndc_y;
		if (ndc_y > max_y) max_y = ndc_y;
	}
	if (!any_in_front || max_x < -1.f || min_x > 1.f || max_y < -1.f || min_y > 1.f)
	{
		return false;
	}

	if (min_x < -1.f) min_x = -1.f;
	if (max_x > 1.f) max_x = 1.f;
	if (min_y < -1.f) min_y = -1.f;
	if (max_y > 1.f) max_y = 1.f;

	out_rect->left = viewport.X + (LONG)((min_x * 0.5f + 0.5f) * viewport.Width);
	out_rect->right = viewport.X + (LONG)((max_x * 0.5f + 0.5f) * viewport.Width) + 1;
	// NDC y is up, screen y is down
	out_rect->top = viewport.Y + (LONG)((-max_y * 0.5f + 0.5f) * viewport.Height);
	out_rect->bottom = viewport.Y + (LONG)((-min_y * 0.5f + 0.5f) * viewport.Height) + 1;
	return out_rect->right > out_rect->left && out_rect->bottom > out_rect->top;
}

// Apply: darken every pixel whose stencil count != 128 (midpoint convention), optionally
// scissored to a rect, with the given darkness. Caller manages stencil clears.
void stencil_shadow_apply_and_clear(real32 darkness, const RECT* scissor)
{
	IDirect3DDevice9Ex* device = rasterizer_dx9_device_get_interface();
	if (!device || !g_stencil_shadow_pixel_shader)
	{
		// This was a SILENT total failure: the volumes pass can count perfectly and every other
		// diagnostic read clean while nothing was ever darkened, because the apply is the only
		// stage that turns counts into pixels. "No shadow and no error" is the hardest state to
		// diagnose from a log, so it must announce itself. (it. 327)
		if (!g_stencil_shadow_warned_no_apply)
		{
			g_stencil_shadow_warned_no_apply = true;
			LOG_INFO_GAME("stencil WARNING: apply skipped — device={} pixel_shader={} — NOTHING will be darkened",
				device ? 1 : 0, g_stencil_shadow_pixel_shader ? 1 : 0);
		}
		return;
	}

	// P1-1 verification hook. Removing the per-object scissored darken in favour of ONE fullscreen
	// apply was recorded IMPLEMENTED / UNVERIFIED IN GAME (td-vista-required-fixes.md) and had no
	// diagnostic at all, so a run could not confirm it. `scissor=0` and exactly one call per frame
	// is what "one fullscreen apply" means; a non-zero scissor or a per-caster call count would show
	// the old behaviour had survived somewhere.
	//
	// The count is reported at the FRAME BOUNDARY, for the frame that just ended — not mid-frame.
	// The first version of this throttled on the call counter and printed `applies_this_frame` as it
	// stood at that moment, which is a PARTIAL count: if the throttle happened to fire on the first
	// call of a frame it printed 1 no matter how many followed. That is worthless precisely in the
	// case the diagnostic exists to catch (a per-caster apply), where it would have read
	// `calls=1` and looked correct — the same under-reporting-while-clean failure the `balance=`
	// term was added to prevent. Self-review caught it in it. 331.
	{
		static uint32 applies_this_frame = 0;
		static uint32 last_apply_frame = 0xFFFFFFFF;
		static uint32 apply_log_counter = 0;
		static real32 last_darkness = 0.f;
		static bool last_scissored = false;
		uint32 frame = *global_frame_index_get();
		if (frame != last_apply_frame)
		{
			// `applies_this_frame` is now the COMPLETE tally for the previous frame
			if (last_apply_frame != 0xFFFFFFFF && (apply_log_counter++ % 600) == 0)
			{
				LOG_INFO_GAME("stencil apply: darkness={:.2f} scissor={} mode={} calls_in_frame={} (expect scissor=0, calls=1)",
					last_darkness, last_scissored ? 1 : 0, (int32)g_stencil_shadow_draw_mode,
					applies_this_frame);
			}
			last_apply_frame = frame;
			applies_this_frame = 0;
		}
		applies_this_frame++;
		last_darkness = darkness;
		last_scissored = scissor != NULL;
	}

	if (scissor)
	{
		device->SetScissorRect(scissor);
		stencil_shadow_force_render_state(D3DRS_SCISSORTESTENABLE, TRUE);
	}

	stencil_shadow_force_render_state(D3DRS_COLORWRITEENABLE,
		D3DCOLORWRITEENABLE_RED | D3DCOLORWRITEENABLE_GREEN | D3DCOLORWRITEENABLE_BLUE);
	stencil_shadow_force_render_state(D3DRS_ALPHABLENDENABLE, TRUE);
	stencil_shadow_force_render_state(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
	stencil_shadow_force_render_state(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
	stencil_shadow_force_render_state(D3DRS_BLENDOP, D3DBLENDOP_ADD);
	stencil_shadow_force_render_state(D3DRS_FOGENABLE, FALSE);
	stencil_shadow_force_render_state(D3DRS_ALPHATESTENABLE, FALSE);
	stencil_shadow_force_render_state(D3DRS_ZENABLE, FALSE);
	stencil_shadow_force_render_state(D3DRS_ZWRITEENABLE, FALSE);
	stencil_shadow_force_render_state(D3DRS_CULLMODE, D3DCULL_NONE);
	// mode 2 now TESTS the stencil like the real mask does — green = pixels the volume
	// counts marked as shadowed (mask visualizer, drawn at the volumes-pass position)
	stencil_shadow_force_render_state(D3DRS_STENCILENABLE, TRUE);
	stencil_shadow_force_render_state(D3DRS_TWOSIDEDSTENCILMODE, FALSE);
	stencil_shadow_force_render_state(D3DRS_STENCILREF, 128);	// tag-debug midpoint convention
	// Set the READ mask explicitly rather than inheriting it. The comparison is
	// (mask & stencil) != (mask & 128), so an inherited mask of 0 would make it 0 != 0 --
	// false for every pixel, darkening nothing, with no error anywhere. It happens to be
	// 0xFFFFFFFF here because the volumes pass sets it, but that is an implicit dependency on
	// call order which the volume sites themselves are careful not to rely on (td sets
	// 0x48 FUNC_MASK = 0xFF at layer open for the same reason).
	stencil_shadow_force_render_state(D3DRS_STENCILMASK, 0xFFFFFFFF);
	stencil_shadow_force_render_state(D3DRS_STENCILFUNC, D3DCMP_NOTEQUAL);
	stencil_shadow_force_render_state(D3DRS_STENCILPASS, D3DSTENCILOP_KEEP);
	stencil_shadow_force_render_state(D3DRS_STENCILZFAIL, D3DSTENCILOP_KEEP);
	stencil_shadow_force_render_state(D3DRS_STENCILFAIL, D3DSTENCILOP_KEEP);

	// diagnostic mode paints opaque green so any stencil-test malfunction is unmissable
	const bool diagnostic = g_stencil_shadow_draw_mode == 2;
	const real32 shadow_color[4] =
	{
		0.f,
		diagnostic ? 1.f : 0.f,
		0.f,
		diagnostic ? 1.f : darkness
	};
	device->SetVertexShader(NULL);
	device->SetPixelShader(g_stencil_shadow_pixel_shader);
	device->SetPixelShaderConstantF(k_stencil_shadow_tint_constant, shadow_color, 1);
	device->SetFVF(D3DFVF_XYZRHW);

	// pretransformed quad far larger than any viewport; clipped to the target
	const real32 quad[4][4] =
	{
		{ -8.f, -8.f, 0.f, 1.f },
		{ 16384.f, -8.f, 0.f, 1.f },
		{ -8.f, 16384.f, 0.f, 1.f },
		{ 16384.f, 16384.f, 0.f, 1.f },
	};
	// MRT slot 1 hardening. On the SM3 path render.cpp binds the "depth as colour target"
	// surface to slot 1 for the whole span that contains this call (it is only restored after
	// the darken), and our pixel shader writes oC0 only -- so oC1 would be UNDEFINED for every
	// pixel of a fullscreen quad. D3D9 leaves an unwritten MRT slot implementation-defined;
	// most drivers leave it alone, which is why this has likely never bitten, but the surface
	// in question is the linear depth consumed downstream by fog, DOF and soft particles, so
	// the failure mode would be global banding/noise rather than anything shadow-shaped.
	//
	// Save and restore rather than forcing 0xF: we never otherwise touch this state, so its
	// value belongs to the engine and must be handed back exactly as found. Devices without
	// independent write masks simply ignore both calls.
	DWORD saved_colorwrite1 = 0xF;
	bool colorwrite1_saved =
		SUCCEEDED(device->GetRenderState(D3DRS_COLORWRITEENABLE1, &saved_colorwrite1));
	stencil_shadow_force_render_state(D3DRS_COLORWRITEENABLE1, 0);

	device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, quad, sizeof(quad[0]));

	if (colorwrite1_saved)
	{
		stencil_shadow_force_render_state(D3DRS_COLORWRITEENABLE1, saved_colorwrite1);
	}

	if (scissor)
	{
		stencil_shadow_force_render_state(D3DRS_SCISSORTESTENABLE, FALSE);
	}
	stencil_shadow_force_render_state(D3DRS_STENCILENABLE, FALSE);
	stencil_shadow_release_pipeline(device);
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

struct s_stencil_shadow_model_cross
{
	bool built;
	std::vector<s_stencil_shadow_cross_quad> quads;
};

static std::unordered_map<uint32, s_stencil_shadow_model_cross> g_stencil_shadow_cross_cache;

// Bind-pose model-space position of a node-local vertex: the node's default_inverse_matrix
// maps bind-model -> node-local (p_local = B*p*s + t), so p = Bt*(p_local - t)/s.
// DEAD, AND ITS PREMISE IS WRONG — do not wire this into the stitching restoration (it. 336).
//
// It converts a NODE-LOCAL position into bind-model space by inverting `default_inverse_matrix`.
// That input convention does not match our data: section vertices are **bind-pose MODEL space**,
// established in it. 333-335 from both binaries —
//   * td `rasterizer_model_section_draw` (0x10F0E0) transforms a section by a SKINNING POOL matrix
//     (`model_skinning_get_node_matrix(skinning, rigid_node, 0)`), and pool entries are
//     `node_world x inverse_bind`; the inverse bind is only needed if the input is bind-model.
//   * Vista `render_visible_section_set_transform_constants` (0x680A68) sends model records down the
//     same pool branch.
// `base_positions` holds the decoded stream verbatim, so it is already bind-model. Feeding it through
// here would apply a spurious inverse-of-the-inverse-bind and displace the point by the node's bind
// offset.
//
// Why that matters even though this is dead code: its intended consumer is seam matching, which pairs
// edges by comparing reconstructed endpoints. Displacing one side's endpoints makes the keys miss, so
// seams silently fail to pair — presenting as "stitching still doesn't work" rather than as a
// coordinate-space error. Kept (not deleted) because the arithmetic is a correct inversion and would be
// the right primitive if a node-local stream ever appears; but the restoration should pass
// `base_positions` straight through instead.
static void stencil_shadow_bind_position(
	const render_model_node* node, const real_point3d* local, real_point3d* out_position)
{
	const real_matrix4x3* inverse = &node->default_inverse_matrix;
	real32 inverse_scale = inverse->scale != 0.f ? 1.f / inverse->scale : 1.f;
	real32 rx = local->x - inverse->position.x;
	real32 ry = local->y - inverse->position.y;
	real32 rz = local->z - inverse->position.z;
	const real_vector3d* f = &inverse->vectors.forward;
	const real_vector3d* l = &inverse->vectors.left;
	const real_vector3d* u = &inverse->vectors.up;
	out_position->x = (f->i * rx + f->j * ry + f->k * rz) * inverse_scale;
	out_position->y = (l->i * rx + l->j * ry + l->k * rz) * inverse_scale;
	out_position->z = (u->i * rx + u->j * ry + u->k * rz) * inverse_scale;
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

// Build the model's cross-quad list once all casting sections are cached: boundary edges
// whose bind-pose endpoints coincide across two sections are retagged matched (skipped by
// the per-section walk) and bridged by ONE owner-side cross quad.
static s_stencil_shadow_model_cross* stencil_shadow_model_cross_get(
	datum render_model_index, const render_model_definition* render_model)
{
	uint32 key = (uint32)render_model_index & 0xFFFF;
	auto found = g_stencil_shadow_cross_cache.find(key);
	if (found != g_stencil_shadow_cross_cache.end())
	{
		return &found->second;
	}

	s_stencil_shadow_model_cross& cross = g_stencil_shadow_cross_cache[key];
	cross.built = true;

	// STITCHING DISABLED (2026-08-16): with sentinel orientation fixed, every section
	// closes itself independently — correct counting without bridges. The one-sided
	// bridge left the PARTNER section's volume open whenever dominant-node skinning
	// opened a seam crack (unpaired wedge sheets fanning from bipeds — user-verified in
	// the green visualizer). td could stitch because its full-weight skinning keeps seam
	// verts coincident; ours cannot until full-weight skinning lands. Sentinels stay
	// untagged; the empty list below makes the cross pass a no-op.
	return &cross;
}

// Draw prebuilt cross-quad indices from the owner section's VB with the volume z-fail
// states (lean twin of stencil_shadow_section_draw's tail; caps stay per-section —
// stitches are silhouette sheets only, exactly like td's).
static void stencil_shadow_draw_cross_indices(
	const s_stencil_shadow_section* shadow,
	const std::vector<uint16>& indices,
	const real_point3d* light_position,
	const real_matrix4x3* model_matrix,
	real32 extrusion_distance,
	real32 opacity)
{
	IDirect3DDevice9Ex* device = rasterizer_dx9_device_get_interface();
	if (!device || indices.empty() || !stencil_shadow_shaders_initialize(device))
	{
		return;
	}

	stencil_shadow_force_render_state(D3DRS_COLORWRITEENABLE, 0);
	stencil_shadow_force_render_state(D3DRS_ALPHABLENDENABLE, FALSE);
	// see the volume pass: alpha test precedes the stencil stage and must not be inherited
	stencil_shadow_force_render_state(D3DRS_ALPHATESTENABLE, FALSE);
	stencil_shadow_force_render_state(D3DRS_STENCILENABLE, TRUE);
	stencil_shadow_force_render_state(D3DRS_TWOSIDEDSTENCILMODE, TRUE);
	stencil_shadow_force_render_state(D3DRS_STENCILREF, 0);
	stencil_shadow_force_render_state(D3DRS_STENCILMASK, 0xFFFFFFFF);
	stencil_shadow_force_render_state(D3DRS_STENCILWRITEMASK, 0xFFFFFFFF);
	stencil_shadow_force_render_state(D3DRS_STENCILFUNC, D3DCMP_ALWAYS);
	stencil_shadow_force_render_state(D3DRS_STENCILPASS, D3DSTENCILOP_KEEP);
	stencil_shadow_force_render_state(D3DRS_STENCILZFAIL, D3DSTENCILOP_INCR);
	stencil_shadow_force_render_state(D3DRS_STENCILFAIL, D3DSTENCILOP_KEEP);
	stencil_shadow_force_render_state(D3DRS_CCW_STENCILFUNC, D3DCMP_ALWAYS);
	stencil_shadow_force_render_state(D3DRS_CCW_STENCILPASS, D3DSTENCILOP_KEEP);
	stencil_shadow_force_render_state(D3DRS_CCW_STENCILZFAIL, D3DSTENCILOP_DECR);
	stencil_shadow_force_render_state(D3DRS_CCW_STENCILFAIL, D3DSTENCILOP_KEEP);
	stencil_shadow_force_render_state(D3DRS_ZWRITEENABLE, FALSE);
	stencil_shadow_force_render_state(D3DRS_ZENABLE, TRUE);
	stencil_shadow_force_render_state(D3DRS_CULLMODE, D3DCULL_NONE);
	stencil_shadow_force_render_state(D3DRS_ZFUNC, D3DCMP_LESS);	// D14: must match the volume pass
	stencil_shadow_force_render_state(D3DRS_SCISSORTESTENABLE, FALSE);
	stencil_shadow_force_render_state(D3DRS_CLIPPLANEENABLE, 0);
	stencil_shadow_force_render_state(D3DRS_FILLMODE, D3DFILL_SOLID);
	// DRIFT FIX (it. 299). These two were missing here while the volume pass sets both. This
	// function is the "lean twin" of stencil_shadow_section_draw's tail and is currently DEAD
	// (stitching disabled), so every state fix since the disable had to be mirrored by hand and
	// nothing would catch an omission — this is one that was missed.
	//
	// It matters for exactly the reason the volume pass sets them (it. 163): these quads bridge
	// seams BETWEEN sections, so their depth comparisons must agree with the volume faces they
	// close against. Vista uses depth bias for decals, so an inherited bias here but not there
	// would make a seam z-fail at a different depth than its neighbours and stop the counts
	// cancelling — light leaking along that seam only.
	stencil_shadow_force_render_state(D3DRS_DEPTHBIAS, 0);
	stencil_shadow_force_render_state(D3DRS_SLOPESCALEDEPTHBIAS, 0);
	// Depth bias must match the volume pass, which zeroes both. These quads bridge seams
	// BETWEEN sections, so their depth comparisons have to agree with the volume faces they
	// close against; a bias inherited here but not there (Vista uses bias for decals) would
	// make the seam z-fail at a different depth than its neighbours and stop the counts
	// cancelling -- artifacts precisely at section boundaries, which is where these draw.
	stencil_shadow_force_render_state(D3DRS_DEPTHBIAS, 0);
	stencil_shadow_force_render_state(D3DRS_SLOPESCALEDEPTHBIAS, 0);

	stencil_shadow_set_node_constants(model_matrix);
	real32 light_constant[4] = { light_position->x, light_position->y, light_position->z, 0.f };
	real32 extrusion_constant[4] = { extrusion_distance, 0.f, 0.f, 0.f };
	device->SetVertexShaderConstantF(k_stencil_shadow_light_constant, light_constant, 1);
	device->SetVertexShaderConstantF(k_stencil_shadow_extrusion_distance_constant, extrusion_constant, 1);

	uint32 index_count = (uint32)indices.size();
	if (index_count > k_stencil_shadow_index_buffer_capacity)
	{
		// Reachable in principle: the plane cap admits 32767 triangles, and a mesh with no
		// shared edges yields up to 3T edges -> 3 * 32767 * 6 indices, which exceeds the
		// buffer. Closed meshes give ~1.5T and stay well under, so this should never fire --
		// but truncating silently draws a PARTIAL volume that reads as a correctness bug
		// rather than a capacity one. Say it once.
		if (!g_stencil_shadow_warned_index_overflow_cross)
		{
			g_stencil_shadow_warned_index_overflow_cross = true;
			LOG_INFO_GAME("stencil WARNING: index buffer overflow — {} indices truncated to {} (partial volume)",
				index_count, (uint32)k_stencil_shadow_index_buffer_capacity);
		}
		index_count = k_stencil_shadow_index_buffer_capacity;
	}
	void* ib_data = NULL;
	if (FAILED(g_stencil_shadow_index_buffer->Lock(0, index_count * sizeof(uint16), &ib_data, D3DLOCK_DISCARD)))
	{
		return;
	}
	memcpy(ib_data, indices.data(), index_count * sizeof(uint16));
	g_stencil_shadow_index_buffer->Unlock();

	device->SetVertexDeclaration(g_stencil_shadow_vertex_declaration);
	bool stipple = opacity < 0.995f
		&& g_stencil_shadow_vertex_shader_sm3 && g_stencil_shadow_stipple_shader;
	device->SetVertexShader(stipple
		? g_stencil_shadow_vertex_shader_sm3 : g_stencil_shadow_vertex_shader);
	if (stipple)
	{
		const real32 stipple_constant[4] = { opacity, 0.f, 0.f, 0.f };
		device->SetPixelShader(g_stencil_shadow_stipple_shader);
		device->SetPixelShaderConstantF(30, stipple_constant, 1);
	}
	else
	{
		device->SetPixelShader(NULL);
	}
	device->SetStreamSource(0, shadow->shadow_vb, 0, sizeof(s_stencil_shadow_vertex));
	device->SetIndices(g_stencil_shadow_index_buffer);
	// The HRESULT was DISCARDED here while the volume draw checks and logs its own (it. 359). Same
	// omission class as the D3DRS_DEPTHBIAS drift found in it. 299/300: this is the "lean twin" of the
	// volume draw, every fix to which has had to be mirrored across by hand, and the error check was
	// never mirrored. Dead today because stitching is disabled -- but restoring stitching is a planned
	// multi-part change (td-seam-stitching-precondition.md), and a failing seam draw would then
	// contribute nothing while the volume draw's log stayed clean. That is the hardest shape to debug.
	//
	// D3D9 also fails a draw when the vertex declaration does not satisfy a declared shader input, so
	// this doubles as the runtime net for a declaration/shader TYPE mismatch -- the one part of the
	// vertex layout that offsetof and the static_asserts cannot enforce (see entry 11 in
	// td-do-not-fix.md).
	HRESULT cross_draw_result = device->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, 0,
		shadow->welded_vertex_count * 2, 0, index_count / 3);
	if (FAILED(cross_draw_result) && !g_stencil_shadow_warned_cross_draw_failed)
	{
		g_stencil_shadow_warned_cross_draw_failed = true;
		LOG_INFO_GAME("stencil CROSS draw FAILED: hr={:#x} indices={} verts={}",
			(uint32)cross_draw_result, index_count, shadow->welded_vertex_count * 2);
	}
	stencil_shadow_release_pipeline(device);
}

void stencil_shadow_cache_clear(void)
{
	for (auto& entry : g_stencil_shadow_cache)
	{
		stencil_shadow_section_destroy(&entry.second);
	}
	g_stencil_shadow_cache.clear();
	g_stencil_shadow_cross_cache.clear();

	// Refresh the build-time diagnostic budgets so each map gets its own samples (it. 310).
	// Without this they were process-lifetime caps and only the first map loaded was ever
	// described — see the "How to run it" note in td-INDEX.md.
	g_stencil_shadow_logged_point_data = 0;
	memset(g_stencil_shadow_logged_classification, 0, sizeof(g_stencil_shadow_logged_classification));
	g_stencil_shadow_warned_degenerate_weld = false;
	g_stencil_shadow_warned_no_apply = false;
	g_stencil_shadow_warned_no_bind = false;
	g_stencil_shadow_warned_no_definition = false;
	g_stencil_shadow_warned_no_section_data = false;
	g_stencil_shadow_warned_no_static_bind = false;
	g_stencil_shadow_warned_no_bounds = false;
	g_stencil_shadow_warned_shadows_off = false;
	g_stencil_shadow_warned_cross_draw_failed = false;
	g_stencil_shadow_warned_section_bounds = false;
	g_stencil_shadow_logged_authored_weld = false;
	g_stencil_shadow_warned_no_shadow_parts = false;
	g_stencil_shadow_warned_unwelded = false;
	g_stencil_shadow_warned_plane_cap = false;
	g_stencil_shadow_warned_index_overflow_volume = false;
	g_stencil_shadow_warned_index_overflow_cross = false;
	g_stencil_shadow_warned_cross_cap = false;
	g_stencil_shadow_warned_caster_cap = false;
	g_stencil_shadow_warned_no_node_map = false;
	g_stencil_shadow_warned_bone_no_map = false;
	g_stencil_shadow_warned_normalized_no_bounds = false;
	g_stencil_shadow_probed_interpolation = false;
	g_stencil_shadow_interp_samples = 0;
	g_stencil_shadow_interp_ran = 0;
	g_stencil_shadow_interp_max_pos = 0.f;
	g_stencil_shadow_interp_max_fwd = 0.f;
	g_stencil_shadow_skipped_class_mask = 0;
	g_stencil_shadow_skipped_class_count = 0;
	g_stencil_shadow_logged_lod_models = 0;
	g_stencil_shadow_logged_manifold_models = 0;
	g_stencil_shadow_warned_bad_strip = false;
}

/* P1 debug visualization: F8 toggles drawing the test volume in front of the camera
   (F10/F9/F2/F1 are taken by other parts of the project) */

// UI-phase hook: key handling ONLY — drawing happens in the render-layer hook where the
// scene depth-stencil is still bound (the UI phase has no functional stencil, verified).
void stencil_shadow_debug_update(void)
{
	static bool key_was_down = false;
	bool key_down = (GetAsyncKeyState(VK_F8) & 0x8000) != 0;
	if (key_down && !key_was_down)
	{
		g_stencil_shadow_active = !g_stencil_shadow_active;
		LOG_INFO_GAME("stencil shadows: active={} mode={}", g_stencil_shadow_active, g_stencil_shadow_draw_mode);
	}
	key_was_down = key_down;

	// F7 cycles draw modes
	static bool probe_key_was_down = false;
	bool probe_key_down = (GetAsyncKeyState(VK_F7) & 0x8000) != 0;
	if (probe_key_down && !probe_key_was_down)
	{
		g_stencil_shadow_draw_mode = (g_stencil_shadow_draw_mode + 1) % 3;
		LOG_INFO_GAME("stencil shadows: active={} mode={} (0=real 1=red 2=green)",
			g_stencil_shadow_active, g_stencil_shadow_draw_mode);
	}
	probe_key_was_down = probe_key_down;

	// F6 cycles extrusion distance (diagnostic)
	static bool extrude_key_was_down = false;
	bool extrude_key_down = (GetAsyncKeyState(VK_F6) & 0x8000) != 0;
	if (extrude_key_down && !extrude_key_was_down)
	{
		if (g_stencil_shadow_extrusion_override == 0.f)
		{
			g_stencil_shadow_extrusion_override = 500.f;
		}
		else if (g_stencil_shadow_extrusion_override == 500.f)
		{
			g_stencil_shadow_extrusion_override = 5.f;
		}
		else
		{
			g_stencil_shadow_extrusion_override = 0.f;
		}
		LOG_INFO_GAME("stencil shadows: extrusion={}",
			g_stencil_shadow_extrusion_override == 0.f
				? k_stencil_shadow_extrusion_distance : g_stencil_shadow_extrusion_override);
	}
	extrude_key_was_down = extrude_key_down;
}

// P2-1 — td's s_model_level_of_detail, the hlmt block that follows the tag's five leading
// tag references (render_model, collision_model, animation_graph, physics, physics_model),
// i.e. hlmt+0x28. Field offsets are td's render_lod_compute_model_alpha (td 0xD5900) rebased
// to the tag: its lod+0 is hlmt+0x28, lod+12 is the reduce-to-LOD distance array, lod+36 the
// shadow-fade LOD selector. Reads are VALIDATED before use (see below) because these offsets
// are reconstructed rather than taken from a Vista symbol.
struct s_model_level_of_detail
{
	real32 disappear_distance;			// td lod+0  -- model fade end (<= 0 disables model fade)
	real32 begin_fade_distance;			// td lod+4  -- model fade start
	int32 pad;							// td lod+8
	real32 reduce_to_lod_distance[5];	// td lod+12 -- l1 super low .. l5 super high
	int32 pad_1;						// td lod+32
	uint16 shadow_fade_lod_index;		// td lod+36 -- which reduce_to distance gates shadows
	uint16 pad_2;
};

// Reach used when a model's LOD block does not validate -- the behaviour this replaced.
static const real32 k_stencil_shadow_fallback_cull_distance = 70.f;
// td's fade band past the cutoff (render_lod_compute_model_alpha: 1 - (d - cutoff) * 0.1).
static const real32 k_stencil_shadow_fade_band = 10.f;

// td's shadow fade (render_lod_compute_model_alpha, td 0xD5900), shadow half only:
//   cutoff = reduce_to_lod_distance[min(shadow_fade_lod_index, 4)]
//   d <= cutoff                  -> 1.0
//   cutoff < d < cutoff + 10     -> clamp01(1 - (d - cutoff) * 0.1)
//   d >= cutoff + 10             -> 0.0   (object casts nothing)
// (td then takes min(model_alpha, shadow_alpha); model_alpha is the object's own fade-out,
// which Vista already applies to the drawn model, so the shadow term is what we want here.)
//
// Returns false when the block does not look like an LOD block, so the caller can fall back
// to a fixed reach instead of trusting a bad read.
static bool stencil_shadow_compute_shadow_alpha(
	const tag_reference* model_definition,
	real32 distance,
	real32* out_alpha)
{
	*out_alpha = 1.f;
	if (!model_definition)
	{
		return false;
	}
	const s_model_level_of_detail* lod =
		(const s_model_level_of_detail*)((const uint8*)model_definition + 0x28);

	// Validation. The offset itself is settled: td takes this block at hlmt+0x50, but that is
	// the TAG-BUILD layout where tag_reference is 16 bytes (5 refs = 0x50); Vista's cache
	// layout uses 8-byte references, so the same block sits at 5 * 8 = 0x28.
	//
	// Keep the checks to what actually catches a bad pointer. td itself validates NOTHING here
	// -- it just clamps the selector and indexes -- so anything stricter risks rejecting valid
	// data. An earlier version required the five distances to ascend and rejected 72 of 73
	// objects: "reduce to l1 (super low)" is the level used FARTHEST away, so the array runs
	// the other way, and unused slots are 0 besides.
	int32 fade_index = lod->shadow_fade_lod_index;
	if (fade_index > 4)
	{
		fade_index = 4;		// td: min(shadow_fade_lod_index, 4)
	}
	for (int32 i = 0; i < 5; i++)
	{
		real32 value = lod->reduce_to_lod_distance[i];
		if (!(value >= 0.f) || value > 100000.f)
		{
			return false;	// NaN, negative or absurd -> not an LOD block
		}
	}
	real32 cutoff = lod->reduce_to_lod_distance[fade_index];
	if (cutoff <= 0.f)
	{
		return false;	// model declares no shadow reach at this level
	}

	// one-shot per-model dump so the values can be eyeballed against the tag
	{
		if (g_stencil_shadow_logged_lod_models < 6)
		{
			g_stencil_shadow_logged_lod_models++;
			LOG_INFO_GAME("stencil lod: fade_index={} cutoff={:.2f} l=[{:.1f} {:.1f} {:.1f} {:.1f} {:.1f}] disappear={:.1f} begin_fade={:.1f}",
				lod->shadow_fade_lod_index, cutoff,
				lod->reduce_to_lod_distance[0], lod->reduce_to_lod_distance[1],
				lod->reduce_to_lod_distance[2], lod->reduce_to_lod_distance[3],
				lod->reduce_to_lod_distance[4],
				lod->disappear_distance, lod->begin_fade_distance);
		}
	}

	// --- shadow alpha: the 10wu fade band past the model's shadow cutoff ---
	real32 shadow_alpha = 1.f;
	if (distance >= cutoff + k_stencil_shadow_fade_band)
	{
		shadow_alpha = 0.f;
	}
	else if (distance > cutoff)
	{
		real32 alpha = 1.f - (distance - cutoff) / k_stencil_shadow_fade_band;
		shadow_alpha = alpha < 0.f ? 0.f : (alpha > 1.f ? 1.f : alpha);
	}

	// --- I2: model alpha, and td's combination of the two ---
	// render_lod_compute_model_alpha (td 0xD5900) computes BOTH alphas and finishes with
	//     *out_model_alpha  = model_alpha;
	//     *out_shadow_alpha = min(model_alpha, shadow_alpha);
	// so an object that is itself fading out drags its shadow down with it. We previously
	// implemented only the shadow half, leaving distant/fading objects casting a full-strength
	// shadow after the object had begun to disappear.
	//     if (lod->disappear_distance > 0) {
	//         if (distance < disappear) {
	//             if (distance > begin_fade)
	//                 model_alpha = 1 - (distance - begin_fade)/(disappear - begin_fade);
	//         } else model_alpha = 0;
	//     }
	real32 model_alpha = 1.f;
	if (lod->disappear_distance > 0.f)
	{
		if (distance < lod->disappear_distance)
		{
			if (distance > lod->begin_fade_distance)
			{
				real32 span = lod->disappear_distance - lod->begin_fade_distance;
				if (span > 0.f)
				{
					model_alpha = 1.f - (distance - lod->begin_fade_distance) / span;
					if (model_alpha < 0.f) model_alpha = 0.f;
					if (model_alpha > 1.f) model_alpha = 1.f;
				}
			}
		}
		else
		{
			model_alpha = 0.f;
		}
	}

	*out_alpha = shadow_alpha < model_alpha ? shadow_alpha : model_alpha;
	return true;
}

// P1-2 — td's authoritative "may this model cast a stencil shadow" gate.
// rasterizer_model_compute_fake_lighting (td 0x1F4060) sets the pass-6 draw flag only when
// render_model_check_shadow_manifold (td 0x1869F0) passes, and rasterizer_model_draw refuses
// to draw the model in pass 6 without it. That check walks EVERY pair of the object's active
// sections at ALL SIX LODs and tests bit `min + max*(max-1)/2` of the render_model's
// invalid_section_pair_bits; a single set bit disqualifies the object entirely. This is
// tag-authored data that Vista caches keep, so it replaces guessing at manifoldness.
//
// td's gate is `object+104 <= 1 && render_model_check_shadow_manifold(...)`. The first half is
// NOT omitted -- object+104 is e_object_type (proved from object_get_and_verify_type, td
// 0x85170), so `type <= 1` is _object_mask_unit, and that half is applied at the iterator via
// k_stencil_shadow_caster_mask. This function is the second half only.
static bool stencil_shadow_model_is_manifold(
	const render_model_definition* model,
	datum object_index)
{
	// Is the gate even live? The struct field survives in Vista's render_model_definition, but
	// that is not proof the CACHE BUILD populates it -- isq/dsq are stripped exactly this way.
	// Log the first few models' block sizes so a permanently-zero count is visible rather than
	// silently reading as "every model is manifold".
	{
		if (model && g_stencil_shadow_logged_manifold_models < 8)
		{
			g_stencil_shadow_logged_manifold_models++;
			LOG_INFO_GAME("stencil manifold: regions={} sections={} invalid_pair_words={}",
				model->regions.count, model->sections.count,
				model->invalid_section_pair_bits.count);
		}
	}

	if (!model || model->invalid_section_pair_bits.count <= 0 || model->regions.count <= 1)
	{
		return true;	// nothing marked, or nothing to pair
	}

	int32 region_count = 0;
	int8* region_permutation_indices = NULL;
	object_get_region_information(object_index, &region_count, &region_permutation_indices, NULL, NULL);

	const int32 bit_word_count = model->invalid_section_pair_bits.count;

	for (int32 lod = 0; lod < 6; lod++)
	{
		for (int32 region_a = 0; region_a < model->regions.count; region_a++)
		{
			const render_model_region* a = model->regions[region_a];
			if (a->permutations.count <= 0)
			{
				continue;
			}
			int32 permutation_a = (region_permutation_indices && region_a < region_count)
				? region_permutation_indices[region_a] : 0;
			// td's clamp, verbatim from rasterizer_model_compute_region_section_indices
			// (td 0x1F4200): an out-of-range POSITIVE index clamps to the LAST permutation;
			// only a NEGATIVE one falls back to 0.
			//
			// This has to match the draw path below exactly. It previously reset any
			// out-of-range index to 0, so for an object whose permutation index exceeded a
			// region's permutation count the gate evaluated a DIFFERENT section pair than the
			// one actually drawn -- validating geometry that never reaches the volume pass.
			if (permutation_a >= 0)
			{
				if (permutation_a > a->permutations.count - 1)
				{
					permutation_a = a->permutations.count - 1;
				}
			}
			else
			{
				permutation_a = 0;
			}
			int16 section_a = (&a->permutations[permutation_a]->l1_section_index)[lod];
			if (section_a == NONE)
			{
				continue;	// this permutation has no geometry at this LOD -> cannot pair
			}

			for (int32 region_b = region_a + 1; region_b < model->regions.count; region_b++)
			{
				const render_model_region* b = model->regions[region_b];
				if (b->permutations.count <= 0)
				{
					continue;
				}
				int32 permutation_b = (region_permutation_indices && region_b < region_count)
					? region_permutation_indices[region_b] : 0;
				// same td clamp as region_a above (td 0x1F4200)
				if (permutation_b >= 0)
				{
					if (permutation_b > b->permutations.count - 1)
					{
						permutation_b = b->permutations.count - 1;
					}
				}
				else
				{
					permutation_b = 0;
				}
				int16 section_b = (&b->permutations[permutation_b]->l1_section_index)[lod];
				if (section_b == NONE)
				{
					continue;
				}

				int32 low = section_a < section_b ? section_a : section_b;
				int32 high = section_a < section_b ? section_b : section_a;
				int32 bit = low + high * (high - 1) / 2;
				int32 word = bit >> 5;
				if (word >= 0 && word < bit_word_count
					&& (*model->invalid_section_pair_bits[word] & (1u << (bit & 31))) != 0)
				{
					return false;
				}
			}
		}
	}

	return true;
}

// Render-phase draw: called from Cartographer's NATIVE render_scene (render/render.cpp)
// right after render_lights_new() — the engine's per-frame shadow phase slot, scene render
// target and depth-stencil bound. (Engine-byte patching was a dead end: the native
// render_scene replaces the engine function, so its bytes never execute.)
void __cdecl stencil_shadow_render_layer_hook(void)
{
	static bool logged_first_fire = false;
	if (!logged_first_fire)
	{
		logged_first_fire = true;
		LOG_INFO_GAME("stencil shadows: render hook first fire");
	}

	if (!g_stencil_shadow_active || !cache_file_is_loaded())
	{
		return;
	}

	// Engine master shadow toggle. **Provenance re-verified in the binary (it. 352): the global at
	// VA 0x8E6948 has exactly four xrefs -- written by `rasterizer_settings_apply_settings` at both
	// 0x590B63 and 0x590C45, read by `render_dynamic_shadows` (0x5931F5) and `render_static_shadows`
	// (0x593904).** Nothing else touches it, so this is unambiguously the video-settings "Shadows"
	// option and not a neighbouring byte. The RVA below (0x4E6948 + 0x400000 base) resolves to it.
	// It is the VIDEO SETTINGS "Shadows" option -- written by
	// rasterizer_settings_apply_settings (halo2.exe 0x590B63 / 0x590C45) and read by both
	// engine shadow systems, render_dynamic_shadows (0x5931F5) and render_static_shadows
	// (0x593904). Its value in the image is 0; settings apply the real one at startup.
	// td's equivalent is byte_53A679, which gates every stencil shadow function there.
	//
	// Honour it, but say so once: a user with Shadows disabled would otherwise see our
	// volumes silently never appear and read that as a bug.
	const uint8* render_shadows = Memory::GetAddress<uint8*>(0x4E6948);
	if (render_shadows && *render_shadows == 0)
	{
		// File-scope and reset per map (it. 352), not a function-static. This is the message that
		// explains a TOTAL absence of shadows, and the one-shot diagnostics are known to land in a
		// rotated log (`h2mod.1.log` and friends -- see the log-rotation note in td-INDEX.md). A
		// process-lifetime latch means someone debugging "no shadows at all" on their third map finds
		// nothing in the current log and concludes the port is broken. Re-announcing per map costs one
		// line and removes that trap.
		if (!g_stencil_shadow_warned_shadows_off)
		{
			g_stencil_shadow_warned_shadows_off = true;
			LOG_INFO_GAME("stencil shadows: suppressed — engine 'render_shadows' video setting is OFF");
		}
		return;
	}

	// Called only from stencil_shadow_lightmap_volumes_pass (mid-scene, where c0-c3 is the
	// current window wvp). Two roles there: masking_pass == false lays the counts for the WORLD
	// term; masking_pass == true lays fullscreen counts for the model mask.
	//
	// it. 497 CORRECTED THIS COMMENT. It said the `masking_pass == false` role "runs the per-object
	// scissored darken cycles (world term, bounded + opacity-scaled)". That mechanism was REMOVED
	// for td parity — `stencil_shadow_compute_screen_rect` records it directly: "CURRENTLY
	// UNREFERENCED: its only caller was the per-object scissored darken, removed for td parity
	// (P1-1 -- td's application is one unscissored fullscreen quad)". The application is now a
	// single fullscreen multiply, as td's is.
	//
	// This mattered beyond tidiness: a per-object scissor would BOUND where a volume can darken,
	// which is exactly the kind of thing symptom 3 (a shadow leaking onto the ground below a ledge)
	// turns on. Reading the stale comment, one would look for the leak being clipped by a rect.
	// There is no rect.

	// P2: attach shadow volumes to real objects — iterate the object table, resolve each
	// object's render model (object def -> hlmt -> render model), build/cache its first
	// rigid shadow-casting section, and draw the volume at the object's node 0 matrix
	// under the object's own cached lighting (render_lighting.shadow_direction — the same
	// data Vista's blob shadows orient by), falling back to a fixed sun.
	static uint32 facing_bitvector[k_stencil_shadow_facing_bitvector_words];

	enum { k_max_debug_volumes_per_frame = 64 };
	int32 volumes_drawn = 0;

	// loop diagnostics (logged every ~300 active frames)
	int32 dbg_iterated = 0, dbg_far = 0, dbg_opacity = 0, dbg_no_model = 0;
	int32 dbg_no_rmodel = 0, dbg_no_matrix = 0, dbg_no_sections = 0, dbg_uncached = 0;
	int32 dbg_no_definition = 0;
	// How many casters take the CINEMATIC arm of render_object_cache_get_lighting (flags bit 24).
	// That accessor returns NULL for two unrelated reasons -- the cinematic arm (which we mirrored
	// as NULL until it. 216) and the lighting-valid byte at cache entry+3 -- so `nolighting` on its
	// own cannot say which one is dropping casters. This splits them:
	//   cinematic > 0                -> the it.216 fix is genuinely feeding casters
	//   cinematic == 0, nolighting>0 -> the validity byte is the cause and it.216 changed nothing
	int32 dbg_cinematic = 0;
	// P0-1 health signal: casters whose shadow_direction is shallower than the variants-path
	// clamp allows (normalized z > -0.6), and the shallowest z seen this pass. A stale read
	// shows up as values near or above 0 that jitter frame to frame; the lightmap-interpolated
	// producer can legitimately sit between -0.6 and 0, so read the value, not just the count.
	int32 dbg_shallow = 0;
	real32 dbg_shallowest_z = -1.f;
	// P1-2: models rejected by the invalid_section_pair_bits manifold gate
	int32 dbg_nonmanifold = 0;
	// objects skipped for being hidden or flagged shadowless
	int32 dbg_shadowless = 0;
	// P2-1: objects whose hlmt LOD block failed validation (td's fade did NOT run for them)
	int32 dbg_lodfail = 0;

	// td's caster set, now known exactly. rasterizer_model_compute_fake_lighting (td 0x1F4060)
	// gates the pass-6 draw flag on `*(uint16*)(object + 104) <= 1`, and
	// object_get_and_verify_type (td 0x85170) proves object+104 is e_object_type:
	//     v3 = *(int16*)(object + 104);
	//     if (((1 << v3) & type_mask) == 0) -> "got an object type we didn't expect"
	// With _object_type_biped = 0 and _object_type_vehicle = 1, `type <= 1` is precisely
	// _object_mask_unit. **tag debug casts stencil shadows from BIPEDS AND VEHICLES ONLY.**
	//
	// We previously also cast from items (weapon/equipment/garbage), projectiles, crates and
	// creatures — none of which td ever shadows. Crates especially are large objects, so this
	// was adding big shadows with no counterpart in the original.
	const int32 k_stencil_shadow_caster_mask = _object_mask_unit;
	c_object_iterator<object_datum> object_iterator;
	object_iterator.begin((e_object_type)k_stencil_shadow_caster_mask, 0);
	while (object_iterator.next() && volumes_drawn < k_max_debug_volumes_per_frame)
	{
		// dbg_iterated counts EVERY object the iterator yields, before any rejection, so the
		// `stencil dbg:` line's arithmetic closes:
		//     iterated == nodef + shadowless + nolighting + nomodel + far + opacity
		//                 + normodel + nonmanifold + nomatrix + nosec + drawn
		// It used to be incremented further down, AFTER the no-definition and shadowless guards,
		// so those two drops were invisible to the accounting and the total only balanced when both
		// happened to be zero — which is exactly what the 17/08 run was (iter=3 = drawn=1 +
		// nolighting=2). A non-zero `shadowless` would silently have broken the sum, and there is no
		// worse diagnostic than one whose arithmetic stops closing without saying so. (it. 328)
		//
		// NOTE `lodfail` is NOT a drop term — it is incremented and then execution continues (it may
		// or may not reach the `far` drop below), so it must be excluded from the sum.
		dbg_iterated++;
		const object_datum* object = object_iterator.get_datum();
		if (!object || object->definition_index == NONE)
		{
			// previously the only SILENT drop in the loop: an object with no definition appeared in
			// no counter at all, not even the iterated total
			dbg_no_definition++;
			continue;
		}
		// I1b — the parent skip is GONE. td has no such check anywhere in its shadow path;
		// it iterates every visible model and gates purely on shadow_alpha and the manifold
		// flag (which carries the `object_type <= 1` test). The skip existed only to stop the
		// first-person WEAPON casting from its camera-space node poses — and weapons are
		// object type 2, so the corrected caster mask (_object_mask_unit = biped|vehicle)
		// now excludes them at source. Keeping it would drop vehicle passengers, which td
		// does shadow.

		// td iterates render_lod_visible_models_iterate — models actually submitted for
		// rendering this frame. We walk the object table directly, so the engine's own
		// "don't draw / don't shadow" state has to be applied by hand:
		//   _object_hidden_bit      — hidden objects are never submitted, so never cast.
		//   _object_shadowless_bit  — object_set_shadowless (halo2.exe 0x4FDD65) sets flags
		//                             bit 0x10000 either from script or from the object
		//                             DEFINITION's own "does not cast shadow" tag flag
		//                             (definition+2 bit 0), so this one bit covers both.
		if (object->object.flags.test(_object_hidden_bit)
			|| object->object.flags.test(_object_shadowless_bit))
		{
			dbg_shadowless++;
			continue;
		}

		// camera distance drives td's shadow fade (applied below, once the model tag is
		// resolved); z-fail over closed volumes is camera-position-correct, so no near skip
		real32 camera_distance;
		{
			const s_render* render = render_get();
			real32 dx = object->object.position.x - render->camera.point.x;
			real32 dy = object->object.position.y - render->camera.point.y;
			real32 dz = object->object.position.z - render->camera.point.z;
			camera_distance = sqrtf(dx * dx + dy * dy + dz * dz);
		}

		// per-object light: cached render state's render_lighting.shadow_direction points
		// the way the shadow falls (our convention wants the vector TOWARD the light);
		// shadow_opacity scales the darkness (the tag-debug lightmap-shadows path built the
		// same kind of fake light from lightmap data).
		//
		// P0-1: the ONLY valid gate is the lighting-valid byte at cache entry+3, which
		// render_object_cache_get_lighting tests. The previous gate
		// (render_object_cache_get_render_state + is_object_cached) tested the rasterizer
		// GEOMETRY cache fields instead, so entries passed while entry+84 still held a
		// previous tenant's or uninitialised lighting -> arbitrary, often near-horizontal
		// shadow_direction -> grazing volumes (the oversized/streaking shadows). Both engines
		// hard-clamp this vector steep (Vista lighting_solve_from_variants 0x587C9C:
		// z <= -0.75; td build_distant_lights 0x107C30: |xy| <= 0.707), so a correct read
		// CANNOT produce a long streak.
		real_point3d toward_light_world = { 0.408f, 0.408f, 0.816f };
		real32 shadow_opacity = 1.f;
		if (object->object.flags.test(_object_uses_cinematic_lighting_bit))
		{
			dbg_cinematic++;
		}
		const render_lighting* lighting =
			render_object_cache_get_lighting(object_iterator.get_index());
		if (!lighting)
		{
			dbg_uncached++;
			continue;	// lighting not computed for this object -> no shadow
		}
		{
			const real_vector3d* direction = &lighting->shadow_direction;
			real32 length_squared = direction->i * direction->i
				+ direction->j * direction->j + direction->k * direction->k;
			if (length_squared > 0.001f)
			{
				toward_light_world.x = -direction->i;
				toward_light_world.y = -direction->j;
				toward_light_world.z = -direction->k;
				// P0-1 verification. Vista's lighting_solve_from_variants (0x587C9C) clamps
				// with `if (z > -0.75) { z = -0.75; normalize(); }` -- note the clamp sets z
				// on the UNNORMALIZED vector and renormalizes afterwards, so the guarantee on
				// the normalized result is NOT -0.75. src is a unit vector, so after forcing
				// z = -0.75 the length is sqrt(1 - z_orig^2 + 0.5625) <= 1.25, hence
				// normalized z <= -0.75/1.25 = -0.6, worst case at z_orig = 0.
				// (The other producer, interpolate_lighting_sample 0x58829F, does
				// `z *= 2; normalize()` and guarantees NOTHING -- a lightmap-sampled direction
				// can legitimately be shallow. So a non-zero count here is not proof of a bad
				// read; the shallowest value below is the real signal.)
				real32 inverse_length = 1.f / sqrtf(length_squared);
				real32 normalized_z = direction->k * inverse_length;
				if (normalized_z > -0.6f)
				{
					dbg_shallow++;
				}
				if (normalized_z > dbg_shallowest_z)
				{
					dbg_shallowest_z = normalized_z;
				}
				shadow_opacity = lighting->shadow_opacity;
				if (shadow_opacity < 0.f) shadow_opacity = 0.f;
				if (shadow_opacity > 1.f) shadow_opacity = 1.f;
			}
			// else: a zero-length shadow_direction means the blob was never populated -- e.g. an
			// object flagged for cinematic lighting in a map whose script never ran
			// cinematic_lighting_set_primary_light, which leaves the whole 84-byte render_lighting
			// at object_globals + 0x1C zeroed. The direction already falls back to the fixed sun
			// above; the OPACITY has to fall back with it, because reading 0.0 out of that same
			// dead blob yields a fully transparent shadow and silently undoes the fallback.
			// Trust the blob's fields together or not at all.
		}
		// (the opacity floor is applied after the td distance fade is folded in, below)

		// object definition -> hlmt -> render model (first tag_reference in hlmt)
		const _object_definition* object_definition =
			(const _object_definition*)tag_get_fast(object->definition_index);
		if (!object_definition || object_definition->model.index == NONE)
		{
			dbg_no_model++;
			continue;
		}
		const tag_reference* hlmt_render_model_reference =
			(const tag_reference*)tag_get_fast(object_definition->model.index);
		if (!hlmt_render_model_reference || hlmt_render_model_reference->index == NONE)
		{
			dbg_no_model++;
			continue;
		}
		datum render_model_index = hlmt_render_model_reference->index;

		// P2-1: td's distance fade. Past the model's own shadow reach the object casts
		// NOTHING (td: shadow_alpha 0 -> rasterizer_stencilshadow_shadows_model_section_draw
		// draws no primitives at all); inside the last 10wu it stipples out. Replaces the
		// arbitrary fixed cull -- but only when the model's LOD block validates; otherwise
		// the fixed reach still applies (see stencil_shadow_compute_shadow_alpha).
		real32 shadow_alpha = 1.f;
		if (stencil_shadow_compute_shadow_alpha(hlmt_render_model_reference, camera_distance, &shadow_alpha))
		{
			if (shadow_alpha <= 0.f)
			{
				dbg_far++;
				continue;
			}
		}
		else
		{
			// the model's LOD block did not validate -- we are on the old fixed reach, so
			// td's fade is NOT actually running for this object. Counted so a permanently
			// high lodfail is visible instead of silently reverting to the old behaviour.
			dbg_lodfail++;
			if (camera_distance > k_stencil_shadow_fallback_cull_distance)
			{
				dbg_far++;
				continue;
			}
		}
		// td: alpha = min(model shadow_alpha, global shadow density). Ours folds in the
		// object's own render_lighting.shadow_opacity as the third term.
		if (shadow_alpha < shadow_opacity)
		{
			shadow_opacity = shadow_alpha;
		}
		if (shadow_opacity < 0.05f)
		{
			dbg_opacity++;
			continue;
		}

		render_model_definition* render_model =
			(render_model_definition*)tag_get_fast(render_model_index);
		if (!render_model)
		{
			dbg_no_rmodel++;
			continue;
		}

		// P1-2: td's tag-data cast gate — models with a non-manifold section pair cast nothing
		if (!stencil_shadow_model_is_manifold(render_model, object_iterator.get_index()))
		{
			dbg_nonmanifold++;
			continue;
		}

		// FAITHFUL tag-debug semantics: a FINITE 2 world-unit extrusion (c[20].w, from
		// rasterizer_light_begin's 2.0f for the lightmap fake light -- see
		// k_stencil_shadow_extrusion_distance). The earlier "effectively infinite volumes,
		// long trails are the authentic look" reading was wrong: it came from noting that
		// rasterizer_light_submit uploads no separate distance constant, before the microcode
		// showed the distance is the .w of the light block it uploads.
		// F6 cycles alternative distances for diagnostics only.
		real32 extrusion_distance = g_stencil_shadow_extrusion_override != 0.f
			? g_stencil_shadow_extrusion_override : k_stencil_shadow_extrusion_distance;

		// per-section facing bits retained for the cross-quad (seam stitch) pass
		enum { k_max_cross_sections = 64 };
		static uint32 facing_scratch[k_max_cross_sections][k_stencil_shadow_facing_bitvector_words];
		s_stencil_shadow_section* cross_shadows[k_max_cross_sections] = {};
		const real_matrix4x3* cross_matrices[k_max_cross_sections] = {};
		// Backing store for composed static transforms published into cross_matrices. The composed
		// matrix is built per loop iteration, so pointing cross_matrices at a loop local would
		// dangle by the time the cross pass reads it AFTER the loop. (The previous code was safe
		// only because `model_matrix` pointed into engine memory.)
		static real_matrix4x3 cross_matrix_storage[k_max_cross_sections];

		// td parity (rasterizer_model_draw + per-section mark bytes): only the ACTIVE
		// LOD's section per region draws. Iterating every section shadowed the UNION of
		// all LOD levels and all permutations — a hull larger than the rendered object
		// (the user's oversized-shadow report). v1 uses permutation 0 (base variant);
		// live damage-state permutation selection is a ledger item.
		// D11/D12 — section selection is a straight port of
		// rasterizer_model_compute_region_section_indices (td 0x1F4200):
		//     perm = object->region_permutation_indices[region]        (NOT always 0)
		//     perm = (perm >= 0) ? min(perm, region->permutations.count - 1) : 0
		//     section_index = permutations[perm]->lod_sections[level_of_detail]   (direct index)
		// td performs NO fallback search across LODs -- it asserts the entry is valid, because
		// the tool guarantees one exists at every level. Our previous code used permutation 0
		// unconditionally (wrong geometry whenever the object's variant wasn't 0) and, when the
		// chosen LOD held NONE, substituted a section from a *different* LOD -- drawing geometry
		// the engine never renders and widening the silhouette at every extrusion distance.
		int32 region_count = 0;
		int8* region_permutation_indices = NULL;
		object_get_region_information(object_iterator.get_index(), &region_count,
			&region_permutation_indices, NULL, NULL);

		int8 object_lod = render_object_cache_get_level_of_detail(object_iterator.get_index());
		int32 sections_drawn = 0;
		for (int32 region_index = 0; region_index < render_model->regions.count; region_index++)
		{
			const render_model_region* region = render_model->regions[region_index];
			if (region->permutations.count <= 0)
			{
				continue;
			}
			int32 permutation_index = (region_permutation_indices && region_index < region_count)
				? region_permutation_indices[region_index] : 0;
			if (permutation_index >= 0)
			{
				if (permutation_index > region->permutations.count - 1)
				{
					permutation_index = region->permutations.count - 1;
				}
			}
			else
			{
				permutation_index = 0;
			}
			const render_model_permutation* permutation = region->permutations[permutation_index];
			const int16* lod_sections = &permutation->l1_section_index;
			// td indexes info->level_of_detail directly; that is the LOD the model is actually
			// rendering at. A NONE there means this region draws nothing at this level -- honour
			// it instead of hunting for a substitute.
			int16 section_index;
			if (object_lod >= 0 && object_lod < 6)
			{
				section_index = lod_sections[object_lod];
			}
			else
			{
				// no cached LOD for this window (the accessor yields NONE when the bound window
				// index is >= 4). td always has one; we don't, so fall back to the LOWEST detail
				// level that exists -- shadows do not need detail, and this cannot pick up more
				// geometry than the object renders.
				section_index = NONE;
				for (int32 level = 0; level < 6 && section_index == NONE; level++)
				{
					section_index = lod_sections[level];
				}
			}
			if (section_index == NONE || section_index < 0
				|| section_index >= render_model->sections.count)
			{
				continue;
			}
			const render_model_section* section = render_model->sections[section_index];
			// D13 — td skips a section unless total_vertex/triangle/part counts are all non-zero
			// (rasterizer_model_section_draw, td 0x10F0E0).
			if (section->section_info.total_vertex_count == 0
				|| section->section_info.total_triangle_count == 0
				|| section->section_info.total_part_count == 0)
			{
				continue;
			}
			// rigid sections ride node 0; uniform rigid_boned sections ride their own node;
			// mixed-node and skinned sections go through the P3 articulated path (CPU
			// skinning + per-frame soft planes, td parity)
			if (section->section_info.shadow_casting_triangle_count == 0)
			{
				continue;
			}
			// it. 477 — SPLIT FROM THE LINE ABOVE PURELY TO MAKE THIS CASE OBSERVABLE. Both still
			// `continue`; behaviour is unchanged.
			//
			// it. 188 recorded that td REMAPS classification 4 (_unsupported_reimport) to 3
			// (_skinned) in `render_model_section_geometry_postprocess` (td 0xF58D0):
			//     if (global_geometry_classification == 4) section_info.geometry_classification = 3;
			// so td BUILDS AND CASTS from such a section via the skinned path, while we drop it.
			// Failing safe (no shadow rather than a wrong one) is the right default, but it is a
			// known divergence, and whether it costs anything depends on whether Vista caches
			// actually contain class-4 sections.
			//
			// it. 188's plan for answering that was "the existing `stencil build: class=` log
			// reports the value, so a run answers it". **That plan cannot work** — this `continue`
			// fires BEFORE any build, so a skipped section never reaches the `stencil build:` line
			// and is invisible in the log. The run would have shown no class-4 sections whether or
			// not any existed. Same fault as it. 471-475: a question routed to an instrument that
			// structurally cannot see the case.
			//
			// Latched per classification value, so output is bounded (there are <8 classes) while
			// still reporting every distinct class that gets dropped; the running count gives the
			// magnitude. If this never fires, the divergence is inert on this content and it. 188
			// closes. If it does, we are losing shadows td would draw.
			if (section->global_geometry_classification > _geometry_classification_skinned)
			{
				const int32 dropped_class = (int32)section->global_geometry_classification;
				g_stencil_shadow_skipped_class_count++;
				const uint32 class_bit = 1u << ((uint32)dropped_class & 31);
				if ((g_stencil_shadow_skipped_class_mask & class_bit) == 0)
				{
					g_stencil_shadow_skipped_class_mask |= class_bit;
					LOG_INFO_GAME("stencil shadows: section {} SKIPPED for classification {} (>{}) — td remaps class 4 to skinned and CASTS from it (it. 188/477); count so far {}",
						section_index, dropped_class,
						(int32)_geometry_classification_skinned,
						g_stencil_shadow_skipped_class_count);
				}
				continue;
			}
			// rigid_node is applied on the SENTINEL, with no classification test -- this is
			// td's rule verbatim (rasterizer_model_section_draw, td 0x10F0E0):
			//     v5 = section[22];              // section + 44 == rigid_node
			//     if (v5 != -1) { load node matrix into v12/v13/v14 }
			// The classification never enters into it. We previously gated on
			// classification == rigid_boned, which left every class-1 (rigid) section carrying a
			// non-zero rigid_node riding node 0 instead of its own -- a volume positioned by the
			// wrong bone, visible as a shadow detached from a turret, hatch or other
			// non-root-mounted part. Testing the field rather than the classification is also
			// robust to a section whose two disagree.
			int16 section_node = 0;
			if (section->rigid_node != NONE)
			{
				section_node = section->rigid_node;
			}
			s_stencil_shadow_section* shadow = stencil_shadow_section_get(render_model_index, section_index);
			if (!shadow)
			{
				continue;
			}
			// The node matrix is only needed by the STATIC path -- articulated sections are
			// CPU-skinned to world and draw with NULL node constants, so a missing matrix must
			// not drop them. (Previously this bailed before `shadow` was known, which was
			// over-strict for articulated sections and became slightly more so once section_node
			// started following rigid_node rather than defaulting to 0.)
			const real_matrix4x3* model_matrix =
				object_get_node_matrix(object_iterator.get_index(), section_node);
			if (!model_matrix && !shadow->articulated)
			{
				dbg_no_matrix++;
				continue;
			}
			if (shadow->articulated)
			{
				// P3: CPU-skin into world space, then everything is world-space — planes,
				// facing light, VB positions — drawn with identity node constants
				if (!stencil_shadow_section_animate(shadow, object_iterator.get_index(), render_model))
				{
					continue;
				}
				stencil_shadow_build_facing_bitvector(shadow, &toward_light_world, false, facing_bitvector);
				stencil_shadow_section_draw(shadow, facing_bitvector, &toward_light_world, false,
					NULL, extrusion_distance, shadow_opacity);
				if (section_index < k_max_cross_sections)
				{
					memcpy(facing_scratch[section_index], facing_bitvector,
						((shadow->plane_count + 31) / 32) * sizeof(uint32));
					cross_shadows[section_index] = shadow;
					cross_matrices[section_index] = NULL;
				}
			}
			else
			{
				// STATIC-PATH INVERSE-BIND PROBE (it. 334) — diagnostic only, changes nothing.
				//
				// We bind a RAW node matrix to c50-c52 here. The engine never does that for a model
				// section: `render_visible_section_set_transform_constants` (halo2.exe 0x680A68)
				// takes its transform from the SKINNING POOL for model records, and every pool entry
				// is `node_world x inverse_bind` (0x77DD88). Branch B is provably the model path — it
				// passes `section->model_tag_index` to `render_model_get_section_rigid_node`, and only
				// MODEL records carry a valid one (clusters/instanced hold -1). So static sections may
				// be missing the same term fixed for the articulated path in it. 317.
				//
				// Before changing the transform (which would affect EVERY static caster), measure the
				// size of the term. `default_inverse_matrix.position` is the displacement the missing
				// composition would introduce; compared against the section's own bind extent it says
				// directly whether the volume is grossly misplaced or whether this is a no-op:
				//   ratio << 1  -> the node sits near the model origin; composition barely matters
				//   ratio ~ 1+  -> the volume is displaced by about its own size; this IS symptom 1
				// Measuring first is deliberate: the it. 314 error came from acting on exactly this
				// kind of inference without measuring.
				if (section_node < render_model->nodes.count)
				{
					static uint32 bindprobe_log = 0;
					if ((bindprobe_log++ % 600) == 0)
					{
						const real_matrix4x3* bind_inverse =
							&render_model->nodes[section_node]->default_inverse_matrix;
						real32 bind_offset = (real32)sqrt((real64)(
							bind_inverse->position.x * bind_inverse->position.x
							+ bind_inverse->position.y * bind_inverse->position.y
							+ bind_inverse->position.z * bind_inverse->position.z));
						real32 extent = 0.f;
						for (uint32 i = 0; i < shadow->welded_vertex_count; i++)
						{
							const real_point3d* b = &shadow->base_positions[i];
							real32 d = b->x * b->x + b->y * b->y + b->z * b->z;
							if (d > extent) { extent = d; }
						}
						extent = (real32)sqrt((real64)extent);
						// POSITION MAGNITUDE IS ONLY HALF THE TERM (corrected it. 372). A live read of
						// real tag data (it. 371) found a root node with `default_translation = (0,0,0)`
						// and `default_inverse_matrix.position = (0,0,0)` whose BASIS is a ~126 degree
						// z-rotation. Composing that changes the transform completely, yet the
						// `bind_offset / extent` ratio alone reports ~0 and reads as "no-op" -- exactly
						// the misleading-baseline failure recorded in it. 331, in a diagnostic added to
						// avoid it.
						//
						// So also report how far the basis is from identity. For a rotation matrix
						// trace = 1 + 2*cos(theta), hence theta = acos((trace - 1) / 2): 0 degrees means
						// the composition is a pure translation (or nothing), anything else means it
						// rotates the volume regardless of what the offset ratio says.
						real32 trace = bind_inverse->vectors.forward.i
							+ bind_inverse->vectors.left.j + bind_inverse->vectors.up.k;
						real32 cos_theta = (trace - 1.f) * 0.5f;
						if (cos_theta > 1.f) { cos_theta = 1.f; }
						if (cos_theta < -1.f) { cos_theta = -1.f; }
						real32 bind_rotation_deg =
							(real32)(acos((real64)cos_theta) * (180.0 / 3.14159265358979323846));
						// EITHER number being significant means the it. 335 composition matters:
						// `ratio` near/above 1 = the volume was displaced; `bind_rot` non-zero = it was
						// mis-ORIENTED. Only BOTH being ~0 means the fix is a no-op for this section.
						LOG_INFO_GAME("stencil bindprobe: static section={} node={} bind_offset={:.3f}wu extent={:.3f}wu ratio={:.2f} bind_rot={:.1f}deg inv_scale={:.3f} class={} (ratio>=1 = displaced; bind_rot>0 = mis-oriented; BOTH ~0 = fix is a no-op here)",
							section_index, (int32)section_node, bind_offset, extent,
							extent > 0.0001f ? bind_offset / extent : -1.f,
							bind_rotation_deg,
							bind_inverse->scale,
							(int32)section->global_geometry_classification);
					}
				}
				// STATIC TRANSFORM = node_world x INVERSE BIND, as for the articulated path (it. 317).
				// Confirmed against BOTH references, independently:
				//
				// * td -- the mandated reference: `rasterizer_model_section_draw` (td 0x10F0E0) loads the
				//   rigid-node transform into v12-v14 from
				//   `model_skinning_get_node_matrix(skinning, section->rigid_node, 0)` -- the SKINNING
				//   POOL, not a raw node matrix. td's pool is built as
				//   `matrix4x3_multiply(object_node_matrix, render_model_node + 68, out)`, so the inverse
				//   bind is already baked in. td does this with NO classification check, for any section
				//   whose rigid_node != -1.
				// * Vista: `render_visible_section_set_transform_constants` (0x680A68) sends model records
				//   down the pool branch -- provably the model path, since it passes
				//   `section->model_tag_index` to `render_model_get_section_rigid_node` and only MODEL
				//   records carry a valid one. Same pool, same composition (0x77DD88).
				//
				// We previously bound a RAW `object_get_node_matrix`, which re-applies the node's bind
				// translation and displaces the volume by that node's offset from the model origin -- the
				// static twin of the starfish fixed in it. 317, and a candidate for the residual half of
				// symptom 1. `stencil bindprobe:` measures the magnitude and stays in place to confirm.
				//
				// The composed matrix feeds BOTH consumers: the facing test needs the light in the same
				// space as the plane data, so it must use the composed rotation, not the raw node rotation.
				real_matrix4x3 composed_static;
				const real_matrix4x3* draw_matrix = model_matrix;
				if (section_node >= 0 && (int32)section_node < render_model->nodes.count)	/* it. 443: `>= 0` guards a corrupt non-sentinel negative rigid_node (int16) from indexing nodes[-2] — an OOB READ feeding a matrix multiply, i.e. a plausible wrong transform, not a clean crash. -1 is already filtered upstream (section_node defaults to 0), so this is unreachable on valid data (it. 435 census: rigid_node is -1 or a real index) and is guarded only on the it. 347 precedent for user-modified maps. Routes corrupt data into the warned fallback below. */
				{
					matrix4x3_multiply(model_matrix,
						&render_model->nodes[section_node]->default_inverse_matrix, &composed_static);
					draw_matrix = &composed_static;
				}
				else
				{
					// Falling back to the RAW node matrix here reinstates exactly the it. 335 defect --
					// a volume displaced by the node's bind offset -- so it must not be silent. The
					// articulated path already announces its equivalent case (`no bind matrix for node`);
					// this branch was added in it. 335 without the matching log, which is the same
					// silent-fallback-to-known-wrong pattern this file has been purged of elsewhere.
					// (it. 346, self-review.)
					if (!g_stencil_shadow_warned_no_static_bind)
					{
						g_stencil_shadow_warned_no_static_bind = true;
						LOG_INFO_GAME("stencil WARNING: static section {} node {} >= nodes.count {} — no inverse bind, volume will be displaced",
							section_index, (int32)section_node, render_model->nodes.count);
					}
				}
				// static: facing test is model/section-space (plane data); the shader light
				// is WORLD-space (extrusion happens after the node transform)
				real_point3d toward_light_model;
				stencil_shadow_direction_to_model_space(draw_matrix, &toward_light_world, &toward_light_model);
				stencil_shadow_build_facing_bitvector(shadow, &toward_light_model, false, facing_bitvector);
				stencil_shadow_section_draw(shadow, facing_bitvector, &toward_light_world, false,
					draw_matrix, extrusion_distance, shadow_opacity);
				if (section_index < k_max_cross_sections)
				{
					memcpy(facing_scratch[section_index], facing_bitvector,
						((shadow->plane_count + 31) / 32) * sizeof(uint32));
					cross_shadows[section_index] = shadow;
					cross_matrix_storage[section_index] = *draw_matrix;
					cross_matrices[section_index] = &cross_matrix_storage[section_index];
				}
			}
			// Fourth silent cap (see td-INDEX.md). Sections at index >= k_max_cross_sections
			// still draw their own volume, but are excluded from facing_scratch/cross_shadows
			// and therefore from seam stitching -- so any seam touching such a section is left
			// unbridged. That LEAKS light through the seam rather than removing a shadow, which
			// is why it is worth reporting even though the volume itself is fine. td has no
			// equivalent limit.
			// WHY THIS FIRES IS NOT "TOO MANY SECTIONS DRAWN" (measured it. 439/440).
			//
			// These arrays are indexed by the MODEL's section index, which is sparse, while only
			// `regions.count` sections ever draw (one per region at the active LOD — the D11/D12
			// rule above). So the limit is reached with the array almost entirely empty:
			//
			//   vehicle @0x0476A0D0 : 161 sections but **9 regions** -> <= 9 drawn, indices spread
			//                         over 0-160, so a drawn section at index 100 is dropped while
			//                         55+ slots sit unused (the array is ~86% empty when it "overflows")
			//   vehicle @0x0476CA10 :  70 sections,  6 regions
			//
			// Four of the eight vehicles resolved in this cache exceed it (161 / 136 / 131 / 70
			// sections); the biped is fine at 20. So this is not exotic content.
			//
			// THE FIX IS A DENSE INDEX, NOT A BIGGER ARRAY: key these by a 0..N-1 counter of
			// sections actually drawn and 64 slots are ample for anything (biped <= 20, these
			// vehicles <= 9), at ZERO extra memory. Raising `k_max_cross_sections` — the obvious
			// reading of this message — does not address the cause and still overflows for any
			// model whose section index exceeds the new value.
			if (section_index >= k_max_cross_sections)
			{
				if (!g_stencil_shadow_warned_cross_cap)
				{
					g_stencil_shadow_warned_cross_cap = true;
					LOG_INFO_GAME("stencil WARNING: section INDEX {} is past the {}-slot seam-stitch array (only {} sections drew) — sparse indexing, not too many sections; its seams will not be bridged",
						section_index, (int32)k_max_cross_sections, sections_drawn + 1);
				}
			}
			sections_drawn++;
		}

		// SIZE DIAGNOSTIC: what the caster actually is, against what we extrude. If the drawn
		// shadow is much larger than radius + extrusion, the extra size is coming from
		// somewhere neither of those two numbers explains.
		{
			static uint32 size_log_frame = 0;
			if (sections_drawn > 0 && (size_log_frame++ % 600) == 0)
			{
				LOG_INFO_GAME("stencil size: radius={:.3f} extrusion={:.3f} sections={} light=({:.3f},{:.3f},{:.3f}) opacity={:.2f}",
					object->object.radius, extrusion_distance, sections_drawn,
					toward_light_world.x, toward_light_world.y, toward_light_world.z,
					shadow_opacity);
			}
		}

		if (sections_drawn > 0)
		{
			// bridge matched seams between this object's sections (td shared-edge stitches)
			s_stencil_shadow_model_cross* cross =
				stencil_shadow_model_cross_get(render_model_index, render_model);
			if (cross && !cross->quads.empty())
			{
				static std::vector<uint16> cross_indices;
				bool owner_handled[k_max_cross_sections] = {};
				for (uint32 seed = 0; seed < cross->quads.size(); seed++)
				{
					uint8 owner = cross->quads[seed].owner_section;
					if (owner >= k_max_cross_sections || owner_handled[owner] || !cross_shadows[owner])
					{
						continue;
					}
					owner_handled[owner] = true;
					cross_indices.clear();
					for (uint32 entry_index = 0; entry_index < cross->quads.size(); entry_index++)
					{
						const s_stencil_shadow_cross_quad* entry = &cross->quads[entry_index];
						if (entry->owner_section != owner
							|| entry->partner_section >= k_max_cross_sections
							|| !cross_shadows[entry->partner_section])
						{
							continue;
						}
						const uint32* owner_bits = facing_scratch[owner];
						const uint32* partner_bits = facing_scratch[entry->partner_section];
						bool left_faces = (owner_bits[entry->owner_triangle >> 5]
							>> (entry->owner_triangle & 31)) & 1;
						bool right_faces = (partner_bits[entry->partner_triangle >> 5]
							>> (entry->partner_triangle & 31)) & 1;
						if (left_faces == right_faces)
						{
							continue;
						}
						uint16 vert_a = entry->vert_a;
						uint16 vert_b = entry->vert_b;
						if (right_faces)
						{
							uint16 swap = vert_a;
							vert_a = vert_b;
							vert_b = swap;
						}
						uint16 a0 = (uint16)(vert_a * 2), a1 = (uint16)(vert_a * 2 + 1);
						uint16 b0 = (uint16)(vert_b * 2), b1 = (uint16)(vert_b * 2 + 1);
						cross_indices.push_back(a0); cross_indices.push_back(b0); cross_indices.push_back(b1);
						cross_indices.push_back(a0); cross_indices.push_back(b1); cross_indices.push_back(a1);
					}
					if (!cross_indices.empty())
					{
						stencil_shadow_draw_cross_indices(cross_shadows[owner], cross_indices,
							&toward_light_world, cross_matrices[owner], extrusion_distance, shadow_opacity);
					}
				}
			}

			// P1-1: no per-object darken here. td applies ONCE, fullscreen, after all
			// volumes are laid (render_layer_lightmap_diffuse, td 0x10D8F0) --
			// stencil_shadow_world_darken does that. The per-object scissored apply that
			// used to live here existed only to contain the P0-1 streaks, and it darkened
			// overlapping casters more than once.
			volumes_drawn++;
		}
		else
		{
			dbg_no_sections++;
		}
	}

	// Silent cap: the iteration loop stops at k_max_debug_volumes_per_frame, so any further
	// casters are dropped ENTIRELY -- they lose their shadow rather than getting a truncated
	// one. tag-debug has no such limit (it walks every visible object). Warn once so a busy
	// scene missing shadows on the objects enumerated last is diagnosable.
	if (volumes_drawn >= k_max_debug_volumes_per_frame)
	{
		if (!g_stencil_shadow_warned_caster_cap)
		{
			g_stencil_shadow_warned_caster_cap = true;
			LOG_INFO_GAME("stencil WARNING: hit the {}-caster frame cap; later casters drew no shadow this frame",
				(int32)k_max_debug_volumes_per_frame);
		}
	}

	static uint32 dbg_frame = 0;
	if (++dbg_frame % 3600 == 0)
	{
		// `balance` must be 0. Any other value means a caster left the loop through a path that
		// increments nothing -- i.e. this line is under-reporting and cannot be trusted to explain a
		// missing shadow. lodfail is deliberately absent from the sum (it is informational, not a
		// drop). (it. 328)
		int32 dbg_balance = dbg_iterated
			- (dbg_no_definition + dbg_shadowless + dbg_uncached + dbg_no_model + dbg_far
				+ dbg_opacity + dbg_no_rmodel + dbg_nonmanifold + dbg_no_matrix + dbg_no_sections
				+ (int32)volumes_drawn);
		LOG_INFO_GAME("stencil dbg: mode={} masking={} iter={} nodef={} shadowless={} far={} lodfail={} nolighting={} cinematic={} shallow={} worst_z={:.3f} opac={} nomodel={} normodel={} nonmanifold={} nomatrix={} nosec={} drawn={} balance={}",
			g_stencil_shadow_draw_mode, g_stencil_shadow_masking_pass, dbg_iterated, dbg_no_definition,
			dbg_shadowless, dbg_far, dbg_lodfail, dbg_uncached, dbg_cinematic, dbg_shallow,
			dbg_shallowest_z, dbg_opacity, dbg_no_model, dbg_no_rmodel, dbg_nonmanifold,
			dbg_no_matrix, dbg_no_sections, volumes_drawn, dbg_balance);

		// PROBE ONLY — no behaviour change. Answers the one open question blocking the
		// visibility-list caster proposal (td-caster-selection.md, it. 269-274): WHICH
		// c_visibility_collection instance is live and populated at THIS hook point.
		//
		// render_scene runs ~8 times per frame (main view, first-person, other windows) and there
		// are two instances, so an outside sample cannot answer it — only a read taken here can.
		// Layout, verified against the engine initialiser (halo2.exe 0x4BAF47):
		//   c_visibility_collection: m_lists[4] at +0x0C; objects live in m_lists[2] (it. 270)
		//   c_visibility_object_list: m_capacity +0x00, m_count +0x04 (uint16)
		// Expected capacities from that initialiser: primary list2 = 256, secondary list2 = 128 —
		// if the capacities do not read back as those, the addresses are wrong and the counts are
		// meaningless (this check is the reason capacity is logged at all).
		{
			struct s_probe { uint32 capacity; uint16 count; };
			auto read_list2 = [](uint32 collection_rva, uint32* out_capacity) -> int32
			{
				uint8* collection = Memory::GetAddress<uint8*>(collection_rva);
				if (!collection)
				{
					return -1;
				}
				const s_probe* list = *(const s_probe**)(collection + 0x0C + 2 * sizeof(void*));
				if (!list)
				{
					return -1;
				}
				*out_capacity = list->capacity;
				return (int32)list->count;
			};
			uint32 primary_capacity = 0, secondary_capacity = 0;
			int32 primary_count = read_list2(0x4D2D60, &primary_capacity);
			int32 secondary_count = read_list2(0x4D5840, &secondary_capacity);
			LOG_INFO_GAME("stencil visprobe: primary count={} cap={} (expect 256) | secondary count={} cap={} (expect 128) | our iter={} drawn={}",
				primary_count, primary_capacity, secondary_count, secondary_capacity,
				dbg_iterated, volumes_drawn);
		}
	}

	if (g_stencil_shadow_masking_pass)
	{
		g_stencil_shadow_mask_pending = volumes_drawn > 0 && g_stencil_shadow_draw_mode == 0;
	}
}

// Tag-debug pass 6: lay stencil counts for all object volumes between the
// lightmap-indirect and SH-PRT layers (called from the native render_scene).
void stencil_shadow_lightmap_volumes_pass(void)
{
	// red mode (1) also draws HERE, not at the late hook: mid-scene c0-c3 is the current
	// window wvp, while after render_lights_new the lights path leaves stale transforms
	// (volumes rendered out of place / clipping with distance). Matches tag-debug, whose
	// colorwrite-debug byte draws during pass 6 itself.
	g_stencil_shadow_mask_pending = false;
	if (!g_stencil_shadow_active || !cache_file_is_loaded())
	{
		return;
	}
	IDirect3DDevice9Ex* device = rasterizer_dx9_device_get_interface();
	if (!device)
	{
		return;
	}

	// One-shot probe: Vista's e_global_vertex_shader table still carries the ported stencil
	// shadow shaders -- index 17 stencil_shadow_cap_proj, 18 stitch_no_proj, 19 stitch_proj
	// (the proj/no_proj split is td's point-vs-directional bank, it. 127/175). The enum slots
	// exist; whether the shipped .map holds vertex-shader tag data for them is the open
	// question, and rasterizer_dx9_set_vertex_shader_permutation (halo2.exe 0x26F3CE) resolves
	// any index generically, so binding one and reading it back answers it.
	//
	// Safe here: this runs before our own shader/state setup, and the setter's writes to the
	// engine's cache fields are undone by stencil_shadow_release_pipeline after every draw.
	{
		static bool probed_vista_stencil_shaders = false;
		if (!probed_vista_stencil_shaders)
		{
			probed_vista_stencil_shaders = true;
			IDirect3DVertexShader9* previous = NULL;
			device->GetVertexShader(&previous);
			typedef void(__cdecl* t_set_vs_permutation)(int32);
			t_set_vs_permutation set_permutation =
				Memory::GetAddress<t_set_vs_permutation>(0x26F3CE);
			int32 present = 0;
			for (int32 probe_index = 17; probe_index <= 19; probe_index++)
			{
				set_permutation(probe_index);
				IDirect3DVertexShader9* probed = NULL;
				if (SUCCEEDED(device->GetVertexShader(&probed)) && probed)
				{
					present |= 1 << (probe_index - 17);
					probed->Release();
				}
			}
			LOG_INFO_GAME("stencil probe: vista stencil shaders present bits={:#x} (bit0=cap_proj, bit1=stitch_no_proj, bit2=stitch_proj)",
				present);
			device->SetVertexShader(previous);
			if (previous)
			{
				previous->Release();
			}
		}
	}

	// Single count pass: clear once to 128 (td does the same at layer open --
	// rasterizer_clear(4, 0, 1.0f, 128) in render_layer_stencil_shadow, td 0x10DFE0), then lay
	// ALL casters' volumes. Overlapping volumes still read as one binary shadowed/not-shadowed
	// state; the counts just move further from 128, and the test is != 128.
	//
	// The SH-PRT layer is NO LONGER stencil-masked. It previously drew under an EQUAL-128 mask
	// in addition to this tier's darken, which attenuated a shadowed pixel TWICE where td
	// attenuates once: td's layer 7 is a fullscreen multiply and nothing else (render_layer_draw
	// td 0x10DBB0 dispatches `case 7: return;`, so the layer submits no geometry, and its pixel
	// shader sets PSTextureModes = 0 -- flat constant, samples nothing). stencil_shadow_world_darken
	// now runs AFTER SH-PRT, so the full lightmap term is down before the single multiply lands.
	//
	// The darken uses the global k_stencil_shadow_darkness; per-object render_lighting.shadow_opacity
	// feeds only the per-caster stipple density. Note td does not use a per-object opacity at all --
	// its stipple alpha is min(model_alpha, distance_fade) and its shadow path reads render_lighting
	// at +28 only (see td-shadow-opacity-term.md).
	// one-shot RE capture (design map 6.7): the last ps bound by the lightmap_indirect
	// layer is still bound here — dump its bytecode for the term-separation analysis
	static bool dumped_env_ps = false;
	if (!dumped_env_ps && g_stencil_shadow_active)
	{
		dumped_env_ps = true;
		IDirect3DPixelShader9* current_ps = NULL;
		HRESULT get_ps_result = device->GetPixelShader(&current_ps);
		if (SUCCEEDED(get_ps_result) && current_ps)
		{
			UINT bytecode_size = 0;
			current_ps->GetFunction(NULL, &bytecode_size);
			if (bytecode_size > 0 && bytecode_size < 65536)
			{
				void* bytecode = malloc(bytecode_size);
				if (bytecode && SUCCEEDED(current_ps->GetFunction(bytecode, &bytecode_size)))
				{
					FILE* dump_file = NULL;
					if (fopen_s(&dump_file, "C:\\Users\\Kant\\Desktop\\projects\\banana\\env_lightmap_ps.bin", "wb") == 0 && dump_file)
					{
						fwrite(bytecode, 1, bytecode_size, dump_file);
						fclose(dump_file);
						LOG_INFO_GAME("stencil RE: dumped env lightmap ps ({} bytes)", bytecode_size);
					}
				}
				free(bytecode);
			}
			current_ps->Release();
		}
		else
		{
			LOG_INFO_GAME("stencil RE: GetPixelShader hr={:#x}", (uint32)get_ps_result);
		}
	}

	if (g_stencil_shadow_draw_mode != 1)
	{
		device->Clear(0, NULL, D3DCLEAR_STENCIL, 0, 1.f, 128);
	}
	LARGE_INTEGER perf_start;
	QueryPerformanceCounter(&perf_start);

	g_stencil_shadow_masking_pass = true;
	stencil_shadow_render_layer_hook();
	g_stencil_shadow_masking_pass = false;

	// mode 2: visualize the stencil mask right where the lit layer would test it —
	// opaque green wherever the volume counts mark a pixel shadowed (count != 128)
	if (g_stencil_shadow_draw_mode == 2)
	{
		stencil_shadow_apply_and_clear(1.f, NULL);
	}

	// CPU cost telemetry (CLAUDE.md asks whether a further caching system is warranted —
	// these numbers answer it): accumulated per volumes-pass invocation, logged ~10s
	LARGE_INTEGER perf_end, perf_frequency;
	QueryPerformanceCounter(&perf_end);
	QueryPerformanceFrequency(&perf_frequency);
	static real64 perf_accum_ms = 0.0;
	static real64 perf_max_ms = 0.0;
	static uint32 perf_samples = 0;
	real64 elapsed_ms = (real64)(perf_end.QuadPart - perf_start.QuadPart) * 1000.0
		/ (real64)perf_frequency.QuadPart;
	perf_accum_ms += elapsed_ms;
	if (elapsed_ms > perf_max_ms)
	{
		perf_max_ms = elapsed_ms;
	}
	if (++perf_samples >= 4800)	// ~10s at 8 passes x 60fps
	{
		LOG_INFO_GAME("stencil perf: volumes pass avg={:.3f}ms max={:.3f}ms over {} passes",
			perf_accum_ms / perf_samples, perf_max_ms, perf_samples);
		perf_accum_ms = 0.0;
		perf_max_ms = 0.0;
		perf_samples = 0;
	}
}

// Mode-3 probe (td-application experiment): stencil-mask the world lightmap layer exactly
// like the PRT layer, instead of post-darkening. If Vista's layer 3 carries the FULL
// lightmap (indirect+direct combined), shadowed world pixels go black here — proving the
// darken is required; if a separate direct term exists, they keep indirect light and the
// faithful mask can replace the darken.
void stencil_shadow_mask_world_begin(void)
{
	if (g_stencil_shadow_draw_mode != 3)
	{
		return;
	}
	stencil_shadow_mask_begin();
}

void stencil_shadow_mask_world_end(void)
{
	if (g_stencil_shadow_draw_mode != 3 || !g_stencil_shadow_mask_pending)
	{
		return;
	}
	// end the lock/state but KEEP the counts and pending for the PRT mask that follows
	bool* disable_stencil = rasterizer_dx9_disable_stencil_get();
	*disable_stencil = false;
	rasterizer_dx9_set_stencil_mode(0);
	stencil_shadow_force_render_state(D3DRS_STENCILWRITEMASK, 0xFFFFFFFF);
	*disable_stencil = g_stencil_shadow_saved_disable_stencil;
}

bool stencil_shadow_active(void)
{
	return g_stencil_shadow_active;
}

// Mode-0 application — tag-debug's pass 7 (render_layer_lightmap_diffuse, td 0x10D8F0).
// ONE unscissored fullscreen quad over the whole frame wherever the stencil count differs
// from 128. td's setup (render_layer_lightmap_diffuse_setup, td 0x10D740) is ZFUNC ALWAYS,
// stencil NOTEQUAL/ref 128 with all ops KEEP, and SRCBLEND=ZERO / DESTBLEND=SRCCOLOR against a
// constant grey (1 - flt_53A674) -- a multiply. Ours is SRCALPHA/INVSRCALPHA against black,
// which is algebraically the same: dst*(1-a) == dst*grey.
//
// SINGLE APPLICATION. This used to run BEFORE the SH-PRT layer, which then also drew under an
// EQUAL-128 mask -- so a shadowed pixel was attenuated TWICE: multiplied by 0.6 and then denied
// its direct lightmap term entirely. td attenuates once. Layer 7 is a fullscreen quad and
// nothing else (render_layer_draw td 0x10DBB0 dispatches `case 7: return;`, so the layer submits
// no geometry at all), and its pixel shader sets PSTextureModes = 0 -- it samples nothing and
// emits a flat grey. So td's shadowed pixel is `(everything accumulated so far) * 0.6`, where
// ours was `indirect * 0.6` with the direct term dropped -- structurally darker by the whole
// direct term, and not fixable by tuning k_stencil_shadow_darkness (which is exactly td's 0.4).
//
// The call therefore moved AFTER the SH-PRT layer in render.cpp and the mask around SH-PRT was
// removed: accumulate the full lightmap term first, then darken shadowed pixels once. That also
// matches td's layer order, where the whole lightmap is down (layer 5) before layer 7 darkens.
void stencil_shadow_world_darken(void)
{
	if (!g_stencil_shadow_active || !g_stencil_shadow_mask_pending)
	{
		return;
	}

	// mode 0 is the shipping path — the tier's single application.
	if (g_stencil_shadow_draw_mode == 0)
	{
		stencil_shadow_apply_and_clear(k_stencil_shadow_darkness, NULL);
	}

	// Teardown runs in EVERY draw mode, matching what stencil_shadow_mask_end did from this same
	// point in the frame (it gated on mask_pending only, never on mode): retire the counts and
	// clear the buffer before render_lights_new's dynamic tier reuses it. td does the same --
	// layer 13 re-clears to 128, but only after layer 7 has consumed the lightmap tier's counts.
	g_stencil_shadow_mask_pending = false;
	IDirect3DDevice9Ex* device = rasterizer_dx9_device_get_interface();
	if (device)
	{
		device->Clear(0, NULL, D3DCLEAR_STENCIL, 0, 1.f, 0);
	}
}

// Tag-debug pass 7 companion: the SH-PRT (direct lightmap) layer draws with the stencil
// test rejecting shadowed pixels — shadowed areas keep only the indirect term.
// Engine integration (same pattern as the first-person pass in render_submit.cpp): the
// engine's rasterizer_dx9_set_stencil_mode runs inside layer submits and would stomp our
// state, but it honors g_rasterizer_disable_stencil — set our states, then LOCK the flag
// for the whole layer, and restore mode 0 + the saved flag afterward.
void stencil_shadow_mask_begin(void)
{
	if (!g_stencil_shadow_mask_pending)
	{
		return;
	}
	bool* disable_stencil = rasterizer_dx9_disable_stencil_get();
	g_stencil_shadow_saved_disable_stencil = *disable_stencil;
	*disable_stencil = false;
	stencil_shadow_force_render_state(D3DRS_STENCILENABLE, TRUE);
	stencil_shadow_force_render_state(D3DRS_TWOSIDEDSTENCILMODE, FALSE);
	stencil_shadow_force_render_state(D3DRS_STENCILREF, 128);
	stencil_shadow_force_render_state(D3DRS_STENCILMASK, 0xFFFFFFFF);
	stencil_shadow_force_render_state(D3DRS_STENCILWRITEMASK, 0);
	stencil_shadow_force_render_state(D3DRS_STENCILFUNC, D3DCMP_EQUAL);
	stencil_shadow_force_render_state(D3DRS_STENCILPASS, D3DSTENCILOP_KEEP);
	stencil_shadow_force_render_state(D3DRS_STENCILZFAIL, D3DSTENCILOP_KEEP);
	stencil_shadow_force_render_state(D3DRS_STENCILFAIL, D3DSTENCILOP_KEEP);
	*disable_stencil = true;
}

void stencil_shadow_mask_end(void)
{
	if (!g_stencil_shadow_mask_pending)
	{
		return;
	}
	g_stencil_shadow_mask_pending = false;
	bool* disable_stencil = rasterizer_dx9_disable_stencil_get();
	*disable_stencil = false;
	rasterizer_dx9_set_stencil_mode(0);
	stencil_shadow_force_render_state(D3DRS_STENCILWRITEMASK, 0xFFFFFFFF);
	*disable_stencil = g_stencil_shadow_saved_disable_stencil;
	IDirect3DDevice9Ex* device = rasterizer_dx9_device_get_interface();
	if (device)
	{
		device->Clear(0, NULL, D3DCLEAR_STENCIL, 0, 1.f, 0);
	}
}

void rasterizer_dx9_stencil_shadows_apply_patches(void)
{
	// no engine byte patches: the draw is called directly from Cartographer's native
	// render_scene (render/render.cpp, after render_lights_new)
	LOG_INFO_GAME("stencil shadows: initialized (drawing from native render_scene)");
}

void stencil_shadow_shaders_dispose(void)
{
	if (g_stencil_shadow_vertex_shader)
	{
		g_stencil_shadow_vertex_shader->Release();
		g_stencil_shadow_vertex_shader = NULL;
	}
	if (g_stencil_shadow_pixel_shader)
	{
		g_stencil_shadow_pixel_shader->Release();
		g_stencil_shadow_pixel_shader = NULL;
	}
	if (g_stencil_shadow_vertex_declaration)
	{
		g_stencil_shadow_vertex_declaration->Release();
		g_stencil_shadow_vertex_declaration = NULL;
	}
	if (g_stencil_shadow_index_buffer)
	{
		g_stencil_shadow_index_buffer->Release();
		g_stencil_shadow_index_buffer = NULL;
	}
	if (g_stencil_shadow_vertex_shader_sm3)
	{
		g_stencil_shadow_vertex_shader_sm3->Release();
		g_stencil_shadow_vertex_shader_sm3 = NULL;
	}
	if (g_stencil_shadow_stipple_shader)
	{
		g_stencil_shadow_stipple_shader->Release();
		g_stencil_shadow_stipple_shader = NULL;
	}
}
