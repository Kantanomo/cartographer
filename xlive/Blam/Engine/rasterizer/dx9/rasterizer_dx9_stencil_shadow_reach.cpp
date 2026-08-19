#include "stdafx.h"
#include "rasterizer_dx9_stencil_shadow_reach.h"

#include "rasterizer_dx9.h"
#include "rasterizer_dx9_main.h"
#include "rasterizer_dx9_stencil_shadow_tunables.h"
#include "rasterizer_dx9_targets.h"			// _rasterizer_target_z_a8b8g8r8 + set_target_as_texture
#include "rasterizer_dx9_shader_submit.h"	// rasterizer_dx9_set_sampler_state
#include "physics/collisions.h"				// collision_test_vector — the per-caster receiver trace
#include "render/render_cameras.h"			// render_camera::point/forward/z_far for the encode
#include "render/render.h"					// global_window_parameters_get / s_frame
#include "rasterizer/rasterizer_globals.h"
#include "H2MOD/Modules/h2log/h2log.h"
#include "shaders/compiled/shadow_reach_clip.h"

/* globals */

// it. 557 — the reach-clip ps_3_0. Shares the stencil system's SM3 vertex shader: D3D9 forbids
// mixing shader models, so this is only usable alongside it. NULL when unsupported, which disables
// the mode rather than failing the draw.
static IDirect3DPixelShader9* g_stencil_shadow_reach_clip_shader = NULL;

// it. 566 — the constant block, rebuilt per caster. Register map is in shadow_reach_clip.fx; the
// count and base register are in the tunables header.
static real32 g_stencil_shadow_reach_c[k_stencil_shadow_reach_constant_count][4] = {};

// set per caster; false when the mode is off, unsupported, or the camera/viewport look wrong
static bool g_stencil_shadow_reach_active = false;

// it. 581: RESET ON EVERY F6 PRESS, not once per process. At ~500 fps a per-process cap is consumed
// by one caster in milliseconds, so it always captured the player's own object and never the
// scene's (it. 562), and once spent it stayed silent — which made "mode selected but no telemetry"
// ambiguous between "not in the mode" and "cap exhausted".
static int32 g_stencil_shadow_logged_reach = 0;
static int32 g_stencil_shadow_logged_bind = 0;		// for `stencil reach bind:`
static int32 g_stencil_shadow_reach_tick = 0;		// it. 585 — periodic sampler for `stencil reach:`

/* public code */

bool stencil_shadow_reach_shader_create(IDirect3DDevice9Ex* device)
{
	if (g_stencil_shadow_reach_clip_shader)
	{
		return true;
	}
	if (FAILED(device->CreatePixelShader((const DWORD*)k_shadow_reach_clip_ps_3_0,
		&g_stencil_shadow_reach_clip_shader)))
	{
		g_stencil_shadow_reach_clip_shader = NULL;
		return false;
	}
	return true;
}

void stencil_shadow_reach_shader_dispose(void)
{
	if (g_stencil_shadow_reach_clip_shader)
	{
		g_stencil_shadow_reach_clip_shader->Release();
		g_stencil_shadow_reach_clip_shader = NULL;
	}
}

bool stencil_shadow_reach_shader_ready(void)
{
	return g_stencil_shadow_reach_clip_shader != NULL;
}

bool stencil_shadow_reach_is_active(void)
{
	return g_stencil_shadow_reach_active;
}

void stencil_shadow_reach_deactivate(void)
{
	g_stencil_shadow_reach_active = false;
}

void stencil_shadow_reach_reset_logs(void)
{
	g_stencil_shadow_logged_reach = 0;
	g_stencil_shadow_logged_bind = 0;
}

