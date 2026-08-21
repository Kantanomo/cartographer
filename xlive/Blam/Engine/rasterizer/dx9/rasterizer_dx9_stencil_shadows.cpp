#include "stdafx.h"
#include "rasterizer_dx9_stencil_shadows.h"

#include "rasterizer_dx9.h"
#include "rasterizer_dx9_errors.h"			// rasterizer_dx9_log_hr - the decoded-HRESULT report
#include "rasterizer_dx9_main.h"
#include "rasterizer_dx9_stencil_shadow_tunables.h"
#include "rasterizer_dx9_stencil_shadow_reach.h"
#include "rasterizer_dx9_stencil_shadow_skinning.h"	// the per-map reset stencil_shadow_cache_clear calls
#include "rasterizer_dx9_stencil_shadow_debug_view.h"
#include "rasterizer_dx9_stencil_shadow_lightmap_tier.h"
#include "render/render_stencil_shadow_casters.h"
#include "render/render_stencil_shadow_environment.h"	// the shared light-reaches-bounds predicate
#include "geometry/geometry_definitions_new_runtime.h"
#include "render/render.h"					// global_frame_index_get - the per-frame apply guard
#include "render/render_lights.h"			// render_lights_reset_diagnostics - the tier's per-map latch
#include "rasterizer/rasterizer_globals.h"
#include "networking/network_event.h"
#include "Util/Hooks/Hook.h"
#include "vertex_shaders_dx9/preprocessed_hlsl_from_tool/compiled/shadow_extrude.h"
#include "vertex_shaders_dx9/preprocessed_hlsl_from_tool/compiled/shadow_extrude_vs30.h"
#include "vertex_shaders_dx9/preprocessed_hlsl_from_tool/compiled/shadow_extrude_skinned.h"
#include "vertex_shaders_dx9/preprocessed_hlsl_from_tool/compiled/shadow_extrude_skinned_vs30.h"
#include "vertex_shaders_dx9/preprocessed_hlsl_from_tool/compiled/shadow_solid.h"
#include "vertex_shaders_dx9/preprocessed_hlsl_from_tool/compiled/shadow_stipple.h"
#include "shaders/compiled/shadow_reach_clip.h"

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

// The SM3 pair backing the stipple fade and the reach clip. D3D9 forbids mixing shader models, so
// a ps_3_0 needs this vs_3_0 beside it. OPTIONAL: creation failure leaves them NULL and the
// features that need them report unavailable rather than failing the draw.
static IDirect3DVertexShader9* g_stencil_shadow_vertex_shader_sm3 = NULL;
static IDirect3DPixelShader9* g_stencil_shadow_stipple_shader = NULL;

// The GPU-skinned pair: a 24-byte declaration (position + extrude + ubyte4 palette indices +
// ubyte4n weights) and the palette-blend extrusion shader in both shader models. Also optional -
// stencil_shadow_skinned_ready() reports false and articulated sections keep the CPU pose path.
static IDirect3DVertexDeclaration9* g_stencil_shadow_skinned_declaration = NULL;
static IDirect3DVertexShader9* g_stencil_shadow_skinned_shader = NULL;
static IDirect3DVertexShader9* g_stencil_shadow_skinned_shader_sm3 = NULL;

// A/B toggle for the silhouette quad winding, settable from a debugger. tag debug emits
// (2b, 2a, 2a+1, 2b+1) when the LEFT triangle faces the light; we emit the reverse cycle, so our
// side sheets wind inward where td's wind outward (td-caps-draw.md). Setting this reproduces td's
// ordering exactly. If same-winding pairing is ever implemented, re-derive which side the
// !same_winding suppression belongs on for the flipped case - it does not follow automatically.
static bool g_stencil_shadow_quad_winding_flip = false;

// Draw-side one-shot latches. FILE SCOPE on purpose, reset per map by stencil_shadow_cache_clear: as
// function-local statics these would cap per PROCESS, so only the first map of a session would ever
// be described. Every other module's latches live with their emitter and are reset by its own clear.
static bool g_stencil_shadow_warned_no_apply = false;
static bool g_stencil_shadow_warned_cross_draw_failed = false;
static bool g_stencil_shadow_warned_index_overflow_volume = false;
static bool g_stencil_shadow_warned_index_overflow_cross = false;

// Volume tint for the colour views. Points at CALLER-OWNED storage and is not copied; NULL selects
// this tier's own red. The dynamic and cluster tiers set it around their draws so their volumes are
// distinguishable from the lightmap tier's in the same frame.
static const real32* g_stencil_shadow_debug_tint_override = NULL;

/* prototypes */

static void stencil_shadow_reset_engine_gpu_state(void);

static void stencil_shadow_release_pipeline(IDirect3DDevice9Ex* device);

static void stencil_shadow_force_render_state(D3DRENDERSTATETYPE state, DWORD value);

static bool stencil_shadow_debug_color_view(void);

static bool stencil_shadow_shaders_initialize(IDirect3DDevice9Ex* device);

