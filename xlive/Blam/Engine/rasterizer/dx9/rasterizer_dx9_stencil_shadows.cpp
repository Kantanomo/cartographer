#include "stdafx.h"
#include "rasterizer_dx9_stencil_shadows.h"

#include "rasterizer_dx9.h"
#include "rasterizer_dx9_main.h"
#include "cache/cache_files.h"
#include "cache/pc_geometry_cache.h"
#include "rasterizer_dx9_stencil_shadow_tunables.h"	// it. 624 — every constant, in one place
#include "rasterizer_dx9_stencil_shadow_reach.h"		// it. 625 — the per-pixel reach bound
#include "rasterizer_dx9_stencil_shadow_skinning.h"	// it. 626 — per-frame articulated pose
#include "rasterizer_dx9_stencil_shadow_debug_view.h"	// it. 630 — F6/F7/F8 mode state
#include "rasterizer_dx9_stencil_shadow_diagnostics.h"	// it. 631 — the caster-loop stats
#include "render/render_stencil_shadow_casters.h"		// it. 632 — caster eligibility
#include "render/render_stencil_shadow_dynamic.h"		// it. 633 — the dynamic light tier
#include "render/render_stencil_shadow_environment.h"	// it. 643 — the BSP/cluster tier
#include "geometry/geometry_definitions_new_runtime.h"	// it. 623 — the ISQ/DSQ generator
#include "objects/objects.h"
#include "objects/object_definition.h"
#include "memory/data.h"
#include "math/matrix_math.h"		// matrix4x3_multiply — skinning = node_world x inverse_bind
#include "main/interpolator.h"		// it. 471 interp probe — needs the interpolator's bool result
#include "models/models.h"			// it. 506 probe — s_model_definition::render_only_node_flags (+0xA4)
#include "H2MOD/Modules/OnScreenDebug/OnscreenDebug.h"	// it. 528 — F6 mode shown on screen, not just logged
#include "physics/collisions.h"		// it. 530 — collision_test_vector for the clipped-extrusion experiment
#include "rasterizer/rasterizer_globals.h"
#include "render/render.h"
#include "render/render_lights.h"
#include "render/render_lod_new.h"
#include "Util/Hooks/Hook.h"
#include "H2MOD/Modules/h2log/h2log.h"
#include "vertex_shaders_dx9/preprocessed_hlsl_from_tool/compiled/shadow_extrude.h"
#include "vertex_shaders_dx9/preprocessed_hlsl_from_tool/compiled/shadow_extrude_vs30.h"
#include "vertex_shaders_dx9/preprocessed_hlsl_from_tool/compiled/shadow_extrude_skinned.h"		// it. 660
#include "vertex_shaders_dx9/preprocessed_hlsl_from_tool/compiled/shadow_extrude_skinned_vs30.h"	// it. 660
#include "vertex_shaders_dx9/preprocessed_hlsl_from_tool/compiled/shadow_solid.h"
#include "vertex_shaders_dx9/preprocessed_hlsl_from_tool/compiled/shadow_stipple.h"
// it. 557 — per-pixel reach bound. Lives under shaders/ (not the tool-dumped tree) because it is
// ours, like the other hand-written .fx in that directory.
#include "shaders/compiled/shadow_reach_clip.h"
#include "render/render_cameras.h"		// it. 557 — render_camera::point/forward/z_far for the reach encode
#include "rasterizer_dx9_targets.h"		// it. 557 — _rasterizer_target_z_a8b8g8r8 + set_target_as_texture
#include "rasterizer_dx9_shader_submit.h"	// it. 557 — rasterizer_dx9_set_sampler_state

#include <xmmintrin.h>
#include <unordered_map>
#include <vector>

/* structures */

struct s_isq_globals
{
	uint16 indices[k_stencil_shadow_index_buffer_capacity];
};

/* globals */

static s_isq_globals isq_globals;

static IDirect3DVertexShader9* g_stencil_shadow_vertex_shader = NULL;
static IDirect3DPixelShader9* g_stencil_shadow_pixel_shader = NULL;
static IDirect3DVertexDeclaration9* g_stencil_shadow_vertex_declaration = NULL;
static IDirect3DIndexBuffer9* g_stencil_shadow_index_buffer = NULL;
// SM3 pair for td's stipple-density fade (screen-door clip in the ps; vs_3_0 required
// because SM3 shaders cannot pair with other model versions). NULL when unsupported.
static IDirect3DVertexShader9* g_stencil_shadow_vertex_shader_sm3 = NULL;
// it. 625: the reach-clip shader, its constant block and its active flag moved to
// rasterizer_dx9_stencil_shadow_reach.cpp. It still depends on the SM3 vertex shader above —
// D3D9 forbids mixing shader models — which is why that availability is passed into its encode.
static IDirect3DPixelShader9* g_stencil_shadow_stipple_shader = NULL;

// it. 660 — the GPU-SKINNED pair: 24-byte declaration (position + extrude + ubyte4 local*3 palette
// indices + ubyte4n weights) and the palette-blend extrusion shader in both models (the vs_3_0 twin
// keeps the stipple/reach pixel shaders legal — D3D9 forbids mixing shader models). OPTIONAL, like
// the SM3 pair: any creation failure leaves them NULL, `stencil_shadow_skinned_ready()` reports
// false, and articulated sections keep the CPU pose path exactly as before this iteration.
static IDirect3DVertexDeclaration9* g_stencil_shadow_skinned_declaration = NULL;
static IDirect3DVertexShader9* g_stencil_shadow_skinned_shader = NULL;
static IDirect3DVertexShader9* g_stencil_shadow_skinned_shader_sm3 = NULL;


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




// it. 538: how often the LOD fallback substitutes a level the engine did not request. A non-zero count
// means some shadows are cast from a mesh that is not the one being rendered.
static bool g_stencil_shadow_warned_lod_fallback = false;
static int32 g_stencil_shadow_lod_fallbacks = 0;

// TOMBSTONE (it. 604, trimmed it. 627). `stencil_shadow_section_extrusion` (per-section dynamic
// extrusion, it. 526) and `stencil_shadow_set_clip_plane` (per-section clip plane, it. 540) were
// DELETED with the DYNAMIC and CLIPPED modes they served. set_clip_plane already had no callers;
// section_extrusion returned its `fallback` unchanged once DYNAMIC became unreachable (it. 603), so
// its two call sites were replaced by `extrusion_distance` directly — behaviour-identical.
// Why those modes failed: td-do-not-fix.md entry 15.

// REGISTER LAYOUT IS OURS, NOT td's (clarified it. 285). The c[15] / c[20].w above describe td's
// NV2A microcode. Our HLSL uses its own high registers -- light in c254 (xyz = position,
// w = 1 point / 0 directional, the SAME packing as td's c[15]) and extrusion in c255**.x**, not .w.
// The value is identical (2.0, confirmed live as `extrusion=2.000`); only the register and lane
// differ, and our shader declares them to match. Do not "fix" the upload to c[15]/c[20].w to match
// the comment above -- that would unbind the constants our own shader reads.
// F6 diagnostic: cycle extrusion distance to isolate rasterization failures at extreme
// magnitudes (0 = k_stencil_shadow_extrusion_distance).

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

// Draw mode (F7 cycles): 0 = real shadows (MASKING architecture: volumes between the
// lightmap-indirect and SH-PRT layers; the PRT draw is stencil-masked — tag-debug passes
// 6/7), 1 = translucent red volume visualization, 2 = stencil plumbing diagnostic.
static bool g_stencil_shadow_masking_pass = false;	// inside the volumes pass (mode 0)
static bool g_stencil_shadow_mask_pending = false;	// volumes counted; PRT layer should mask
static bool g_stencil_shadow_saved_disable_stencil = false;	// engine lock flag save (mask scope)