bool stencil_shadow_reach_encode(
	const object_datum* object,
	datum object_index,
	const real_point3d& toward_light_world,
	real32 extrusion_override,
	bool sm3_vertex_ready)
{
	// it. 557 — REACH CLIP. Needs the SM3 pair; without it the mode degrades to the stock
	// distance rather than drawing an unbounded volume, which would look like a catastrophic
	// leak and get blamed on the idea rather than on the missing shader.
	const bool reach_extrusion =
		k_stencil_shadow_reach_enabled
		&& extrusion_override == k_stencil_shadow_reach_extrusion
		&& g_stencil_shadow_reach_clip_shader && sm3_vertex_ready;
	g_stencil_shadow_reach_active = false;
	if (reach_extrusion)
	{
		const s_frame* frame = global_window_parameters_get();
		const render_camera* cam = frame ? &frame->camera : NULL;
		// z_far normalises the encode; a zero/absurd one means the camera is not set up for
		// this view, so leave the mode inactive rather than divide by it.
		const real32 vw = cam ? (real32)rectangle2d_width(&cam->viewport_bounds) : 0.f;
		const real32 vh = cam ? (real32)rectangle2d_height(&cam->viewport_bounds) : 0.f;
		if (cam && cam->z_far > 1.f && vw > 0.f && vh > 0.f
			&& cam->vertical_field_of_view > 0.01f)
		{
			// it. 566 — FULL WORLD RECONSTRUCTION. it. 557 and it. 563 both compared VIEW DEPTHS
			// to keep the unverified encoding semantic non-load-bearing; it. 565 measured why
			// that cannot work at any constant (a downward light with a horizontal view puts the
			// shadow's end at LOWER view depth than the caster, so the threshold inverts).
			// "Within R of the caster" is a 3D distance. Reconstruct and measure it.
			const real_point3d* p = &object->object.center;

			// right = cross(forward, up). Halo's convention is forward=+x, LEFT=+y, up=+z, so
			// cross((1,0,0),(0,0,1)) = (0,-1,0) — i.e. +right, which is what the shader's
			// `uv.x * 2 - 1` (left-to-right) expects.
			const real_vector3d right =
			{
				cam->forward.j * cam->up.k - cam->forward.k * cam->up.j,
				cam->forward.k * cam->up.i - cam->forward.i * cam->up.k,
				cam->forward.i * cam->up.j - cam->forward.j * cam->up.i
			};
			const real32 tan_half_v = tanf(cam->vertical_field_of_view * 0.5f);
			const real32 tan_half_h = tan_half_v * (vw / vh);

			// it. 568: the depth target holds NDC z, not linear depth (it. 566 produced no shadows
			// at all, which is the signature of reconstructing everything at ~z_far). Hand the
			// shader the projection's A/B so it can invert `z_ndc = A - B/d`. These are the same
			// quantities it. 550 measured live off the engine's wvp constants (1.0000587 /
			// 0.0601054) — cross-check the log against those; a large divergence means z_near or
			// z_far here is not the projection the depth target was written with.
			g_stencil_shadow_reach_c[0][0] = 1.f / vw;
			g_stencil_shadow_reach_c[0][1] = 1.f / vh;
			g_stencil_shadow_reach_c[0][2] = cam->z_far;
			g_stencil_shadow_reach_c[0][3] = 0.f;

			g_stencil_shadow_reach_c[1][0] = cam->point.x;
			g_stencil_shadow_reach_c[1][1] = cam->point.y;
			g_stencil_shadow_reach_c[1][2] = cam->point.z;

			// it. 578 — REACH IS PER-CASTER, NOT A CONSTANT.
			//
			// User's observation, and it is a real limitation rather than a tuning problem: a fixed
			// reach encodes "the caster sits near its receiver". An airborne Banshee, a thrown
			// grenade, a player mid-jump all have their LEGITIMATE ground shadow cut off, because
			// their receiver is further away than the constant. That is the same truncation failure
			// the CLIPPED experiments were rejected for, arriving by another route.
			//
			// So find the caster's own receiver: one ray from its centre along the light. This is the
			// SAME probe CLIPPED uses (it. 530), but the use is different and that difference is the
			// whole point — CLIPPED moved the FAR CAP onto the hit, putting a cap in the scene and
			// reproducing the artefact it was meant to remove (it. 522/525). Here the volume stays
			// INFINITE and the hit only sets a per-pixel bound. No cap, no coplanarity.
			//
			// `+ radius` because the ray leaves the object's CENTRE while its sections extend below
			// it; without it a low section's shadow would be cut before reaching the floor.
			real32 caster_reach = k_stencil_shadow_reach_distance;
			{
				real_vector3d reach_probe;
				reach_probe.i = -toward_light_world.x * k_stencil_shadow_reach_probe_length;
				reach_probe.j = -toward_light_world.y * k_stencil_shadow_reach_probe_length;
				reach_probe.k = -toward_light_world.z * k_stencil_shadow_reach_probe_length;

				collision_result reach_hit;
				reach_hit.global_material_index = NONE;
				// World only. Including objects would let one caster truncate another's shadow.
				const uint32 reach_flags =
					FLAG(_collision_test_structure_bit) | FLAG(_collision_test_instanced_geometry_bit);
				if (collision_test_vector(reach_flags, &object->object.center, &reach_probe,
					object_index, NONE, &reach_hit))
				{
					// it. 586 — NO `+ radius`. That term was carried over from CLIPPED (it. 530) where
					// it was correct: there it padded the EXTRUSION LENGTH so sections hanging below
					// the object's centre still reached the floor.
					//
					// Here it is wrong and it is exactly the leak the user reported. `along` measures
					// the RECEIVER's distance from the caster CENTRE along the light, and the floor
					// beneath sits at `hit_distance` no matter which section casts onto it — sections
					// below centre project onto that same floor at the same `along`. So the radius
					// bought nothing and simply licensed the shadow to pass through anything thinner
					// than the caster's own radius (1.077 wu on a Warthog — through most floors).
					// it. 602 - SLOPE IS ANALYTIC. The two-ray fit is REMOVED.
					//
					// it. 590 added a second collision ray to MEASURE d(along)/d(lateral) instead of
					// assuming it. It was the right instinct and it worked, but three measurements
					// retired it:
					//   * it. 592 - the second ray MISSES ~78% of the time (47 of 60 samples);
					//   * it. 592 - when it hits, the measured slope equals |light_xy| to three
					//     decimals in EVERY sample (13/13), i.e. it only ever reproduced the formula;
					//   * it. 598 - `max(measured, analytic)` was then required to reject fits that
					//     spanned two surfaces and went NEGATIVE, which made the measurement
					//     one-sided; it. 599/600 then observed `slope > analytic` NEVER firing.
					//
					// So the ray could only ever agree with the formula or be discarded. Removing it
					// is behaviour-neutral BY MEASUREMENT and deletes a collision_test_vector per
					// caster per frame.
					//
					// Keep the derivation, it is what makes the analytic value exact rather than a
					// guess: on a receiver at depth h, a point L along the shadow has
					//     along = L*|light_xy| + h*|light_z|   ->   d(along)/d(lateral) = |light_xy|
					caster_reach = (reach_hit.t * k_stencil_shadow_reach_probe_length)
						+ k_stencil_shadow_reach_margin;

					const real32 analytic_slope = sqrtf(
						toward_light_world.x * toward_light_world.x
						+ toward_light_world.y * toward_light_world.y);
					g_stencil_shadow_reach_c[7][3] = analytic_slope;

					// Horizontal direction the shadow travels; the shader projects the receiver onto
					// it to get `lateral`. Degenerate under a near-vertical light, and correctly so -
					// there is no lateral spread, and the slope term contributes nothing.
					if (analytic_slope > 0.05f)
					{
						g_stencil_shadow_reach_c[7][0] = -toward_light_world.x / analytic_slope;
						g_stencil_shadow_reach_c[7][1] = -toward_light_world.y / analytic_slope;
					}
					else
					{
						g_stencil_shadow_reach_c[7][0] = 0.f;
						g_stencil_shadow_reach_c[7][1] = 0.f;
					}
					g_stencil_shadow_reach_c[7][2] = 0.f;
				}
				else
				{
					// Nothing within the probe: the caster is over a void or a very distant floor.
					// FAIL OPEN — leave the reach effectively unbounded rather than truncating a
					// shadow we simply could not measure. A leak is recoverable; a missing shadow
					// over open ground reads as the feature being broken.
					caster_reach = k_stencil_shadow_reach_probe_length;
				}
			}
			g_stencil_shadow_reach_c[1][3] = caster_reach;

			g_stencil_shadow_reach_c[2][0] = cam->forward.i;
			g_stencil_shadow_reach_c[2][1] = cam->forward.j;
			g_stencil_shadow_reach_c[2][2] = cam->forward.k;

			g_stencil_shadow_reach_c[3][0] = right.i;
			g_stencil_shadow_reach_c[3][1] = right.j;
			g_stencil_shadow_reach_c[3][2] = right.k;
			g_stencil_shadow_reach_c[3][3] = tan_half_h;

			g_stencil_shadow_reach_c[4][0] = cam->up.i;
			g_stencil_shadow_reach_c[4][1] = cam->up.j;
			g_stencil_shadow_reach_c[4][2] = cam->up.k;
			g_stencil_shadow_reach_c[4][3] = tan_half_v;

			g_stencil_shadow_reach_c[5][0] = p->x;
			g_stencil_shadow_reach_c[5][1] = p->y;
			g_stencil_shadow_reach_c[5][2] = p->z;

			// Extrusion direction = AWAY from the light, the axis the shadow actually travels.
			g_stencil_shadow_reach_c[6][0] = -toward_light_world.x;
			g_stencil_shadow_reach_c[6][1] = -toward_light_world.y;
			g_stencil_shadow_reach_c[6][2] = -toward_light_world.z;

			g_stencil_shadow_reach_active = true;
		}
		// it. 562: cap raised from 8. At ~500 fps the old cap was consumed by ONE caster in a
		// dozen milliseconds, so every sample showed the same object and its unrepresentative
		// depth (~0, the player's own) stood in for all casters. Also report world units and
		// the caster position, because the encoded values alone hid that.
		// it. 585 — SAMPLE PERIODICALLY, AND ALWAYS ON A NOTABLE REACH.
		//
		// A flat cap cannot capture the case under test. At ~500 fps 40 samples are consumed in well
		// under a second, so the log only ever describes whatever was on screen the instant the mode
		// was entered — grounded props, every time (it. 584). The airborne caster that motivated
		// it. 578 would have to be in view at that exact instant to be seen at all.
		//
		// So: sample slowly in the background, but log IMMEDIATELY whenever a caster's traced reach
		// is large, because that is precisely the airborne / distant-receiver case we cannot
		// otherwise observe. `> 5 wu` is comfortably above the 1.1-2.1 wu grounded band measured in
		// it. 584 and well below the 50 wu fail-open.
		const bool reach_notable = g_stencil_shadow_reach_c[1][3] > 5.f;
		if (g_stencil_shadow_logged_reach < 60
			&& (reach_notable || (g_stencil_shadow_reach_tick++ % 240) == 0))
		{
			g_stencil_shadow_logged_reach++;
			// it. 592: slope and its expected value are the whole story for the airborne breakage,
			// and neither was visible. For a FLAT receiver the slope must equal |light_xy| exactly
			// (derivation in it. 590) — printing both makes a bad fit self-evident: slope 0 means the
			// second ray missed, and slope far from |light_xy| means it hit a different surface.
			const real32 expected_slope = sqrtf(
				toward_light_world.x * toward_light_world.x
				+ toward_light_world.y * toward_light_world.y);

			UNREFERENCED_PARAMETER(expected_slope);


#ifdef LOG_STENCIL
			LOG_INFO_GAME("stencil reach: active={} caster=({:.2f},{:.2f},{:.2f}) cam=({:.2f},{:.2f},{:.2f}) SLOPE={:.3f} expected_flat={:.3f} bound_base={:.2f}wu tan_h={:.3f} tan_v={:.3f} z_far={:.1f} extrude_dir=({:.2f},{:.2f},{:.2f}) — it. 592 (slope 0 = 2nd ray missed; slope != expected = different surface)",
				g_stencil_shadow_reach_active ? 1 : 0,
				object->object.center.x, object->object.center.y, object->object.center.z,
				g_stencil_shadow_reach_c[1][0], g_stencil_shadow_reach_c[1][1], g_stencil_shadow_reach_c[1][2],
				g_stencil_shadow_reach_c[7][3], expected_slope, g_stencil_shadow_reach_c[1][3],
				g_stencil_shadow_reach_c[3][3], g_stencil_shadow_reach_c[4][3],
				// it. 597: the SECOND `reach_c[1][3]` that used to sit here was a leftover from the
				// it. 575 format, giving 17 arguments for 16 placeholders. The surplus consumed the
				// first extrude_dir slot and shifted the vector by one, so the line printed
				// (bound_base, extrude.x, extrude.y) while claiming to print extrude_dir. That is
				// what made it. 596 read a ~10-degree light off a perfectly ordinary ~46-degree one.
				// fmt does not error on surplus arguments, so nothing flagged it.
				g_stencil_shadow_reach_c[0][2],
				g_stencil_shadow_reach_c[6][0], g_stencil_shadow_reach_c[6][1], g_stencil_shadow_reach_c[6][2]);
#endif
		}
	}
	return reach_extrusion;
}