static void stencil_shadow_set_node_constants(const real_matrix4x3* model_matrix);

static void stencil_shadow_set_volume_render_states(bool color_view);

static void stencil_shadow_cull_facing_by_reach(
	const s_stencil_shadow_section* shadow,
	const real_point3d* light_position,
	real32 light_reach,
	uint32* bitvector);

static uint32 stencil_shadow_stage_volume_indices(
	const s_stencil_shadow_section* shadow,
	const uint32* facing_bitvector);

static uint32 stencil_shadow_stage_index_buffer(
	const uint16* indices,
	uint32 count,
	bool* warned_once);

/* public code */

void stencil_shadow_debug_tint_override_set(const real32* rgba_or_null)
{
	g_stencil_shadow_debug_tint_override = rgba_or_null;
}


void stencil_shadow_emit_silhouette_sheet(uint16 vert_a, uint16 vert_b, uint16* out_six)
{
	// (a0, b0, b1, a1) as two triangles - a0 leads both, so the pair shares the a0-b1 diagonal.
	const uint16 a0 = (uint16)(vert_a * 2), a1 = (uint16)(vert_a * 2 + 1);
	const uint16 b0 = (uint16)(vert_b * 2), b1 = (uint16)(vert_b * 2 + 1);
	out_six[0] = a0;
	out_six[1] = b0;
	out_six[2] = b1;
	out_six[3] = a0;
	out_six[4] = b1;
	out_six[5] = a1;
}