// DRAW-SIDE per-map one-shot latches. FILE SCOPE on purpose, reset by stencil_shadow_cache_clear.
//
// it. 310 established the rule: as function-local statics these capped per PROCESS, so whichever map
// loaded first consumed them and later maps were never described — the reason it. 226 found `vbuf:`
// data only in a previous session's log. it. 330 and it. 353 swept further latches out of function
// scope for the same reason. A new latch that is function-static will silently describe only the
// first map of a session.
//
// it. 623: the GENERATION-side latches moved to geometry_definitions_new_runtime.cpp with the code
// that emits them, and are reset by stencil_shadow_generation_cache_clear. Split by OWNERSHIP — a
// latch belongs with the module that prints it, so add new ones beside their emitter and reset them
// in that module's clear.
static bool g_stencil_shadow_warned_no_apply = false;
static bool g_stencil_shadow_warned_no_static_bind = false;
static bool g_stencil_shadow_warned_shadows_off = false;
static bool g_stencil_shadow_warned_cross_draw_failed = false;
// Capacity caps biting: index-buffer overflow x2, the 64-section stitch cap and the caster cap.
// td-INDEX.md lists "did a capacity cap ever bite?" as a question the next run is supposed to
// answer, so a cap that bit harder on a later map must not be silent.
static bool g_stencil_shadow_warned_index_overflow_volume = false;
static bool g_stencil_shadow_warned_index_overflow_cross = false;
static bool g_stencil_shadow_warned_cross_cap = false;
static bool g_stencil_shadow_warned_caster_cap = false;
// it. 506: bounds it. 487's render-only-node / eye-tracking divergence. One-shot per map.
static bool g_stencil_shadow_probed_render_only = false;
// it. 477: sections dropped for classification > skinned. td casts from class 4; we do not. Latched
// per class value so output is bounded, with a running count for magnitude.
static uint32 g_stencil_shadow_skipped_class_mask = 0;
static uint32 g_stencil_shadow_skipped_class_count = 0;
// it. 480: BOUNDED latches ("log the first N"), moved here from function scope. They are latches, not
// throttles — they stop firing forever — so under the per-map rule they must be file-scope and reset
// in stencil_shadow_cache_clear. The documented `static bool` sweep could not see them because they
// are int32; see the widened check in td-do-not-fix.md.
// it. 471: the interp probe samples ACROSS FRAMES rather than once — see the probe for why a
// single sample cannot distinguish "no lag" from "sampled at the wrong instant".

// it. 623 — THE ISQ/DSQ GENERATOR NOW LIVES IN geometry/geometry_definitions_new_runtime.cpp.
//
// Vertex access across every position format, welding, edge pairing, plane construction,
// cross-section seam pairing, validation and the section cache all moved there. What stayed here is
// what tag debug also keeps rasterizer-side: the per-frame facing bitvector
// (`rasterizer_stencilshadow_build_bitvector_from_rigid_groups`, td 0x1A1100), the per-frame
// skinning (`section_skin_from_rigid_point_groups`, td 0x19EAF0), and everything that issues D3D.
// See that module's header for the full rationale.