void stencil_shadow_reach_bind(IDirect3DDevice9Ex* device)
{
	device->SetPixelShader(g_stencil_shadow_reach_clip_shader);
	device->SetPixelShaderConstantF(k_stencil_shadow_reach_constant_base,
		&g_stencil_shadow_reach_c[0][0], k_stencil_shadow_reach_constant_count);
	// The engine's depth-as-colour MRT output (target 22), already written by the geometry
	// passes that run before us. POINT filtering on purpose: the packed 24-bit depth is spread
	// across R,G,B, so any interpolation between texels blends unrelated byte planes and
	// produces depths that exist nowhere in the scene.
	rasterizer_dx9_set_target_as_texture(0, _rasterizer_target_z_a8b8g8r8);
	rasterizer_dx9_set_sampler_state(0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
	rasterizer_dx9_set_sampler_state(0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
	rasterizer_dx9_set_sampler_state(0, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
	rasterizer_dx9_set_sampler_state(0, D3DSAMP_MINFILTER, D3DTEXF_POINT);
	rasterizer_dx9_set_sampler_state(0, D3DSAMP_MIPFILTER, D3DTEXF_NONE);

	// it. 568 — VERIFY THE BIND, do not infer it. Two engine shaders agree the encoding is linear,
	// so it. 566 should have produced shadows and did not. That makes "the sample never happens" the
	// leading candidate, and it is directly checkable: read the texture back off the device and
	// compare its surface size against the viewport the UVs are built from. A size mismatch also
	// invalidates the `(vpos + 0.5) * (1/viewport)` mapping even when the bind succeeds.
	if (g_stencil_shadow_logged_bind < 4)
	{
		g_stencil_shadow_logged_bind++;
		IDirect3DBaseTexture9* bound = NULL;
		HRESULT hr = device->GetTexture(0, &bound);
		uint32 tw = 0, th = 0;
		if (SUCCEEDED(hr) && bound)
		{
			IDirect3DTexture9* tex2d = NULL;
			if (SUCCEEDED(bound->QueryInterface(__uuidof(IDirect3DTexture9), (void**)&tex2d)) && tex2d)
			{
				D3DSURFACE_DESC desc = {};
				if (SUCCEEDED(tex2d->GetLevelDesc(0, &desc)))
				{
					tw = desc.Width;
					th = desc.Height;
				}
				tex2d->Release();
			}
			bound->Release();
		}
		LOG_INFO_GAME("stencil reach bind: hr={:#x} texture={} size={}x{} viewport={:.0f}x{:.0f} (sizes MUST match or the VPOS->UV mapping is wrong)",
			(uint32)hr, bound ? 1 : 0, tw, th,
			(g_stencil_shadow_reach_c[0][0] > 0.f ? 1.f / g_stencil_shadow_reach_c[0][0] : 0.f),
			(g_stencil_shadow_reach_c[0][1] > 0.f ? 1.f / g_stencil_shadow_reach_c[0][1] : 0.f));
	}
}

void stencil_shadow_reach_unbind(void)
{
	// it. 568 — RELEASE SAMPLER 0. The reach path binds the depth target to s0 and forces POINT
	// filtering; leaving either in place corrupts every later draw that samples s0 without setting
	// its own texture. The user saw exactly that: a wall decal (the "3" numeral) rendering as a flat
	// black shape. Unbinding the texture is the part that matters — the filter is restored to LINEAR
	// as well so a later consumer relying on the engine's default is not silently point-sampled.
	//
	// it. 573 — THROUGH THE CACHED SETTER, never the raw device. it. 568 used
	// `device->SetTexture(0, NULL)` and desynced the engine's redundancy cache: the cache still
	// believed the depth target was bound, so the NEXT section's bind no-opped and every draw after
	// the first sampled nothing (MEASURED: `texture=1` on sample 1, `texture=0` on 2-4), while the
	// engine separately believed s0 was populated — which is what corrupted the wall decals.
	//
	// `rasterizer_dx9_device_set_texture` is the setter `rasterizer_dx9_set_target_as_texture` itself
	// ends in (targets.cpp:353). Its body (IDB 0x66EBC7) is `if (cache[stage] != texture) {
	// SetTexture(...); cache[stage] = texture; }` — a plain compare-and-set that handles NULL,
	// VERIFIED by decompilation. Releasing through it keeps cache and device in step.
	rasterizer_dx9_device_set_texture(0, NULL);
	rasterizer_dx9_set_sampler_state(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
	rasterizer_dx9_set_sampler_state(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
}