void stencil_shadow_build_facing_bitvector(
	const s_stencil_shadow_section* shadow,
	const real_point3d* light_position,
	bool point_light,
	uint32* out_bitvector,
	real32 light_reach)
{
	// Port of tag debug's rasterizer_stencilshadow_build_bitvector_from_rigid_groups: the sign of
	// dot(light, N) - minus d for point lights - packed LSB-first in 4-bit groups via SSE movemask
	// over the SoA 4-blocks, with completed words inverted so bit == 1 means the triangle FACES the
	// light.
	//
	// `planes_soa` is allocated for every validly built section, so the guard is defence against a
	// half-built one rather than a second code path; there is no scalar fallback.
	if (!shadow->planes_soa)
	{
		return;
	}

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
	// Full z-fail volume: silhouette quads (as triangle pairs) plus near and far caps, drawn with
	// two-sided stencil. Cap construction matches tag-debug's caps_internal (td 0x9C700), which
	// draws two batches from two index lists - near unextruded, far extruded. We encode the same
	// distinction in the indices themselves (2v selects the unextruded copy, 2v+1 the extruded one),
	// so one draw does what td does in two.
	IDirect3DDevice9Ex* device = rasterizer_dx9_device_get_interface();
	if (!device || !shadow->valid)
	{
		return;
	}

	uint32 ib_size = stencil_shadow_stage_volume_indices(shadow, facing_bitvector);
	if (ib_size == 0)
	{
		return;
	}


	if (!stencil_shadow_shaders_initialize(device))
	{
		return;
	}

	stencil_shadow_set_volume_render_states(stencil_shadow_debug_color_view());

	// The GPU-skinned draw: static bind-pose VB plus the palette at c50 replacing the single node
	// matrix. The silhouette indices come from CPU planes posed by the SAME matrices, so the drawn
	// vertices and the facing decision cannot disagree.
	const bool gpu_skin = shadow->skinned_vb && palette_rows && palette_matrix_count > 0
		&& stencil_shadow_skinned_ready();
	if (gpu_skin)
	{
		// Cache-coherent palette upload: mirror the rows into the engine's vertex-shader constant
		// cache before the device call, exactly as the engine's own palette upload does, so later
		// test_cache-based uploads reason from what c50.. actually holds. g_region_skinning_active
		// stays untouched - we never enter the engine's region-skinning state machine.
		memcpy(Memory::GetAddress<real32*>(0xA3C7B0) + k_stencil_shadow_node_constant * 4, palette_rows,
			(size_t)palette_matrix_count * 3 * sizeof(real_vector4d));
		device->SetVertexShaderConstantF(k_stencil_shadow_node_constant, (const real32*)palette_rows,
			palette_matrix_count * 3);
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
	// .y is the self-shadow bias - see shadow_extrude.fx.
	real32 extrusion_constant[4] =
		{ extrusion_distance, self_shadow_bias, 0.f, 0.f };
	device->SetVertexShaderConstantF(k_stencil_shadow_light_constant, light_constant, 1);
	device->SetVertexShaderConstantF(k_stencil_shadow_extrusion_distance_constant, extrusion_constant, 1);

	ib_size = stencil_shadow_stage_index_buffer(isq_globals.indices, ib_size,
		&g_stencil_shadow_warned_index_overflow_volume);
	if (ib_size == 0)
	{
		return;
	}

	device->SetVertexDeclaration(gpu_skin
		? g_stencil_shadow_skinned_declaration : g_stencil_shadow_vertex_declaration);
	// td stipple fade: fragments screen-door-clipped at the object's shadow opacity so
	// faded objects mark proportionally fewer stencil pixels (SM3 pair required)
	bool stipple = stencil_shadow_debug_draw_mode() != 1 && opacity < 0.995f
		&& g_stencil_shadow_vertex_shader_sm3 && g_stencil_shadow_stipple_shader;
	// Reach clip needs the same SM3 vertex shader and takes priority over stipple when both apply:
	// they share the single pixel-shader slot, and losing the fade costs less than losing the bound
	// this mode exists to test.
	//
	// !point_light: reach clip is a LIGHTMAP-tier formulation and must never bind for point-light
	// draws. Its constants are encoded by the lightmap pass for its directional light, and the depth
	// texture its pixel shader samples is only legally unbound inside that pass's MRT-suppression
	// window - during the lights phase it is still an MRT output, so the texkill reads undefined and
	// the counts die silently.
	const bool reach_clip = stencil_shadow_reach_is_active() && !point_light
		&& stencil_shadow_debug_draw_mode() != 1 && stencil_shadow_reach_shader_ready();
	// NEITHER SM3 POINTER CAN BE NULL HERE, and only one of the two guarantees is local. `stipple`
	// tests the pointer it needs on the line above; `reach_clip` does not, and its guarantee lives
	// three calls away - reach ACTIVATION is gated on the `sm3_vertex_ready` argument that the caster
	// loop fills from stencil_shadow_sm3_vertex_shader_ready(), so an active reach implies the shader.
	// The skinned pair is covered by stencil_shadow_skinned_ready() inside `gpu_skin`. Binding NULL
	// here would silently drop to fixed-function and stop extruding, so re-derive this before
	// changing how reach activates.
	device->SetVertexShader((stipple || reach_clip)
		? (gpu_skin ? g_stencil_shadow_skinned_shader_sm3 : g_stencil_shadow_vertex_shader_sm3)
		: (gpu_skin ? g_stencil_shadow_skinned_shader : g_stencil_shadow_vertex_shader));
	if (stencil_shadow_debug_color_view())
	{
		// The tint is per-CALLER - the dynamic tier overrides it (cyan-blue,
		// and the override alone activates the colour view for its mode-3 draws). NULL
		// override = the classic red (mode 1, the sun tier).
		const real32 red_tint[4] = { 1.f, 0.125f, 0.125f, 0.375f };
		const real32* tint = g_stencil_shadow_debug_tint_override
			? g_stencil_shadow_debug_tint_override : red_tint;
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
		device->SetPixelShaderConstantF(k_stencil_shadow_stipple_constant, stipple_constant, 1);
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

	HRESULT draw_result;
	rasterizer_dx9_log_hr(
		draw_result,
		device->DrawIndexedPrimitive(
			D3DPT_TRIANGLELIST,
			0,
			0,
			shadow->welded_vertex_count * 2,
			0,
			ib_size / 3)
	);
	stencil_shadow_force_render_state(D3DRS_DEPTHBIAS, 0);

	// Release sampler 0 through the CACHED setter, never the raw device: the reach path binds the
	// depth target there and forces point filtering, and a raw SetTexture(0, NULL) desyncs the
	// engine's redundancy cache - the next bind then no-ops while the engine believes s0 is
	// populated, which corrupts every later draw that samples it (observed as black wall decals).
	if (reach_clip)
	{
		stencil_shadow_reach_unbind();
	}

	if (FAILED(draw_result))
	{
		// The HRESULT itself is decoded by rasterizer_dx9_log_hr on the call above; this adds the
		// context that says WHICH draw and with what, which the statement text cannot.
		event(_event_error, "rasterizer:dx9:stencil:draw: FAILED indices=%u verts=%u mode=%d",
			ib_size, shadow->welded_vertex_count * 2,
			stencil_shadow_debug_draw_mode());
	}

	stencil_shadow_release_pipeline(device);
	return;
}


bool stencil_shadow_sm3_vertex_shader_ready(void)
{
	return g_stencil_shadow_vertex_shader_sm3 != NULL;
}

// Darken every pixel whose stencil count differs from the 128 midpoint, optionally scissored to a
// rect. Callers manage the stencil clears.
void stencil_shadow_apply_and_clear(real32 darkness, const RECT* scissor)
{
	IDirect3DDevice9Ex* device = rasterizer_dx9_device_get_interface();
	if (!device || !g_stencil_shadow_pixel_shader)
	{
		// The apply is the only stage that turns counts into pixels, so a failure here is silent:
		// every other diagnostic reads clean while nothing is ever darkened.
		if (!g_stencil_shadow_warned_no_apply)
		{
			g_stencil_shadow_warned_no_apply = true;
			event(_event_warning, "rasterizer:dx9:stencil:apply: skipped - device=%d pixel_shader=%d - NOTHING will be darkened",
				device ? 1 : 0, g_stencil_shadow_pixel_shader ? 1 : 0);
		}
		return;
	}

	// The tier applies ONCE per frame, unscissored: a non-zero scissor or a call count above one
	// would mean the per-caster darken had come back somewhere. Reported at the frame boundary for
	// the frame that just ended, so the tally is complete rather than however far the current frame
	// happens to have got.
	{
		static uint32 applies_this_frame = 0;
		static uint32 last_apply_frame = 0xFFFFFFFF;
		static uint32 apply_log_counter = 0;
		static real32 last_darkness = 0.f;
		static bool last_scissored = false;
		uint32 frame = *global_frame_index_get();
		if (frame != last_apply_frame)
		{
			if (last_apply_frame != 0xFFFFFFFF && (apply_log_counter++ % 600) == 0)
			{
				event(_event_verbose, "rasterizer:dx9:stencil:apply: darkness=%.2f scissor=%d mode=%d calls_in_frame=%u (expect scissor=0, calls=1)",
					last_darkness, last_scissored ? 1 : 0, stencil_shadow_debug_draw_mode(),
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
	stencil_shadow_force_render_state(D3DRS_STENCILENABLE, TRUE);
	stencil_shadow_force_render_state(D3DRS_TWOSIDEDSTENCILMODE, FALSE);
	stencil_shadow_force_render_state(D3DRS_STENCILREF, 128);	// tag-debug midpoint convention
	// Set the READ mask explicitly rather than inheriting it: the comparison is
	// (mask & stencil) != (mask & 128), so an inherited mask of 0 is false for every pixel and
	// darkens nothing with no error anywhere.
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


// Draw prebuilt cross-quad indices from the owner section's VB, under the same pipeline
// configuration as the volume draw. Caps stay per-section - stitches are silhouette sheets only,
// exactly like td's.
void stencil_shadow_draw_cross_indices(
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

	stencil_shadow_set_volume_render_states(false);

	stencil_shadow_set_node_constants(model_matrix);
	real32 light_constant[4] = { light_position->x, light_position->y, light_position->z, 0.f };
	// .y is the self-shadow bias - see shadow_extrude.fx.
	real32 extrusion_constant[4] =
		{ extrusion_distance, k_stencil_shadow_self_shadow_bias, 0.f, 0.f };
	device->SetVertexShaderConstantF(k_stencil_shadow_light_constant, light_constant, 1);
	device->SetVertexShaderConstantF(k_stencil_shadow_extrusion_distance_constant, extrusion_constant, 1);

	const uint32 index_count = stencil_shadow_stage_index_buffer(indices.data(), (uint32)indices.size(),
		&g_stencil_shadow_warned_index_overflow_cross);
	if (index_count == 0)
	{
		return;
	}

	device->SetVertexDeclaration(g_stencil_shadow_vertex_declaration);
	bool stipple = opacity < 0.995f
		&& g_stencil_shadow_vertex_shader_sm3 && g_stencil_shadow_stipple_shader;
	device->SetVertexShader(stipple
		? g_stencil_shadow_vertex_shader_sm3 : g_stencil_shadow_vertex_shader);
	if (stipple)
	{
		const real32 stipple_constant[4] = { opacity, 0.f, 0.f, 0.f };
		device->SetPixelShader(g_stencil_shadow_stipple_shader);
		device->SetPixelShaderConstantF(k_stencil_shadow_stipple_constant, stipple_constant, 1);
	}
	else
	{
		device->SetPixelShader(NULL);
	}
	device->SetStreamSource(0, shadow->shadow_vb, 0, sizeof(s_stencil_shadow_vertex));
	device->SetIndices(g_stencil_shadow_index_buffer);
	// Check the HRESULT even though stitching is disabled today. This function is the lean twin of the
	// volume draw and every fix to that one has had to be mirrored across by hand, so a seam draw that
	// silently contributed nothing while the volume draw's log stayed clean is the shape this system
	// is worst at debugging. D3D9 also fails a draw when the vertex declaration does not satisfy a
	// declared shader input, which makes this the runtime net for a declaration/shader type mismatch -
	// the one part of the vertex layout offsetof and the static_asserts cannot enforce.
	HRESULT cross_draw_result;
	rasterizer_dx9_log_hr(
		cross_draw_result,
		device->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, 0,
			shadow->welded_vertex_count * 2, 0, index_count / 3)
	);
	if (FAILED(cross_draw_result) && !g_stencil_shadow_warned_cross_draw_failed)
	{
		g_stencil_shadow_warned_cross_draw_failed = true;
		event(_event_error, "rasterizer:dx9:stencil:draw: CROSS FAILED indices=%u verts=%u",
			index_count, shadow->welded_vertex_count * 2);
	}
	stencil_shadow_release_pipeline(device);
}

void stencil_shadow_cache_clear(void)
{
	// Each module owns its own latches and clears them here, so a budget always sits beside the code
	// that spends it. The reach module is the one exception and is not missing from this list: its
	// budgets reset on every F6 press instead, because a per-map cap is spent in milliseconds at
	// several hundred fps and would only ever describe whatever was on screen when the mode opened.
	stencil_shadow_generation_cache_clear();
	stencil_shadow_skinning_reset_diagnostics();
	stencil_shadow_lightmap_tier_reset_diagnostics();
	render_stencil_shadow_casters_reset_diagnostics();
	render_lights_reset_diagnostics();

	// Draw-side diagnostic budgets are per map, not per process, so each map gets its own samples.
	g_stencil_shadow_warned_no_apply = false;
	g_stencil_shadow_warned_cross_draw_failed = false;
	g_stencil_shadow_warned_index_overflow_volume = false;
	g_stencil_shadow_warned_index_overflow_cross = false;
}


void rasterizer_dx9_stencil_shadows_apply_patches(void)
{
	// no engine byte patches: the draw is called directly from Cartographer's native
	// render_scene (render/render.cpp, after render_lights_new)
	event(_event_status, "rasterizer:dx9:stencil: initialized (drawing from native render_scene)");
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
	// than track the per-draw pixel-shader choice here, require the pair on SM3 hardware outright -
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

/* private code */

// Dirty the engine's gpu frontend cache (g_gpu_frontend @ halo2.exe 0xE4B040) after raw device
// binds - the engine's own post-mutation resync primitive (c_gpu_frontend::reset_state,
// halo2.exe 0x66E7D7). Covers ONLY streams + SetIndices(NULL) + field_FC.
static void stencil_shadow_reset_engine_gpu_state(void)
{
	INVOKE_TYPE(0x26E7D7, 0x0, void(__fastcall*)(void*, void*),
		Memory::GetAddress<void*>(0xA4B040), NULL);
}

// Hand the pipeline back to the engine after our raw binds. reset_state does NOT cover the
// cached vertex shader / declaration pointers (verified: c_gpu_frontend layout), so the engine
// would compare against stale pointers and SKIP re-binding - its draws would then run with OUR
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
//
// This is also why no pass here brackets itself in a D3D state block: on this device D3DSBT_ALL
// capture is unreliable (pure-device/9Ex) and its Apply() corrupts live state, bisected in-game.
// Every state goes through this wrapper instead, and stencil_shadow_release_pipeline hands the
// engine back a pipeline it will re-bind on its next draw.
static void stencil_shadow_force_render_state(D3DRENDERSTATETYPE state, DWORD value)
{
	rasterizer_dx9_set_render_state(state, value);
	IDirect3DDevice9Ex* force_device = rasterizer_dx9_device_get_interface();
	if (force_device)
	{
		force_device->SetRenderState(state, value);
	}
}

// The colour-volume view is active when F7 selects it, or when a caller holds the tint override -
// the other tiers draw their views under a base mode that reads as "real" to this one.
static bool stencil_shadow_debug_color_view(void)
{
	return stencil_shadow_debug_draw_mode() == 1
		|| g_stencil_shadow_debug_tint_override != NULL;
}


static bool stencil_shadow_shaders_initialize(IDirect3DDevice9Ex* device)
{
	// The guard covers EVERY resource the callers dereference, not just the first created: the index
	// buffer is created last, so testing only the shader and declaration would let one transient
	// CreateIndexBuffer failure early-return true forever with a NULL buffer the draw paths lock.
	// The optional pairs are excluded on purpose - every use site checks them.
	if (g_stencil_shadow_vertex_shader
		&& g_stencil_shadow_vertex_declaration
		&& g_stencil_shadow_index_buffer)
	{
		return true;
	}

	// Offsets via offsetof, not literals: the struct-size assert pins only the stride, so reordering
	// the members would still compile while the shader read `extrude` as `position.x`. The types must
	// agree with the shader's declarations by hand - the extrusion blob reads v0.xyz and v1.x.
	const D3DVERTEXELEMENT9 elements[] =
	{
		{ 0, (WORD)offsetof(s_stencil_shadow_vertex, position),
			D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
		{ 0, (WORD)offsetof(s_stencil_shadow_vertex, extrude),
			D3DDECLTYPE_FLOAT1, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
		D3DDECL_END()
	};
	static_assert(offsetof(s_stencil_shadow_vertex, position) == 0,
		"shadow vertex POSITION must be first; the extrusion shader reads it as v0.xyz");
	static_assert(offsetof(s_stencil_shadow_vertex, extrude) == 12,
		"shadow vertex extrude flag must follow position; the shader reads it as v1.x");
	// The four REQUIRED resources report through rasterizer_dx9_log_hr, which decodes the HRESULT:
	// losing any of them means the system never draws again, and it used to return false in silence.
	HRESULT hr;
	rasterizer_dx9_log_hr(
		hr,
		device->CreateVertexDeclaration(elements, &g_stencil_shadow_vertex_declaration)
	);
	if (FAILED(hr))
	{
		return false;
	}

	// vs_2_0 on purpose: the non-debug stencil-only path runs without a pixel shader,
	// and D3D9 forbids a SM3 vertex shader with fixed-function pixel processing.
	rasterizer_dx9_log_hr(
		hr,
		device->CreateVertexShader((const DWORD*)k_shadow_extrude_vs_2_0, &g_stencil_shadow_vertex_shader)
	);
	if (FAILED(hr))
	{
		return false;
	}
	rasterizer_dx9_log_hr(
		hr,
		device->CreatePixelShader((const DWORD*)k_shadow_solid_ps_2_0, &g_stencil_shadow_pixel_shader)
	);
	if (FAILED(hr))
	{
		return false;
	}

	rasterizer_dx9_log_hr(
		hr,
		device->CreateIndexBuffer(
			k_stencil_shadow_index_buffer_capacity * sizeof(uint16),
			D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY,
			D3DFMT_INDEX16,
			D3DPOOL_DEFAULT,
			&g_stencil_shadow_index_buffer,
			NULL)
	);
	if (FAILED(hr))
	{
		return false;
	}

	// The GPU-skinned pair, optional like the SM3 pair below: failures leave the pointers NULL and
	// articulated sections keep the CPU pose path.
	//
	// These deliberately do NOT go through rasterizer_dx9_log_hr. That macro reports a rasterizer
	// ERROR, and a machine without the shader model is a supported configuration rather than a fault -
	// filing it as one would put a normal state in the error report. `stencil_shadow_skinned_ready`
	// and `stencil_shadow_sm3_vertex_shader_ready` are how a caller asks whether they exist.
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

	// The SM3 pair for the stipple fade, and the reach clip that shares it. Optional: failures leave
	// the pointers NULL, faded objects cast full-strength, and reach disables only its own F6 mode.
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
		stencil_shadow_reach_shader_create(device);
	}
	return true;
}


// Per-object node transform through the engine's own upload (halo2.exe 0x662bd4): packs the engine
// row convention (row r = forward[r], left[r], up[r], position[r]) into c50-c52 and keeps the engine
// transform cache coherent - the same path render_visible_section_set_transform_constants (halo2.exe 0x680A68)
// uses for rigid sections. The volume shader then transforms like any engine model shader: node rows
// to world, inherited c0-c3 to clip.
static void stencil_shadow_set_node_constants(const real_matrix4x3* model_matrix)
{
	real_vector4d rows[3];
	if (model_matrix)
	{
		for (int32 row = 0; row < 3; row++)
		{
			// Scale applies to the BASIS only, matching how the engine packs a raw node matrix
			// (model_skinning_matrix_from_real_matrix4x3). Do not "correct" this against
			// render_visible_section_set_transform_constants, which has no scale multiply: its input
			// is a visible-section matrix that is already pre-scaled, while ours is a raw node matrix
			// with a live scale field. Dropping the multiply shrinks every volume on a scaled node.
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

// The volume pipeline configuration, in ONE place, because both volume draws must rasterize under
// identical rules: stencil_shadow_section_draw emits a section's silhouette sheets and caps, and
// stencil_shadow_draw_cross_indices bridges the seams BETWEEN sections. A bridge quad that
// rasterizes or depth-tests differently from the faces it closes against stops the counts
// cancelling, and light leaks along exactly the seams that used it. These were two hand-mirrored
// blocks; a state fixed in one and not the other was invisible until the seams misbehaved.
//
// Every state here fails SILENTLY and TOTALLY when wrong - no error, no warning, just a missing or
// wrong-sized shadow - which is why each is set explicitly rather than inherited from whatever pass
// the engine ran before us.
static void stencil_shadow_set_volume_render_states(bool color_view)
{
	if (color_view)
	{
		// Colour volume visualization: colour writes on, no stencil writes.
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
		// Real shadows: stencil count only, z-fail over closed volumes - correct from any camera
		// position, including inside the volume.
		stencil_shadow_force_render_state(D3DRS_COLORWRITEENABLE, 0);
		stencil_shadow_force_render_state(D3DRS_ALPHABLENDENABLE, FALSE);
		// Alpha test runs BEFORE the stencil stage, so an inherited enabled alpha test discards
		// volume fragments before any count is written - no shadow, no error. tag debug disables it
		// at layer open for the same reason.
		stencil_shadow_force_render_state(D3DRS_ALPHATESTENABLE, FALSE);
		stencil_shadow_force_render_state(D3DRS_STENCILENABLE, TRUE);
		stencil_shadow_force_render_state(D3DRS_TWOSIDEDSTENCILMODE, TRUE);
		stencil_shadow_force_render_state(D3DRS_STENCILREF, 0);
		stencil_shadow_force_render_state(D3DRS_STENCILMASK, 0xFFFFFFFF);
		stencil_shadow_force_render_state(D3DRS_STENCILWRITEMASK, 0xFFFFFFFF);
		stencil_shadow_force_render_state(D3DRS_STENCILFUNC, D3DCMP_ALWAYS);
		stencil_shadow_force_render_state(D3DRS_STENCILPASS, D3DSTENCILOP_KEEP);
		// D3D9's INCR/DECR ARE THE WRAPPING OPS - the saturating pair is INCRSAT/DECRSAT. This is the
		// reverse of the D3D10+/GL naming, where plain INCR clamps and INCR_WRAP wraps, so these two
		// lines read like the wrong choice to anyone carrying that habit. They are the right one, and
		// wrapping is what makes counting around the 128 midpoint work: a volume that pushes the count
		// past an end has to come back through it, which clamping would not do. tag debug set the same
		// semantics on NV2A (GL_INCR_WRAP 0x8507).
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
	// These passes OWN the scissor test bit rather than inheriting it, so anything that overrides the
	// RECT must save and restore it - the rect is device-persistent and the live variable here, and a
	// caster-sized rect left behind clips the engine's own later passes, not ours.
	stencil_shadow_force_render_state(D3DRS_SCISSORTESTENABLE, TRUE);
	// Inert, and set only so the configuration is complete: the retail engine never calls
	// SetClipPlane (swept for the vtable call in both encodings, zero matches), so plane 0 holds the
	// all-zero D3D9 default, which clips nothing whatever the enable says. The 1 that used to sit
	// here was a leftover from the deleted CLIPPED extrusion mode.
	stencil_shadow_force_render_state(D3DRS_CLIPPLANEENABLE, 0);
	stencil_shadow_force_render_state(D3DRS_FILLMODE, D3DFILL_SOLID);

	// Depth func and bias are LOAD-BEARING for z-fail counting, not cosmetic. With LESS, a near-cap
	// fragment sitting exactly at its caster's surface depth FAILS the depth test and therefore
	// COUNTS - tag debug relies on that, drawing caps from the model's own vertices so the depths
	// are bit-identical. LESSEQUAL (or a toward-camera bias) makes those fragments pass instead,
	// which removes the near cap's contribution and leaves the volume open at the light end.
	// Vista uses depth bias for decals, so an inherited bias is a live possibility, not a formality.
	stencil_shadow_force_render_state(D3DRS_ZFUNC, D3DCMP_LESS);
	stencil_shadow_force_render_state(D3DRS_DEPTHBIAS, 0);
	stencil_shadow_force_render_state(D3DRS_SLOPESCALEDEPTHBIAS, 0);
}

// LIGHT REACH CULL - clear the facing bit of any triangle beyond the light's radius so it generates
// no silhouette. This is our equivalent of tag debug's subclusters: it processes only the subclusters
// a light actually reaches, and without a partition every triangle of a cluster casts however far
// away it sits (one Metropolis cluster spans 1173 x 599 wu against a 5 wu flashlight).
//
// A triangle beyond the radius receives no light, so it can block none - clearing its bit is the
// correct answer, and the edges where an in-reach triangle meets an out-of-reach one become
// silhouettes that close the volume exactly where the light stops.
//
// Guarded on !articulated: for those, base_positions is the bind pose while the light is world space.
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
	for (uint32 plane_index = 0; plane_index < shadow->plane_count; plane_index++)
	{
		const uint32 mask = 1u << (plane_index & 31);
		if ((bitvector[plane_index >> 5] & mask) == 0)
		{
			continue;		// already not facing - nothing to clear
		}
		// A CORNER test over-culls large triangles, verified live: a 0.8wu muzzle light over outdoor
		// ground culled facing 210 -> 0, because the ground under the light has all three corners
		// metres away while the light plainly hits it. Test the light against the triangle's AABB
		// instead - the same predicate the cluster tier applies to a whole room - which keeps any
		// triangle whose SURFACE can enter the lit sphere.
		const uint16* triangle = &shadow->triangles[plane_index * 3];
		const real_point3d* p0 = &shadow->base_positions[triangle[0]];
		const real_point3d* p1 = &shadow->base_positions[triangle[1]];
		const real_point3d* p2 = &shadow->base_positions[triangle[2]];
		real_rectangle3d bounds;
		bounds.x0 = MIN(p0->x, MIN(p1->x, p2->x));
		bounds.x1 = MAX(p0->x, MAX(p1->x, p2->x));
		bounds.y0 = MIN(p0->y, MIN(p1->y, p2->y));
		bounds.y1 = MAX(p0->y, MAX(p1->y, p2->y));
		bounds.z0 = MIN(p0->z, MIN(p1->z, p2->z));
		bounds.z1 = MAX(p0->z, MAX(p1->z, p2->z));
		if (!stencil_shadow_light_touches_bounds(&bounds, light_position, light_reach))
		{
			bitvector[plane_index >> 5] &= ~mask;
		}
	}
}

// Stage the volume's indices: silhouette quads as triangle pairs, then the near and far caps that
// close it for z-fail counting. The doubled indices carry the extrusion decision - 2v selects a
// welded vertex's unextruded copy, 2v+1 the extruded one - so one draw does what tag debug's
// caps_internal (td 0x9C700) does in two batches from two index lists.
//
// Cap convention is tag debug's: the front cap is the light-facing triangles at their original
// positions, the back cap the NON-facing ones at extruded positions, whose winding already faces
// outward at the far end. Returns the index count, or 0 when this section contributes no volume.
static uint32 stencil_shadow_stage_volume_indices(
	const s_stencil_shadow_section* shadow,
	const uint32* facing_bitvector)
{
	// silhouette index generation (doubled indices: 2v = original, 2v+1 = extruded)

	uint32 ib_size = 0;

	for (uint32 quad_index = 0; quad_index < shadow->quad_count; quad_index++)
	{
		const s_stencil_shadow_quad* quad = &shadow->quads[quad_index];
		// The matched-boundary skip must precede the facing reads: the sentinel would index far
		// past the end of the bitvector.
		if (quad->tri_right == k_stencil_shadow_matched_boundary)
		{
			continue;	// seam bridged by the cross-quad pass
		}
		bool left_faces = BIT_VECTOR_TEST_FLAG(facing_bitvector, quad->tri_left);
		bool right_faces = quad->tri_right != k_stencil_shadow_boundary_triangle && BIT_VECTOR_TEST_FLAG(facing_bitvector, quad->tri_right);

		if (left_faces == right_faces)		// no silhouette crossing at this edge
		{
			continue;
		}

		// Bound the staging write before it lands: isq_globals is a global, so an overrun corrupts
		// the D3D handles behind it. Model sections cannot reach the cap, cluster-scale ones can.
		if (ib_size + 6 > k_stencil_shadow_index_buffer_capacity)
		{
			if (!g_stencil_shadow_warned_index_overflow_volume)
			{
				g_stencil_shadow_warned_index_overflow_volume = true;
				event(_event_warning, "rasterizer:dx9:stencil:draw: index staging FULL at silhouette quad %u (ib_size %u) - volume truncated",
					quad_index, ib_size);
			}
			break;
		}

		// Order the quad so its front faces away from the lit triangle; same-winding source pairs
		// never swap, since both sides want the stored orientation.
		uint16 vert_a = quad->vert_a;
		uint16 vert_b = quad->vert_b;
		bool same_winding = shadow->quad_same_winding_bits && BIT_VECTOR_TEST_FLAG(shadow->quad_same_winding_bits, quad_index);

		bool swap_side = g_stencil_shadow_quad_winding_flip ? left_faces : right_faces;
		if (swap_side && !same_winding)
		{
			uint16 swap = vert_a;
			vert_a = vert_b;
			vert_b = swap;
		}

		stencil_shadow_emit_silhouette_sheet(vert_a, vert_b, &isq_globals.indices[ib_size]);
		ib_size += 6;
	}

	// caps close the volume for z-fail counting (tag-debug convention: front cap =
	// light-facing triangles at original positions; back cap = NON-facing triangles at
	// extruded positions - their winding already faces outward at the far end)
	for (uint32 triangle_index = 0; triangle_index < shadow->plane_count; triangle_index++)
	{
		const uint16* triangle = &shadow->triangles[triangle_index * 3];
		bool faces = BIT_VECTOR_TEST_FLAG(facing_bitvector, triangle_index);
		if (ib_size + 3 > k_stencil_shadow_index_buffer_capacity)
		{
			if (!g_stencil_shadow_warned_index_overflow_volume)
			{
				g_stencil_shadow_warned_index_overflow_volume = true;
				event(_event_warning, "rasterizer:dx9:stencil:draw: index staging FULL at cap triangle %u (ib_size %u) - volume truncated",
					triangle_index, ib_size);
			}
			break;
		}
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
	return ib_size;
}

// Stage indices into the shared index buffer. Returns the count actually written, or 0 when the
// buffer could not be locked and the caller must not draw.
//
// The cap is reachable in principle: the plane cap admits 32767 triangles, and a mesh with no shared
// edges yields up to 3T edges -> 3 * 32767 * 6 indices, which exceeds the buffer. Closed meshes give
// ~1.5T and stay well under, so it should never fire - but truncating silently draws a PARTIAL volume
// that reads as a correctness bug rather than a capacity one, so each caller says it once. The latch
// is the caller's because the volume draw and the seam bridges overflow for different reasons and
// both are worth hearing.
static uint32 stencil_shadow_stage_index_buffer(
	const uint16* indices,
	uint32 count,
	bool* warned_once)
{
	if (count > k_stencil_shadow_index_buffer_capacity)
	{
		if (!*warned_once)
		{
			*warned_once = true;
			event(_event_warning, "rasterizer:dx9:stencil:draw: index buffer overflow - %u indices truncated to %u (partial volume)",
				count, (uint32)k_stencil_shadow_index_buffer_capacity);
		}
		count = k_stencil_shadow_index_buffer_capacity;
	}

	void* ib_data = NULL;
	HRESULT hr;
	rasterizer_dx9_log_hr(
		hr,
		g_stencil_shadow_index_buffer->Lock(0, count * sizeof(uint16), &ib_data, D3DLOCK_DISCARD)
	);
	if (FAILED(hr))
	{
		return 0;
	}
	memcpy(ib_data, indices, count * sizeof(uint16));
	g_stencil_shadow_index_buffer->Unlock();
	return count;
}