/* private code */


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

	// it. 660 — the GPU-skinned pair, optional exactly like the SM3 pair below: failures leave the
	// pointers NULL and articulated sections keep the CPU pose path. The declaration's UBYTE4 /
	// UBYTE4N types are universal on the D3D9Ex-era hardware Cartographer runs on; a device that
	// rejects them simply gets the CPU path.
	{
		const D3DVERTEXELEMENT9 skinned_elements[] =
		{
			{ 0, (WORD)offsetof(s_stencil_shadow_skinned_vertex, position),
				D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
			{ 0, (WORD)offsetof(s_stencil_shadow_skinned_vertex, extrude),
				D3DDECLTYPE_FLOAT1, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
			{ 0, (WORD)offsetof(s_stencil_shadow_skinned_vertex, indices),
				D3DDECLTYPE_UBYTE4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_BLENDINDICES, 0 },
			{ 0, (WORD)offsetof(s_stencil_shadow_skinned_vertex, weights),
				D3DDECLTYPE_UBYTE4N, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_BLENDWEIGHT, 0 },
			D3DDECL_END()
		};
		static_assert(sizeof(s_stencil_shadow_skinned_vertex) == 24,
			"skinned shadow vertex stride is baked into the VB build and the stream bind");
		if (FAILED(device->CreateVertexDeclaration(skinned_elements,
			&g_stencil_shadow_skinned_declaration)))
		{
			g_stencil_shadow_skinned_declaration = NULL;
		}
		if (g_stencil_shadow_skinned_declaration
			&& FAILED(device->CreateVertexShader((const DWORD*)k_shadow_extrude_skinned_vs_2_0,
				&g_stencil_shadow_skinned_shader)))
		{
			g_stencil_shadow_skinned_shader = NULL;
		}
	}

	// optional SM3 pair for td's stipple fade — failures leave the pointers NULL and
	// faded objects simply cast full-strength shadows
	if (rasterizer_globals_get()->d3d9_sm3_supported)
	{
		if (g_stencil_shadow_skinned_shader
			&& FAILED(device->CreateVertexShader((const DWORD*)k_shadow_extrude_skinned_vs_3_0,
				&g_stencil_shadow_skinned_shader_sm3)))
		{
			g_stencil_shadow_skinned_shader_sm3 = NULL;
		}
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
		// it. 557 — reach clip. Optional exactly like the stipple pair: a failure here must leave
		// the other modes working, so it only disables its own F6 mode.
		stencil_shadow_reach_shader_create(device);
	}
	return true;
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

// it. 626: the per-frame articulated skin + plane recompute moved to
// rasterizer_dx9_stencil_shadow_skinning.cpp. It stays RASTERIZER-side on purpose — tag debug
// keeps its equivalent (section_skin_from_rigid_point_groups, td 0x19EAF0) there too.


/* public code */

// it. 651 — LIGHT REACH CULL. Clear the facing bit of any triangle entirely beyond the light's
// radius, so it generates no silhouette.
//
// THIS IS OUR EQUIVALENT OF td's SUBCLUSTERS, and it. 643 was wrong to call those "culling
// granularity, not correctness". td partitions each cluster and processes only the subclusters that
// are visible AND carmack; without any such partition, EVERY triangle of a cluster casts, however far
// from the light it sits. Measured on Metropolis: one cluster spans 1173 x 599 wu and carries 1788
// shadow triangles, against a flashlight that reaches 5 wu. Volumes appeared across the entire map.
//
// A triangle beyond the light's radius receives no light, so it can block none — clearing its bit is
// not an approximation, it is the correct answer. And the edges where an in-reach triangle meets an
// out-of-reach one BECOME silhouettes, which closes the volume exactly where the light stops.
//
// Uses base_positions, which the builder keeps for every section. Guarded on `!articulated` because
// for those the array is the BIND pose while the light here is world space.
static void stencil_shadow_cull_facing_by_reach(
	const s_stencil_shadow_section* shadow,
	const real_point3d* light_position,
	real32 light_reach,
	uint32* bitvector)
{
	if (light_reach <= 0.f || shadow->articulated
		|| !shadow->base_positions || !shadow->triangles)
	{
		return;
	}
	const real32 reach_squared = light_reach * light_reach;
	for (uint32 plane_index = 0; plane_index < shadow->plane_count; plane_index++)
	{
		const uint32 mask = 1u << (plane_index & 31);
		if ((bitvector[plane_index >> 5] & mask) == 0)
		{
			continue;		// already not facing — nothing to clear
		}
		const uint16* triangle = &shadow->triangles[plane_index * 3];
		bool within_reach = false;
		for (int32 corner = 0; corner < 3 && !within_reach; corner++)
		{
			const real_point3d* position = &shadow->base_positions[triangle[corner]];
			const real32 dx = position->x - light_position->x;
			const real32 dy = position->y - light_position->y;
			const real32 dz = position->z - light_position->z;
			within_reach = (dx * dx + dy * dy + dz * dz) <= reach_squared;
		}
		if (!within_reach)
		{
			bitvector[plane_index >> 5] &= ~mask;
		}
	}
}

void stencil_shadow_build_facing_bitvector(
	const s_stencil_shadow_section* shadow,
	const real_point3d* light_position,
	bool point_light,
	uint32* out_bitvector,
	real32 light_reach)
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
		stencil_shadow_cull_facing_by_reach(shadow, light_position, light_reach, out_bitvector);
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
	real32 opacity,
	real32 self_shadow_bias,
	const real_vector4d* palette_rows,
	int32 palette_matrix_count)
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

	uint32 ib_size = 0;

	for (uint32 quad_index = 0; quad_index < shadow->quad_count; quad_index++)
	{
		const s_stencil_shadow_quad* quad = &shadow->quads[quad_index];
		if (quad->tri_right == k_stencil_shadow_matched_boundary)
		{
			continue;	// seam bridged by the model's cross-quad pass
		}

		bool left_faces = BIT_VECTOR_TEST_FLAG(facing_bitvector, quad->tri_left);
		bool right_faces = quad->tri_right != k_stencil_shadow_boundary_triangle && BIT_VECTOR_TEST_FLAG(facing_bitvector, quad->tri_right);

		if (left_faces == right_faces)
		{
			continue;
		}

		// order the quad so its front faces away from the lit triangle; same-winding
		// source pairs never swap (both sides want the stored orientation)
		uint16 vert_a = quad->vert_a;
		uint16 vert_b = quad->vert_b;
		bool same_winding = shadow->quad_same_winding_bits && BIT_VECTOR_TEST_FLAG(shadow->quad_same_winding_bits, quad_index);

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

		isq_globals.indices[ib_size + 0] = a0;
		isq_globals.indices[ib_size + 1] = b0;
		isq_globals.indices[ib_size + 2] = b1;
		isq_globals.indices[ib_size + 3] = a0;
		isq_globals.indices[ib_size + 4] = b1;
		isq_globals.indices[ib_size + 5] = a1;

		ib_size += 6;
	}

	// caps close the volume for z-fail counting (tag-debug convention: front cap =
	// light-facing triangles at original positions; back cap = NON-facing triangles at
	// extruded positions — their winding already faces outward at the far end)
	for (uint32 triangle_index = 0; triangle_index < shadow->plane_count; triangle_index++)
	{
		const uint16* triangle = &shadow->triangles[triangle_index * 3];
		bool faces = BIT_VECTOR_TEST_FLAG(facing_bitvector, triangle_index);
		if (faces)
		{
			isq_globals.indices[ib_size + 0] = (triangle[0] * 2);
			isq_globals.indices[ib_size + 1] = (triangle[1] * 2);
			isq_globals.indices[ib_size + 2] = (triangle[2] * 2);
		}
		else
		{
			isq_globals.indices[ib_size + 0] = (triangle[0] * 2 + 1);
			isq_globals.indices[ib_size + 1] = (triangle[1] * 2 + 1);
			isq_globals.indices[ib_size + 2] = (triangle[2] * 2 + 1);
		}

		ib_size += 3;
	}

	if (ib_size==0)
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
	if (stencil_shadow_debug_draw_mode() == 1)
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
	stencil_shadow_force_render_state(D3DRS_SCISSORTESTENABLE, TRUE);
	stencil_shadow_force_render_state(D3DRS_CLIPPLANEENABLE, 1);
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

	// it. 660 — the GPU-skinned draw: static bind-pose VB + the palette at c50 replacing the single
	// node matrix. The silhouette indices were emitted from CPU planes posed by the SAME matrices
	// (pool-or-composed, measured identical in it. 659), so the drawn verts and the facing decision
	// cannot disagree.
	const bool gpu_skin = shadow->skinned_vb && palette_rows && palette_matrix_count > 0
		&& stencil_shadow_skinned_ready();
	if (gpu_skin)
	{
		// Cache-coherent palette upload: the engine's vertex-shader constant cache is a flat
		// float4[256] at engine 0xA3C7B0, and the engine's own palette upload
		// (rasterizer_dx9_set_region_skinning_from_pool, 0x662D17) memcpys the rows there before
		// the device call. Mirror it exactly, so later test_cache-based engine uploads reason from
		// what c50.. actually holds. `g_region_skinning_active` is deliberately untouched — we
		// never enter the engine's region-skinning state machine.
		memcpy(Memory::GetAddress<real32*>(0xA3C7B0) + 50 * 4, palette_rows,
			(size_t)palette_matrix_count * 3 * sizeof(real_vector4d));
		device->SetVertexShaderConstantF(50, (const real32*)palette_rows, palette_matrix_count * 3);
	}
	else
	{
		stencil_shadow_set_node_constants(model_matrix);
	}

	// Convention (matches tag debug 0x82B954 usage): light_position is the point light position,
	// or for directional lights the vector TOWARD the light. Shader extrudes along
	// pos*c4.w - c4.xyz, so both cases pass through unchanged (directional: -toward = away).
	real32 light_constant[4];
	light_constant[0] = light_position->x;
	light_constant[1] = light_position->y;
	light_constant[2] = light_position->z;
	light_constant[3] = point_light ? 1.f : 0.f;
	// it. 539: .y is the MINIMUM extrusion the shader will apply under clipping. Non-zero only when a
	// clip plane is active — see the shader for why zero is unsafe there (coincident caps cancel and the
	// shadow disappears). Zero in every other mode, which never consults it.
	// it. 605: `.y` carried the CLIPPED mode's minimum extrusion (it. 539). That mode and the shader
	// branch reading it are gone, so the slot is unused and zero.
	// it. 615: `.y` is the SELF-SHADOW BIAS — see shadow_extrude.fx. The slot was freed when the
	// CLIPPED minimum-extrusion was removed (it. 605), so this needs no new register.
	real32 extrusion_constant[4] =
		{ extrusion_distance, self_shadow_bias, 0.f, 0.f };
	device->SetVertexShaderConstantF(k_stencil_shadow_light_constant, light_constant, 1);
	device->SetVertexShaderConstantF(k_stencil_shadow_extrusion_distance_constant, extrusion_constant, 1);

	if (ib_size > k_stencil_shadow_index_buffer_capacity)
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
				ib_size, (uint32)k_stencil_shadow_index_buffer_capacity);
		}
		ib_size = k_stencil_shadow_index_buffer_capacity;
	}

	void* ib_data = NULL;
	if (FAILED(g_stencil_shadow_index_buffer->Lock(0, ib_size * sizeof(uint16), &ib_data, D3DLOCK_DISCARD)))
	{
		return;
	}
	
	memcpy(ib_data, isq_globals.indices, ib_size * sizeof(uint16));
	
	g_stencil_shadow_index_buffer->Unlock();

	device->SetVertexDeclaration(gpu_skin
		? g_stencil_shadow_skinned_declaration : g_stencil_shadow_vertex_declaration);
	// td stipple fade: fragments screen-door-clipped at the object's shadow opacity so
	// faded objects mark proportionally fewer stencil pixels (SM3 pair required)
	bool stipple = stencil_shadow_debug_draw_mode() != 1 && opacity < 0.995f
		&& g_stencil_shadow_vertex_shader_sm3 && g_stencil_shadow_stipple_shader;
	// it. 557: reach clip also needs the SM3 vertex shader (D3D9 forbids mixing shader models, the
	// same rule stencil_shadow_shaders_initialize records). It takes priority over stipple when both apply —
	// they both want the single pixel-shader slot, and losing the stipple fade in an experimental
	// mode is far cheaper than losing the reach bound this mode exists to test.
	const bool reach_clip = stencil_shadow_reach_is_active()
		&& stencil_shadow_debug_draw_mode() != 1 && stencil_shadow_reach_shader_ready();
	device->SetVertexShader((stipple || reach_clip)
		? (gpu_skin ? g_stencil_shadow_skinned_shader_sm3 : g_stencil_shadow_vertex_shader_sm3)
		: (gpu_skin ? g_stencil_shadow_skinned_shader : g_stencil_shadow_vertex_shader));
	if (stencil_shadow_debug_draw_mode() == 1)
	{
		const real32 tint[4] = { 1.f, 0.125f, 0.125f, 0.375f };
		device->SetPixelShader(g_stencil_shadow_pixel_shader);
		device->SetPixelShaderConstantF(k_stencil_shadow_tint_constant, tint, 1);
	}
	else if (reach_clip)
	{
		stencil_shadow_reach_bind(device);
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
	if (gpu_skin)
	{
		device->SetStreamSource(0, shadow->skinned_vb, 0, sizeof(s_stencil_shadow_skinned_vertex));
	}
	else
	{
		device->SetStreamSource(0, shadow->shadow_vb, 0, sizeof(s_stencil_shadow_vertex));
	}
	device->SetIndices(g_stencil_shadow_index_buffer);

	HRESULT draw_result = device->DrawIndexedPrimitive(
		D3DPT_TRIANGLELIST,
		0,
		0,
		shadow->welded_vertex_count * 2,
		0,
		ib_size / 3);
	stencil_shadow_force_render_state(D3DRS_DEPTHBIAS, 0);

	// it. 568 — RELEASE SAMPLER 0. The reach path binds the depth target to s0 and forces POINT
	// filtering; leaving either in place corrupts every later draw that samples s0 without setting its
	// own texture. The user saw exactly that: a wall decal (the "3" numeral) rendering as a flat black
	// shape. Unbinding the texture is the part that matters — the filter is restored to LINEAR as well
	// so a later consumer relying on the engine's default is not silently point-sampled.
	// it. 573 — RELEASE THROUGH THE CACHED SETTER, never the raw device.
	//
	// it. 568 used `device->SetTexture(0, NULL)` and desynced the engine's redundancy cache: the cache
	// still believed the depth target was bound, so the NEXT section's bind no-opped and every draw after
	// the first sampled nothing (MEASURED: `texture=1` on sample 1, `texture=0` on 2-4), while the engine
	// separately believed s0 was populated — which is what corrupted the wall decals.
	//
	// `rasterizer_dx9_device_set_texture` is the setter `rasterizer_dx9_set_target_as_texture` itself ends
	// in (targets.cpp:353). Its body (IDB 0x66EBC7) is `if (cache[stage] != texture) { SetTexture(...);
	// cache[stage] = texture; }` — a plain compare-and-set that handles NULL, VERIFIED by decompilation.
	// Releasing through it keeps cache and device in step, so the next bind is seen as a real change.
	if (reach_clip)
	{
		stencil_shadow_reach_unbind();
	}

	if (FAILED(draw_result))
	{
		LOG_INFO_GAME("stencil draw FAILED: hr={:#x} indices={} verts={} mode={}",
			(uint32)draw_result, ib_size, shadow->welded_vertex_count * 2,
			stencil_shadow_debug_draw_mode());
	}

	stencil_shadow_release_pipeline(device);
	return;
}


// Apply: darken every pixel whose stencil count != 128 (midpoint convention), optionally
// scissored to a rect, with the given darkness. Caller manages stencil clears.
bool stencil_shadow_sm3_vertex_shader_ready(void)
{
	return g_stencil_shadow_vertex_shader_sm3 != NULL;
}

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
	// it. 635 — THIS DIAGNOSTIC NOW HAS A SECOND, LEGITIMATE CALLER. The dynamic light tier
	// (render_stencil_shadow_dynamic.cpp, F5) calls this once more per frame WITH a scissor, which
	// is correct for it — its shadow is bounded to the lights' screen extent. So while that tier is
	// enabled, `calls=2 scissored=1` is EXPECTED and is NOT evidence the per-caster apply came back.
	//
	// Read the two together: with F5 off, the invariant below still holds exactly as written. With
	// F5 on, subtract one scissored call before judging it. Recorded here rather than teaching the
	// diagnostic about the other tier, because a shared counter that knows about both is harder to
	// reason about than one whose exception is written down.
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
					last_darkness, last_scissored ? 1 : 0, (int32)stencil_shadow_debug_draw_mode(),
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
	const bool diagnostic = stencil_shadow_debug_draw_mode() == 2;
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
	// it. 539: .y is the MINIMUM extrusion the shader will apply under clipping. Non-zero only when a
	// clip plane is active — see the shader for why zero is unsafe there (coincident caps cancel and the
	// shadow disappears). Zero in every other mode, which never consults it.
	// it. 605: `.y` carried the CLIPPED mode's minimum extrusion (it. 539). That mode and the shader
	// branch reading it are gone, so the slot is unused and zero.
	// it. 615: `.y` is the SELF-SHADOW BIAS — see shadow_extrude.fx. The slot was freed when the
	// CLIPPED minimum-extrusion was removed (it. 605), so this needs no new register.
	real32 extrusion_constant[4] =
		{ extrusion_distance, k_stencil_shadow_self_shadow_bias, 0.f, 0.f };
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
	// it. 623: the generated data and the latches that describe BUILDING it are owned by
	// geometry_definitions_new_runtime.cpp and cleared there. This function keeps the public name
	// and both existing call sites (map unload, and before device reset) — only the ownership of
	// the two halves changed.
	stencil_shadow_generation_cache_clear();
	stencil_shadow_skinning_reset_diagnostics();

	// Refresh the DRAW-side diagnostic budgets so each map gets its own samples (it. 310).
	// Without this they were process-lifetime caps and only the first map loaded was ever
	// described — see the "How to run it" note in td-INDEX.md.
	g_stencil_shadow_warned_no_apply = false;
	g_stencil_shadow_warned_no_static_bind = false;
	g_stencil_shadow_warned_shadows_off = false;
	g_stencil_shadow_warned_cross_draw_failed = false;
	g_stencil_shadow_warned_index_overflow_volume = false;
	g_stencil_shadow_warned_index_overflow_cross = false;
	g_stencil_shadow_warned_cross_cap = false;
	g_stencil_shadow_warned_caster_cap = false;
	g_stencil_shadow_probed_render_only = false;
	g_stencil_shadow_warned_lod_fallback = false;
	g_stencil_shadow_lod_fallbacks = 0;
	g_stencil_shadow_skipped_class_mask = 0;
	g_stencil_shadow_skipped_class_count = 0;
	render_stencil_shadow_casters_reset_diagnostics();
	stencil_shadow_dynamic_reset_diagnostics();
	stencil_shadow_environment_reset_diagnostics();
}

void __cdecl stencil_shadow_render_layer_hook(void)
{
	static bool logged_first_fire = false;
	if (!logged_first_fire)
	{
		logged_first_fire = true;
		LOG_INFO_GAME("stencil shadows: render hook first fire");
	}

	if (!stencil_shadow_active() || !cache_file_is_loaded())
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
	// for td parity — `stencil_shadow_compute_screen_rect` used to record it directly ("its only
	// caller was the per-object scissored darken, removed for td parity"). That function was
	// DELETED in it. 609 as unreachable, so this note is now the only record. The application is a
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

	// cap + rationale: k_stencil_shadow_max_casters_per_frame, rasterizer_dx9_stencil_shadow_tunables.h
	int32 volumes_drawn = 0;

	// Per-frame caster telemetry. Field docs and the balance invariant live in
	// rasterizer_dx9_stencil_shadow_diagnostics.h — the counters MUST sum (it. 631).
	s_stencil_shadow_frame_stats dbg;
	stencil_shadow_stats_reset(&dbg);

	// caster set + rationale: k_stencil_shadow_caster_mask, rasterizer_dx9_stencil_shadow_tunables.h
	c_object_iterator<object_datum> object_iterator;
	object_iterator.begin((e_object_type)k_stencil_shadow_caster_mask, 0);
	while (object_iterator.next() && volumes_drawn < k_stencil_shadow_max_casters_per_frame)
	{
		// dbg.iterated counts EVERY object the iterator yields, before any rejection, so the
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
		dbg.iterated++;
		const object_datum* object = object_iterator.get_datum();
		if (!object || object->definition_index == NONE)
		{
			// previously the only SILENT drop in the loop: an object with no definition appeared in
			// no counter at all, not even the iterated total
			dbg.no_definition++;
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
			dbg.shadowless++;
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
			dbg.cinematic++;
		}
		const render_lighting* lighting =
			render_object_cache_get_lighting(object_iterator.get_index());
		if (!lighting)
		{
			dbg.no_lighting++;
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
					dbg.shallow++;
				}
				if (normalized_z > dbg.shallowest_z)
				{
					dbg.shallowest_z = normalized_z;
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
			dbg.no_model++;
			continue;
		}
		const tag_reference* hlmt_render_model_reference =
			(const tag_reference*)tag_get_fast(object_definition->model.index);
		if (!hlmt_render_model_reference || hlmt_render_model_reference->index == NONE)
		{
			dbg.no_model++;
			continue;
		}
		datum render_model_index = hlmt_render_model_reference->index;

		// it. 506 PROBE — DIAGNOSTIC ONLY, changes nothing. Bounds it. 487's divergence.
		//
		// The engine skins from `object_compute_render_time_node_matrices` (0x53599B), which we skip.
		// That function does three things: recompute animated orientations, **compose render-only
		// nodes onto their parents** (`render_only_node_flags`), and apply **eye tracking** for units.
		// it. 502 established it is LIVE on our casters — it early-outs on
		// `node_orientation_block.offset == -1`, and `objects.cpp:642-644` allocates that block only
		// when the object can interpolate, which the it. 498 run proved true (`ran` 200-240 of 240).
		// What has never been measured is how much it MOVES anything, and that decides whether the
		// item is worth the risky plumbing (0x53599B dereferences `section_indices` and mutates its
		// input array in place, so a wrong argument is a crash, not a wrong shadow).
		//
		// Reading it:
		//   node_orient=-1/0        -> the function early-outs for this caster; NO divergence at all,
		//                              and it. 502's one unverified link (does a zero-size allocation
		//                              yield offset -1?) is answered in the affirmative
		//   render_only_nodes=0     -> no cosmetic nodes to compose; the divergence reduces to eye
		//                              tracking alone, i.e. head/eye nodes on units — small and
		//                              bounded, and it. 487's "probably secondary" is CONFIRMED
		//   render_only_nodes>0     -> that many nodes are posed by a composition we skip, so their
		//                              geometry sits wrongly in the shadow. Worth the plumbing
		//
		// `render_only_node_flags` is at s_model_definition+0xA4 — verified against the Vista IDB's
		// own type (offset 0xA4, int8[32], struct size 252) and pinned by
		// `ASSERT_STRUCT_SIZE(s_model_definition, 252)`, so this is not a header-trusting read.
		// One-shot per map; latch reset in stencil_shadow_cache_clear.
		// it. 510: published for the BUILDER's intersection probe (see stencil_shadow_section_build).
		// Set every iteration, not just while probing, so the builder always sees the flags belonging
		// to the caster it is building for. Cleared per map in stencil_shadow_cache_clear.
		{
			const s_model_definition* current_model =
				(const s_model_definition*)tag_get_fast(object_definition->model.index);
			g_stencil_shadow_render_only_flags = current_model ? current_model->render_only_node_flags : NULL;
			g_stencil_shadow_render_only_node_count =
				object->object.node_orientation_block.size / 32;
		}
		if (!g_stencil_shadow_probed_render_only)
		{
			g_stencil_shadow_probed_render_only = true;
			const s_model_definition* model_definition =
				(const s_model_definition*)tag_get_fast(object_definition->model.index);
			// it. 507: BOUND THE POPCOUNT BY node_count. The first version counted all 256 bits of
			// `render_only_node_flags[32]` and reported **45** for a model with **33** nodes — more
			// render-only nodes than nodes. Bits at or above `node_count` are not meaningful (the tool
			// only defines the low `node_count` of them), so counting them inflates the answer with
			// whatever the tag happens to carry there.
			//
			// `node_orientation_block.size` is the node count times 32 — that is literally how the
			// engine sizes it (`objects.cpp:642`: `32 * node_count`), and the live read confirms it:
			// size 1056 = 32 x 33, against the 33 nodes it. 378 measured on this biped. So the bound
			// is derived from the same field rather than assumed.
			//
			// Both numbers are reported: `raw` exposes bits set beyond the node count (which would
			// mean the flags field carries junk, worth knowing on its own), `nodes` is the answer.
			const int32 probe_node_count = object->object.node_orientation_block.size / 32;
			int32 render_only_nodes = 0;
			int32 render_only_raw = 0;
			if (model_definition)
			{
				for (int32 bit_index = 0; bit_index < 256; bit_index++)
				{
					const uint8 byte = (uint8)model_definition->render_only_node_flags[bit_index >> 3];
					if (((byte >> (bit_index & 7)) & 1) == 0)
					{
						continue;
					}
					render_only_raw++;
					if (bit_index < probe_node_count)
					{
						render_only_nodes++;
					}
				}
			}
			// Object type is deliberately not logged: it. 420 established the caster mask is
			// `_object_mask_unit`, so every caster reaching here is already a unit and eye tracking
			// applies by construction. Logging it would add a column that is constant.
			LOG_INFO_GAME("stencil renderonly probe: node_orient=off {} size {} nodes={} | render_only_nodes={} (raw bits {}) (it. 487/502/506/507 — offset -1 or size 0 means 0x53599B early-outs and there is NO divergence; nodes=0 bounds it to eye tracking alone; raw >> nodes just means junk bits past node_count)",
				(int32)object->object.node_orientation_block.offset,
				(int32)object->object.node_orientation_block.size,
				probe_node_count,
				render_only_nodes,
				render_only_raw);

			// it. 512 — HOW FAR does the composition we skip actually MOVE those nodes?
			//
			// it. 511 measured SCOPE (12 of the 19 nodes a biped's shadow binds are render-only) but
			// not MAGNITUDE, and scope alone does not justify the change: `stencil inflation:` reads
			// 0.72-0.99 (it. 504) and the shadows look right, so the correction must be small. Shipping
			// a reimplementation of engine node composition onto a working state, for an unmeasured
			// gain, is the wrong trade — so measure first, exactly as it. 471 preceded it. 500.
			//
			// Reproduces 0x53599B's render-only pass READ-ONLY, into a scratch copy:
			//     matrix4x3_from_orientation(m, &orientations[i]);
			//     matrix4x3_multiply(&composed[parent_node(i)], m, &composed[i]);
			// A single forward pass is correct because Halo node arrays are ordered parent-before-child,
			// so `composed[parent]` is already final when child `i` is reached.
			//
			// Deliberately NOT calling 0x53599B itself: it mutates its input array in place and
			// dereferences `section_indices`, where a wrong argument is a crash rather than a wrong
			// number (it. 487/502). Reimplementing just the one loop, read-only, has neither hazard.
			// Eye tracking is not reproduced — it is a separate effect on head/eye nodes only.
			//
			//   max_pos ~1e-4 wu -> the correction is negligible; it. 487 closes as MEASURED-IMMATERIAL
			//   max_pos >~1e-2 wu -> a visible limb-scale offset (the it. 431 yardstick on a 0.705 wu
			//                        caster); build the plumbing
			const render_model_definition* probe_render_model =
				(const render_model_definition*)tag_get_fast(render_model_index);
			int32 orientation_count = 0;
			const real_orientation* orientations = (const real_orientation*)object_header_block_get_with_count(
				object_iterator.get_index(), &object->object.node_orientation_block,
				sizeof(real_orientation), &orientation_count);
			int32 raw_count = 0;
			const real_matrix4x3* raw_nodes =
				(const real_matrix4x3*)object_get_node_matrices(object_iterator.get_index(), &raw_count);
			if (model_definition && probe_render_model && orientations && raw_nodes
				&& orientation_count > 0 && raw_count >= orientation_count
				&& orientation_count <= (int32)probe_render_model->nodes.count)
			{
				static real_matrix4x3 composed[256];
				const int32 count = orientation_count < 256 ? orientation_count : 256;
				for (int32 i = 0; i < count; i++) { composed[i] = raw_nodes[i]; }
				real32 max_pos = 0.f, max_deg = 0.f;
				int32 compared = 0;
				// it. 515 SELF-CHECK on this probe's one unverified assumption. The single forward pass
				// is only valid if node arrays are ordered PARENT-BEFORE-CHILD, so that `composed[parent]`
				// is already final when child `i` is reached. That is the usual Halo convention but it was
				// asserted, not measured — and an out-of-order parent would compose against a RAW parent
				// instead of a composed one, silently under-reporting the delta. Counting violations makes
				// the number self-validating: `order_violations=0` means the pass was legitimate.
				int32 order_violations = 0;
				for (int32 i = 1; i < count; i++)
				{
					if (((model_definition->render_only_node_flags[i >> 3] >> (i & 7)) & 1) == 0) { continue; }
					const int16 parent = probe_render_model->nodes[i]->parent_node;
					if (parent < 0 || parent >= count) { continue; }
					if (parent >= i) { order_violations++; }
					real_matrix4x3 local;
					matrix4x3_from_orientation(&local, &orientations[i]);
					matrix4x3_multiply(&composed[parent], &local, &composed[i]);
					const real32 dx = composed[i].position.x - raw_nodes[i].position.x;
					const real32 dy = composed[i].position.y - raw_nodes[i].position.y;
					const real32 dz = composed[i].position.z - raw_nodes[i].position.z;
					const real32 pos_delta = sqrtf(dx * dx + dy * dy + dz * dz);
					real32 cos_theta = composed[i].vectors.forward.i * raw_nodes[i].vectors.forward.i
						+ composed[i].vectors.forward.j * raw_nodes[i].vectors.forward.j
						+ composed[i].vectors.forward.k * raw_nodes[i].vectors.forward.k;
					if (cos_theta > 1.f) { cos_theta = 1.f; }
					if (cos_theta < -1.f) { cos_theta = -1.f; }
					const real32 deg = (real32)(acos((real64)cos_theta) * (180.0 / 3.14159265358979323846));
					if (pos_delta > max_pos) { max_pos = pos_delta; }
					if (deg > max_deg) { max_deg = deg; }
					compared++;
				}
				LOG_INFO_GAME("stencil renderonly delta: compared={} max_pos={:.5f}wu max_fwd={:.3f}deg order_violations={} (it. 512/515 — magnitude of the composition we skip; ~1e-4 closes it. 487 as immaterial, >~1e-2 justifies the plumbing. order_violations MUST be 0 or the single forward pass composed against a raw parent and the deltas are UNDER-reported)",
					compared, max_pos, max_deg, order_violations);
			}
		}

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
				dbg.far_culled++;
				continue;
			}
		}
		else
		{
			// the model's LOD block did not validate -- we are on the old fixed reach, so
			// td's fade is NOT actually running for this object. Counted so a permanently
			// high lodfail is visible instead of silently reverting to the old behaviour.
			dbg.lod_fail++;
			if (camera_distance > k_stencil_shadow_fallback_cull_distance)
			{
				dbg.far_culled++;
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
			dbg.opacity++;
			continue;
		}

		render_model_definition* render_model =
			(render_model_definition*)tag_get_fast(render_model_index);
		if (!render_model)
		{
			dbg.no_render_model++;
			continue;
		}

		// P1-2: td's tag-data cast gate — models with a non-manifold section pair cast nothing
		if (!stencil_shadow_model_is_manifold(render_model, object_iterator.get_index()))
		{
			dbg.non_manifold++;
			continue;
		}

		// FAITHFUL tag-debug semantics: a FINITE 2 world-unit extrusion (c[20].w, from
		// rasterizer_light_begin's 2.0f for the lightmap fake light -- see
		// k_stencil_shadow_extrusion_distance). The earlier "effectively infinite volumes,
		// long trails are the authentic look" reading was wrong: it came from noting that
		// rasterizer_light_submit uploads no separate distance constant, before the microcode
		// showed the distance is the .w of the light block it uploads.
		// F6 cycles alternative distances for diagnostics only.
		//
		// it. 627: ~26 lines describing the DYNAMIC per-caster extrusion experiment (it. 523/524/526)
		// used to sit here, immediately above this reach code, long after it. 604/605 deleted the mode.
		// It read as though it described what follows. The reasoning is preserved where it belongs —
		// td-do-not-fix.md entry 15, with the shared fatal property of every finite-cap variant: any
		// finite extrusion puts the far cap somewhere in the scene, so it RELOCATES the artefact
		// instead of removing it. Reach-clip exists because it is the one formulation that does not.
		// it. 625: the reach encode — receiver trace, slope fit and camera basis — moved to
		// rasterizer_dx9_stencil_shadow_reach.cpp. `reach_extrusion` still means "mode is
		// selected AND the SM3 pair exists", which is what picks the extrusion distance below.
		const bool reach_extrusion = stencil_shadow_reach_encode(
			object, object_iterator.get_index(), toward_light_world,
			stencil_shadow_debug_extrusion_override(), stencil_shadow_sm3_vertex_shader_ready());
		// it. 534: cleared for EVERY caster, so a plane from the previous caster (or the previous mode)
		// can never leak into one that should not be clipped. Zero xyz disables it in the shader.
		// it. 557: reach mode is checked FIRST and by its OVERRIDE value, not by `reach_extrusion`.
		// The two differ when SM3 is missing, and the difference matters: the sentinel (-3) would
		// otherwise fall through into the generic branch and be used as a literal -3 wu extrusion.
		const bool reach_mode_selected =
			stencil_shadow_debug_extrusion_override() == k_stencil_shadow_reach_extrusion;
		real32 extrusion_distance =
			reach_mode_selected
				? (reach_extrusion ? k_stencil_shadow_reach_extrusion_distance
					: k_stencil_shadow_extrusion_distance)
			: (stencil_shadow_debug_extrusion_override() != 0.f)
				? stencil_shadow_debug_extrusion_override() : k_stencil_shadow_extrusion_distance;

		// it. 605: the CLIPPED per-caster probe block was here (it. 530/541). Deleted with the mode;
		// its failure reasoning is in td-do-not-fix.md entry 15.

		// per-section facing bits retained for the cross-quad (seam stitch) pass
		// cap: k_stencil_shadow_max_cross_sections, rasterizer_dx9_stencil_shadow_tunables.h
		static uint32 facing_scratch[k_stencil_shadow_max_cross_sections][k_stencil_shadow_facing_bitvector_words];
		s_stencil_shadow_section* cross_shadows[k_stencil_shadow_max_cross_sections] = {};
		const real_matrix4x3* cross_matrices[k_stencil_shadow_max_cross_sections] = {};
		// Backing store for composed static transforms published into cross_matrices. The composed
		// matrix is built per loop iteration, so pointing cross_matrices at a loop local would
		// dangle by the time the cross pass reads it AFTER the loop. (The previous code was safe
		// only because `model_matrix` pointed into engine memory.)
		static real_matrix4x3 cross_matrix_storage[k_stencil_shadow_max_cross_sections];
		// it. 543 — DENSE cross slots. The arrays above used to be indexed by the MODEL's section index,
		// which overflows: the run reports *"section INDEX 107 is past the 64-slot seam-stitch array
		// (only 1 sections drew)"*. The array was 98% empty at the moment it overflowed, and every seam
		// touching such a section was paired by it. 542 and then silently never drawn — which is why
		// restoring the producer changed nothing on screen.
		//
		// Storage is now packed 0..N-1 in draw order, and the READER maps section -> slot through the
		// table below. Doing the remap at the reader (rather than having the producer emit dense slots)
		// keeps the per-model cross cache valid: cached quads keep stable SECTION indices, which do not
		// shift when a different LOD or permutation changes which sections draw. That is the hazard
		// it. 505 identified in the "producer emits dense slots" alternative.
		uint32 cross_count = 0;
		int16 dense_of_section[256];
		for (int32 i = 0; i < 256; i++) { dense_of_section[i] = -1; }
		// reverse map, so the producer can emit stable SECTION indices while iterating dense storage
		int16 section_of_dense[k_stencil_shadow_max_cross_sections];
		for (int32 i = 0; i < k_stencil_shadow_max_cross_sections; i++) { section_of_dense[i] = -1; }

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
				// level that exists -- shadows do not need detail.
				//
				// it. 538: THE OLD COMMENT ALSO CLAIMED "this cannot pick up more geometry than the
				// object renders". That was an assertion, never verified, and it is the wrong
				// reassurance either way: the risk is not *more* geometry but **different** geometry.
				// If the engine renders LOD 3 and we substitute LOD 0, the shadow is cast from a mesh
				// the engine never draws — which presents exactly as the user's report, *"floating
				// pieces that don't actually have geometry that is visible"*.
				//
				// It was also SILENT, so there was no way to tell whether it fires at all. Now counted
				// and reported once per map with the LOD actually substituted.
				section_index = NONE;
				int32 fallback_level = NONE;
				for (int32 level = 0; level < 6 && section_index == NONE; level++)
				{
					section_index = lod_sections[level];
					fallback_level = level;
				}
				g_stencil_shadow_lod_fallbacks++;
				if (!g_stencil_shadow_warned_lod_fallback && section_index != NONE)
				{
					g_stencil_shadow_warned_lod_fallback = true;
					LOG_INFO_GAME("stencil WARNING: no cached LOD for this object (requested {}), substituted level {} — the shadow is cast from a mesh the engine may not be rendering (it. 538; suspect for 'shadows on geometry that isn't visible')",
						(int32)object_lod, fallback_level);
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
				dbg.no_matrix++;
				continue;
			}

			// it. 617 — INTERPOLATE THE STATIC PATH TOO.
			//
			// User-reported: a fast-moving object's volume runs AHEAD of the object. The direction is the
			// diagnosis. `object_get_node_matrix` returns the TICK pose, while the model is RENDERED at the
			// interpolated pose, which lerps previous->target and therefore LAGS the tick target. Raw =
			// target = ahead of what is drawn, so the volume leads the vehicle, and the gap scales with
			// velocity — invisible at rest, obvious at speed.
			//
			// The ARTICULATED path already adopts the interpolated matrix (it. 500, in
			// stencil_shadow_section_animate). This static twin was never given the same treatment, so RIGID
			// casters — vehicles, crates, most scenery — kept the tick pose while skinned ones followed the
			// render pose. Vehicles are exactly the fast-moving case, which is why it shows up on a Banshee.
			//
			// Same contract as it. 500: call unconditionally, adopt only when the interpolator reports it ran.
			// It declines for objects that cannot interpolate, and when previous->target exceeds
			// `k_interpolation_distance_cutoff` (900.0, the teleport guard) — keeping the raw matrix there is
			// correct, and is what `object_try_get_node_matrix_interpolated` does.
			real_matrix4x3 interpolated_static;
			if (model_matrix
				&& halo_interpolator_interpolate_object_node_matrix(
					object_iterator.get_index(), (int16)section_node, &interpolated_static))
			{
				model_matrix = &interpolated_static;
			}
			if (shadow->articulated)
			{
				// P3: CPU-skin into world space, then everything is world-space — planes,
				// facing light, VB positions — drawn with identity node constants
				if (!stencil_shadow_section_animate(shadow, object_iterator.get_index(), render_model,
					region_index))
				{
					continue;
				}
				stencil_shadow_build_facing_bitvector(shadow, &toward_light_world, false, facing_bitvector);

				// it. 660 — the c50 palette for the GPU-skinned draw. Static buffer, not stack: 201
				// float4s, and this hook is single-threaded render code. Built per (caster, section);
				// count 0 means the CPU path draws from the dynamic VB exactly as before — and the
				// animate above still refreshed that VB in that case (it skips the refresh only when
				// the skinned pair is ready, the same condition the draw checks).
				static real_vector4d articulated_palette[201];
				int32 articulated_palette_count = 0;
				const bool gpu_pose = shadow->skinned_vb && stencil_shadow_skinned_ready();
				if (gpu_pose)
				{
					articulated_palette_count = stencil_shadow_pool_build_palette(
						object_iterator.get_index(), render_model, region_index, shadow,
						articulated_palette);
					if (articulated_palette_count == 0)
					{
						// The animate above skipped the dynamic-VB refresh on this same gpu_pose
						// condition, so the CPU fallback would draw a STALE pose. A missing shadow
						// for a frame beats a wrongly-posed one — skip, throttled-visible.
						static uint32 palette_failed_log = 0;
						if ((palette_failed_log++ % 600) == 0)
						{
							LOG_INFO_GAME("stencil WARNING: skinned palette build failed for section {} — section skipped this frame (count {})",
								section_index, palette_failed_log);
						}
						continue;
					}
				}

				// it. 541: the per-caster plane set before the section loop is used for every section —
				// see the static path below for why per-section clipping is the wrong granularity while
				// stitching is disabled.
				stencil_shadow_section_draw(shadow, facing_bitvector, &toward_light_world, false,
					NULL, extrusion_distance,
					shadow_opacity, k_stencil_shadow_self_shadow_bias,
					articulated_palette, articulated_palette_count);
				// it. 543: dense slot, not section index — see the declaration above.
				// it. 610: gated on the stitch flag. This bookkeeping exists ONLY to feed the
				// cross-quad pass, which has been off since it. 561, so with stitching disabled it was
				// pure per-section waste that also emitted a misleading overflow warning ("its seams
				// will not be bridged" — true, but no seams are bridged either way).
				// it. 610: `if constexpr`, not `if` — the flag is a compile-time constant and a plain
				// `&&` against it trips C4127, which /WX promotes to an error.
				if constexpr (k_stencil_shadow_stitch_seams)
				if (cross_count < k_stencil_shadow_max_cross_sections && section_index >= 0 && section_index < 256)
				{
					dense_of_section[section_index] = (int16)cross_count;
					section_of_dense[cross_count] = (int16)section_index;
					memcpy(facing_scratch[cross_count], facing_bitvector,
						((shadow->plane_count + 31) / 32) * sizeof(uint32));
					cross_shadows[cross_count] = shadow;
					cross_matrices[cross_count] = NULL;
					cross_count++;
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

						UNREFERENCED_PARAMETER(bind_offset);

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
						
						UNREFERENCED_PARAMETER(bind_rotation_deg);

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

				// it. 655/658 — THE POOL FIRST. The engine already composed this exact product
				// (`interpolated_node_world x inverse_bind`, 0x77DD88) for every node of every
				// visible object, and td's own static draw reads the same accessor from the same
				// pool (the reference note below). One read replaces the interpolate + multiply,
				// and carries the render-time corrections (it. 487) the multiply below cannot.
				// On Vista content the pool is region palettes in local node_map order (it. 657),
				// hence the region index and the slot translation. Invalid entry (off-screen,
				// cinematic-lit, first-person, LOD-divergent palette) falls through to the
				// unchanged composition.
				bool composed_from_pool = false;
				if (k_stencil_shadow_use_skinning_pool)
				{
					s_stencil_shadow_pool_ref pool_ref = {};
					if (stencil_shadow_pool_resolve(object_iterator.get_index(), render_model,
						region_index, shadow, &pool_ref))
					{
						const int32 pool_slot = stencil_shadow_pool_slot_for_node(
							&pool_ref, shadow, (int32)section_node);
						if (pool_slot != NONE)
						{
							model_skinning_get_node_matrix(
								pool_ref.pool, (int16)pool_slot, (real32*)&composed_static);
							stencil_shadow_pool_parity_probe(object_iterator.get_index(),
								(int32)section_node, render_model, &composed_static);
							draw_matrix = &composed_static;
							composed_from_pool = true;
						}
					}
				}

				if (composed_from_pool)
				{
					// pool matrix adopted above
				}
				else if (section_node >= 0 && (int32)section_node < render_model->nodes.count)	/* it. 443: `>= 0` guards a corrupt non-sentinel negative rigid_node (int16) from indexing nodes[-2] — an OOB READ feeding a matrix multiply, i.e. a plausible wrong transform, not a clean crash. -1 is already filtered upstream (section_node defaults to 0), so this is unreachable on valid data (it. 435 census: rigid_node is -1 or a real index) and is guarded only on the it. 347 precedent for user-modified maps. Routes corrupt data into the warned fallback below. */
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
				// it. 541 REVERTED it. 540's per-section ray here. Per-section is the WRONG granularity
				// for clipping, for a structural reason:
				//
				//   Adjacent sections that clip to DIFFERENT planes end their volumes at different
				//   depths, so they no longer meet along their shared edge. With cross-section stitching
				//   disabled those seams are already only partially sealed by the sentinel (it. 457/467),
				//   and a per-section plane turns "partially sealed" into "guaranteed mismatched" at
				//   EVERY seam — the counts stop cancelling and shadow goes missing.
				//
				// The evidence was decisive: in draw mode 1 the volume is a clean, coherent slab, while
				// mode 2 shows fragments. The GEOMETRY is right and the COUNTING is wrong, which is a
				// closure problem, not an extrusion problem. And the user saw *less* shadow after
				// it. 540, exactly as more seam mismatch predicts.
				//
				// A single per-caster plane keeps every section ending at a common depth, so seams stay
				// aligned. That is strictly better until stitching is restored (it. 518/519), at which
				// point per-section becomes viable again — the bridge quads would close the mismatch.
				stencil_shadow_section_draw(shadow, facing_bitvector, &toward_light_world, false,
					draw_matrix,
					extrusion_distance,
					shadow_opacity);
				// it. 543: dense slot, not section index — see the declaration above.
				// it. 610: gated on the stitch flag. This bookkeeping exists ONLY to feed the
				// cross-quad pass, which has been off since it. 561, so with stitching disabled it was
				// pure per-section waste that also emitted a misleading overflow warning ("its seams
				// will not be bridged" — true, but no seams are bridged either way).
				// it. 610: `if constexpr`, not `if` — the flag is a compile-time constant and a plain
				// `&&` against it trips C4127, which /WX promotes to an error.
				if constexpr (k_stencil_shadow_stitch_seams)
				if (cross_count < k_stencil_shadow_max_cross_sections && section_index >= 0 && section_index < 256)
				{
					dense_of_section[section_index] = (int16)cross_count;
					section_of_dense[cross_count] = (int16)section_index;
					memcpy(facing_scratch[cross_count], facing_bitvector,
						((shadow->plane_count + 31) / 32) * sizeof(uint32));
					cross_shadows[cross_count] = shadow;
					cross_matrix_storage[cross_count] = *draw_matrix;
					cross_matrices[cross_count] = &cross_matrix_storage[cross_count];
					cross_count++;
				}
			}
			// Fourth silent cap (see td-INDEX.md). Sections at index >= k_stencil_shadow_max_cross_sections
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
			// vehicles <= 9), at ZERO extra memory. Raising `k_stencil_shadow_max_cross_sections` — the obvious
			// reading of this message — does not address the cause and still overflows for any
			// model whose section index exceeds the new value.
			// it. 610: only meaningful when stitching is ON. With it off (it. 561) the message stated a
			// consequence that never applied — nothing bridges seams either way — and read as a live
			// defect in an otherwise clean log.
			if constexpr (k_stencil_shadow_stitch_seams)
			if (section_index >= k_stencil_shadow_max_cross_sections)
			{
				if (!g_stencil_shadow_warned_cross_cap)
				{
					g_stencil_shadow_warned_cross_cap = true;
					LOG_INFO_GAME("stencil WARNING: section INDEX {} is past the {}-slot seam-stitch array (only {} sections drew) — sparse indexing, not too many sections; its seams will not be bridged",
						section_index, (int32)k_stencil_shadow_max_cross_sections, sections_drawn + 1);
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
			// it. 571 — THE STITCH GATE MUST WRAP ONLY THE STITCHING.
			//
			// it. 561 put `&& k_stencil_shadow_stitch_seams` on the OUTER condition, which also gated
			// `volumes_drawn++` at the bottom of this block. Disabling stitching therefore left
			// volumes_drawn at 0 -> `g_stencil_shadow_mask_pending` false -> `stencil_shadow_world_darken`
			// early-returns -> the mode-0 apply never runs. Shadows vanished from the SHIPPING path while
			// draw mode 2 kept working, because mode 2 calls apply directly and never consults
			// mask_pending. That asymmetry is what made it look like a reach-clip problem for six
			// iterations.
			if (k_stencil_shadow_stitch_seams)
			{
			// bridge matched seams between this object's sections (td shared-edge stitches)
			// it. 542: the pairing needs the sections actually built for this caster — it reads their
			// boundary quads and bind-pose positions, and retags the matched ones in place.
			s_stencil_shadow_model_cross* cross =
				stencil_shadow_model_cross_get(render_model_index, render_model,
					cross_shadows, section_of_dense, (int32)cross_count);
			if (cross && !cross->quads.empty())
			{
				static std::vector<uint16> cross_indices;
				bool owner_handled[k_stencil_shadow_max_cross_sections] = {};
				for (uint32 seed = 0; seed < cross->quads.size(); seed++)
				{
					// it. 543: quads carry SECTION indices; storage is dense. Map before every use.
					const uint8 owner = cross->quads[seed].owner_section;
					const int16 owner_slot = dense_of_section[owner];
					if (owner_slot < 0 || owner_handled[owner_slot] || !cross_shadows[owner_slot])
					{
						continue;
					}
					owner_handled[owner_slot] = true;
					cross_indices.clear();
					for (uint32 entry_index = 0; entry_index < cross->quads.size(); entry_index++)
					{
						const s_stencil_shadow_cross_quad* entry = &cross->quads[entry_index];
						const int16 partner_slot = dense_of_section[entry->partner_section];
						if (entry->owner_section != owner
							|| partner_slot < 0
							|| !cross_shadows[partner_slot])
						{
							continue;
						}
						const uint32* owner_bits = facing_scratch[owner_slot];
						const uint32* partner_bits = facing_scratch[partner_slot];
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
						stencil_shadow_draw_cross_indices(cross_shadows[owner_slot], cross_indices,
							&toward_light_world, cross_matrices[owner_slot], extrusion_distance, shadow_opacity);
					}
				}
			}

			}	// it. 571: end of the stitching-only gate

			// P1-1: no per-object darken here. td applies ONCE, fullscreen, after all
			// volumes are laid (render_layer_lightmap_diffuse, td 0x10D8F0) --
			// stencil_shadow_world_darken does that. The per-object scissored apply that
			// used to live here existed only to contain the P0-1 streaks, and it darkened
			// overlapping casters more than once.
			//
			// it. 571: this MUST stay outside the stitch gate — it is what tells the darken pass a
			// caster was laid down (`mask_pending = volumes_drawn > 0`).
			volumes_drawn++;
		}
		else
		{
			dbg.no_sections++;
		}
	}

	// Silent cap: the iteration loop stops at k_stencil_shadow_max_casters_per_frame, so any further
	// casters are dropped ENTIRELY -- they lose their shadow rather than getting a truncated
	// one. tag-debug has no such limit (it walks every visible object). Warn once so a busy
	// scene missing shadows on the objects enumerated last is diagnosable.
	if (volumes_drawn >= k_stencil_shadow_max_casters_per_frame)
	{
		if (!g_stencil_shadow_warned_caster_cap)
		{
			g_stencil_shadow_warned_caster_cap = true;
			LOG_INFO_GAME("stencil WARNING: hit the {}-caster frame cap; later casters drew no shadow this frame",
				(int32)k_stencil_shadow_max_casters_per_frame);
		}
	}

	stencil_shadow_stats_report(&dbg, volumes_drawn, g_stencil_shadow_masking_pass,
		g_stencil_shadow_lod_fallbacks);

	if (g_stencil_shadow_masking_pass)
	{
		g_stencil_shadow_mask_pending = volumes_drawn > 0 && stencil_shadow_debug_draw_mode() == 0;
	}
}

// Tag-debug pass 6: lay stencil counts for all object volumes between the
// lightmap-indirect and SH-PRT layers (called from the native render_scene).
// it. 647 DIAGNOSTIC — seeded from the tunable, mutable so it can be flipped live. See the constant.
static bool g_stencil_shadow_lightmap_tier_enabled = k_stencil_shadow_lightmap_tier_enabled;

void stencil_shadow_lightmap_volumes_pass(void)
{
	// red mode (1) also draws HERE, not at the late hook: mid-scene c0-c3 is the current
	// window wvp, while after render_lights_new the lights path leaves stale transforms
	// (volumes rendered out of place / clipping with distance). Matches tag-debug, whose
	// colorwrite-debug byte draws during pass 6 itself.
	g_stencil_shadow_mask_pending = false;
	// it. 647 DIAGNOSTIC: suppress this tier entirely so the environment tier can be seen alone.
	//
	// Read through a mutable global rather than the constant directly. Testing the constant in a
	// runtime `if` is C4127 and /WX makes that an error (it. 621's trap, hit again here); an
	// `if constexpr` early return is C4702 for the same reason. The variable dodges both AND is
	// flippable from a debugger mid-session, which is worth having while bringing a tier up.
	if (!g_stencil_shadow_lightmap_tier_enabled || !stencil_shadow_active() || !cache_file_is_loaded())
	{
		return;
	}
	IDirect3DDevice9Ex* device = rasterizer_dx9_device_get_interface();
	if (!device)
	{
		return;
	}

	// it. 560/561 — REACH CLIP RESOURCE CONFLICT.
	//
	// On SM3 the engine creates the z target as a COLOUR TEXTURE and renders depth into it during
	// the lightmap-indirect stage (rasterizer_dx9_targets.cpp:810-811), and
	// `global_d3d_surface_render_z_as_target_z` is surface level 0 of that same texture (:834-836).
	// targets.cpp:145 keeps it bound as an MRT output for as long as
	// `g_dx9_dont_draw_to_depth_target_if_mrt_is_used` is false — which render.cpp:408 clears before
	// lightmap_indirect and only re-sets at :448, AFTER this pass. So during our draws the surface we
	// want to SAMPLE is still BOUND AS A TARGET, which D3D9 forbids.
	//
	// It fails silently and misleadingly: undefined reads come back ~0, so `receiver - caster` is
	// negative, `reach - delta` stays positive, texkill never fires, and the volume renders as pure
	// unbounded infinite extrusion — indistinguishable from "the reach constant is too large"
	// (CONFIRMED on the it. 558 build: reach active, leak unchanged).
	//
	// Suppressing the MRT z output for the duration of this pass is safe for the stencil counting:
	// on SM3 that surface is a COLOUR output carrying depth-as-colour, NOT the depth-stencil buffer
	// z-fail depends on — that one is separate and untouched. (In the non-SM3 path the same global
	// IS a depth-stencil surface, targets.cpp:846, which is why this had to be read rather than
	// assumed.) Re-applying the target is what actually rebinds without it, exactly as render.cpp:448
	// relies on the next set_target to do.
	const bool reach_mode_needs_depth_texture =
		stencil_shadow_debug_extrusion_override() == k_stencil_shadow_reach_extrusion
		&& stencil_shadow_reach_shader_ready() && stencil_shadow_sm3_vertex_shader_ready();
	const bool saved_suppress_z_target = g_dx9_dont_draw_to_depth_target_if_mrt_is_used;
	if (reach_mode_needs_depth_texture && !saved_suppress_z_target)
	{
		g_dx9_dont_draw_to_depth_target_if_mrt_is_used = true;
		rasterizer_dx9_set_target((e_rasterizer_target)*rasterizer_dx9_main_render_target_get(), 0, true);
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
	if (!dumped_env_ps && stencil_shadow_active())
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

	if (stencil_shadow_debug_draw_mode() != 1)
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
	if (stencil_shadow_debug_draw_mode() == 2)
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
	if (++perf_samples >= 4800)	// it. 554 MEASURED ~580 passes/s, i.e. ~1.2 per frame, not 8
	{
		LOG_INFO_GAME("stencil perf: volumes pass avg={:.3f}ms max={:.3f}ms over {} passes",
			perf_accum_ms / perf_samples, perf_max_ms, perf_samples);
		perf_accum_ms = 0.0;
		perf_max_ms = 0.0;
		perf_samples = 0;
	}

	// it. 561: hand the MRT z output back exactly as we found it. Restoring the flag alone is not
	// enough — the binding only changes on the next set_target, so re-apply it here rather than
	// leaving the rest of the frame drawing without its depth-as-colour output.
	if (reach_mode_needs_depth_texture && !saved_suppress_z_target)
	{
		g_dx9_dont_draw_to_depth_target_if_mrt_is_used = saved_suppress_z_target;
		rasterizer_dx9_set_target((e_rasterizer_target)*rasterizer_dx9_main_render_target_get(), 0, true);
	}
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
	if (!stencil_shadow_active() || !g_stencil_shadow_mask_pending)
	{
		return;
	}

	// mode 0 is the shipping path — the tier's single application.
	if (stencil_shadow_debug_draw_mode() == 0)
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
	if (g_stencil_shadow_skinned_declaration)
	{
		g_stencil_shadow_skinned_declaration->Release();
		g_stencil_shadow_skinned_declaration = NULL;
	}
	if (g_stencil_shadow_skinned_shader)
	{
		g_stencil_shadow_skinned_shader->Release();
		g_stencil_shadow_skinned_shader = NULL;
	}
	if (g_stencil_shadow_skinned_shader_sm3)
	{
		g_stencil_shadow_skinned_shader_sm3->Release();
		g_stencil_shadow_skinned_shader_sm3 = NULL;
	}
	stencil_shadow_reach_shader_dispose();
}

bool stencil_shadow_skinned_ready(void)
{
	// The SM3 twin is required whenever SM3 is in play at all: with reach or stipple active the
	// pixel shader is ps_3_0, and a vs_2_0 skinned shader beside it is an illegal pairing. Rather
	// than track the per-draw pixel-shader choice here, require the pair on SM3 hardware outright —
	// on SM2 hardware the vs_2_0 shader alone suffices because no ps_3_0 can ever be bound.
	if (!g_stencil_shadow_skinned_declaration || !g_stencil_shadow_skinned_shader)
	{
		return false;
	}
	if (rasterizer_globals_get()->d3d9_sm3_supported && !g_stencil_shadow_skinned_shader_sm3)
	{
		return false;
	}
	return true;
}
