#include "stdafx.h"
#include "rasterizer_dx9_stencil_shadow_reach.h"

#include "rasterizer_dx9.h"
#include "rasterizer_dx9_main.h"
#include "rasterizer_dx9_stencil_shadow_tunables.h"
#include "rasterizer_dx9_targets.h"			// _rasterizer_target_z_a8b8g8r8 + set_target_as_texture
#include "rasterizer_dx9_shader_submit.h"	// rasterizer_dx9_set_sampler_state
#include "physics/collisions.h"				// collision_test_vector - the per-caster receiver trace
#include "render/render_cameras.h"			// render_camera::point/forward/z_far for the encode
#include "render/render.h"					// global_window_parameters_get / s_frame
#include "networking/network_event.h"
#include "shaders/compiled/shadow_reach_clip.h"

/* globals */

// The reach-clip ps_3_0. Shares the stencil system's SM3 vertex shader: D3D9 forbids
// mixing shader models, so this is only usable alongside it. NULL when unsupported, which disables
// the mode rather than failing the draw.
static IDirect3DPixelShader9* g_stencil_shadow_reach_clip_shader = NULL;

// The constant block, rebuilt per caster. Register map is in shadow_reach_clip.fx; the
// count and base register are in the tunables header.
static real32 g_stencil_shadow_reach_c[k_stencil_shadow_reach_constant_count][4] = {};

// set per caster; false when the mode is off, unsupported, or the camera/viewport look wrong
static bool g_stencil_shadow_reach_active = false;

// RESET ON EVERY F6 PRESS, not once per process. At several hundred fps a per-process cap is
// consumed by one caster in milliseconds, so it always captured the player's own object and never
// the scene's, and once spent it stayed silent - which made "mode selected but no telemetry"
// ambiguous between "not in the mode" and "cap exhausted".
static int32 g_stencil_shadow_logged_reach = 0;
static int32 g_stencil_shadow_logged_bind = 0;		// for `stencil reach bind:`
static int32 g_stencil_shadow_reach_tick = 0;		// periodic sampler for the `:reach` line

// A per-caster scissor rect - the kill's kept-region capsule projected to pixels - lived here and was
// REMOVED after measurement: it engaged at an average 10% of the viewport and the frame rate did not
// move, so the GPU is not what this system pays for. The derivation and the SetScissorRect-persistence
// lesson are in td-isq-generation.md, should a GPU-bound scenario ever revive it.

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
	// REACH CLIP. Needs the SM3 pair; without it the mode degrades to the stock
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
			// FULL WORLD RECONSTRUCTION. Comparing view DEPTHS instead cannot work at any constant: a
			// downward light with a horizontal view puts the shadow's end at LOWER view depth than the
			// caster, so the threshold inverts. "Within R of the caster" is a 3D distance, so
			// reconstruct the position and measure it.
			const real_point3d* p = &object->object.center;

			// right = cross(forward, up). Halo's convention is forward=+x, LEFT=+y, up=+z, so
			// cross((1,0,0),(0,0,1)) = (0,-1,0) - i.e. +right, which is what the shader's
			// `uv.x * 2 - 1` (left-to-right) expects.
			const real_vector3d right =
			{
				cam->forward.j * cam->up.k - cam->forward.k * cam->up.j,
				cam->forward.k * cam->up.i - cam->forward.i * cam->up.k,
				cam->forward.i * cam->up.j - cam->forward.j * cam->up.i
			};
			const real32 tan_half_v = tanf(cam->vertical_field_of_view * 0.5f);
			const real32 tan_half_h = tan_half_v * (vw / vh);

			// The depth target holds NDC z, not linear depth - treating it as linear reconstructs
			// everything at ~z_far and produces no shadows at all. Hand the shader the projection's
			// A/B so it can invert `z_ndc = A - B/d`. Measured live off the engine's wvp constants as
			// 1.0000587 / 0.0601054; cross-check the log against those, since a large divergence means
			// z_near or z_far here is not the projection the depth target was written with.
			g_stencil_shadow_reach_c[0][0] = 1.f / vw;
			g_stencil_shadow_reach_c[0][1] = 1.f / vh;
			g_stencil_shadow_reach_c[0][2] = cam->z_far;
			g_stencil_shadow_reach_c[0][3] = 0.f;

			g_stencil_shadow_reach_c[1][0] = cam->point.x;
			g_stencil_shadow_reach_c[1][1] = cam->point.y;
			g_stencil_shadow_reach_c[1][2] = cam->point.z;

			// REACH IS PER-CASTER, NOT A CONSTANT. A fixed reach encodes "the caster sits near its
			// receiver", so an airborne Banshee, a thrown grenade or a player mid-jump has its
			// legitimate ground shadow cut off - the same truncation failure the finite-cap
			// experiments were rejected for, arriving by another route. So find the caster's own
			// receiver with one ray from its centre along the light. The hit only sets a per-pixel
			// bound; the volume stays infinite, so there is still no cap in the scene to graze.
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
					// NO `+ radius` here, however tempting. `along` measures the RECEIVER's distance
					// from the caster CENTRE along the light, and the floor beneath sits at
					// hit_distance no matter which section casts onto it, so sections below centre
					// project onto that same floor at the same `along`. Padding by the radius buys
					// nothing and licenses the shadow to pass through anything thinner than the
					// caster's own radius - 1.077 wu on a Warthog, which is most floors.
					//
					caster_reach = (reach_hit.t * k_stencil_shadow_reach_probe_length)
						+ k_stencil_shadow_reach_margin;

					// The slope is ANALYTIC - no second ray. One was tried and retired: it missed ~78%
					// of the time, and the trace we already have carries the receiver's plane anyway.
					//
					// The horizontal-only form was `|light_xy|`, from
					//     along = L*|light_xy| + h*|light_z|   ->   d(along)/d(lateral) = |light_xy|
					// which holds only where h does not vary with L - a FLAT receiver. On a ramp the
					// true slope is |light_xy| + (dh/dL)*|light_z|, so every tilted surface was
					// mis-bounded, leaking or truncating depending on which way it tilted.
					//
					// The hit plane fixes that exactly. Project the shadow's horizontal direction onto
					// the receiver and take the ratio the shader needs:
					//     s_proj = s - dot(s, n) * n
					//     slope  = dot(s_proj, e) / dot(s_proj, s)
					// On flat ground dot(s, n) == 0, so s_proj == s and this returns dot(s, e), which
					// is |light_xy| - the old value exactly. A strict generalisation: it cannot regress
					// the case that already worked.
					const real32 analytic_slope = sqrtf(
						toward_light_world.x * toward_light_world.x
						+ toward_light_world.y * toward_light_world.y);

					// Horizontal direction the shadow travels; the shader projects the receiver onto
					// it to get `lateral`. Degenerate under a near-vertical light, and correctly so -
					// there is no lateral spread, and the slope term contributes nothing.
					real32 slope = analytic_slope;
					if (analytic_slope > 0.05f)
					{
						real_vector3d spread;
						spread.i = -toward_light_world.x / analytic_slope;
						spread.j = -toward_light_world.y / analytic_slope;
						spread.k = 0.f;
						g_stencil_shadow_reach_c[7][0] = spread.i;
						g_stencil_shadow_reach_c[7][1] = spread.j;

						// s_proj = s - dot(s, n) * n, with n the hit plane's normal. `plane` is the hit
						// surface in WORLD space with n already facing the ray, for both the structure
						// and instanced-geometry paths.
						const real_vector3d* n = &reach_hit.plane.n;
						const real32 s_dot_n = spread.i * n->i + spread.j * n->j;	// spread.k is 0
						real_vector3d s_proj;
						s_proj.i = spread.i - s_dot_n * n->i;
						s_proj.j = spread.j - s_dot_n * n->j;
						s_proj.k = -s_dot_n * n->k;

						// The denominator is |s_proj| projected back onto s, and it collapses toward 0
						// as the receiver turns edge-on to the light - where "further along the shadow"
						// stops being a direction on that surface at all. Keep the flat-ground slope
						// there rather than dividing into a blow-up.
						const real32 d_lateral =
							s_proj.i * spread.i + s_proj.j * spread.j;
						if (d_lateral > 0.05f)
						{
							const real32 d_along =
								s_proj.i * -toward_light_world.x
								+ s_proj.j * -toward_light_world.y
								+ s_proj.k * -toward_light_world.z;
							slope = d_along / d_lateral;
						}
					}
					else
					{
						g_stencil_shadow_reach_c[7][0] = 0.f;
						g_stencil_shadow_reach_c[7][1] = 0.f;
					}
					g_stencil_shadow_reach_c[7][2] = 0.f;
					g_stencil_shadow_reach_c[7][3] = slope;
				}
				else
				{
					// Nothing within the probe: the caster is over a void or a very distant floor.
					// FAIL OPEN - leave the reach effectively unbounded rather than truncating a
					// shadow we simply could not measure. A leak is recoverable; a missing shadow
					// over open ground reads as the feature being broken.
					caster_reach = k_stencil_shadow_reach_probe_length;

					// These constants are file scope and survive between casters, so the fail-open
					// path has to write them too. It did not need to while the slope was |light_xy|,
					// which is identical for every caster under one light - a stale copy was the same
					// value. The slope is per-RECEIVER now, so a stale one belongs to whatever the
					// previous caster happened to be standing on.
					g_stencil_shadow_reach_c[7][0] = 0.f;
					g_stencil_shadow_reach_c[7][1] = 0.f;
					g_stencil_shadow_reach_c[7][2] = 0.f;
					g_stencil_shadow_reach_c[7][3] = 0.f;
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
		// SAMPLE PERIODICALLY, AND ALWAYS ON A NOTABLE REACH. A flat sample cap cannot capture the
		// case under test: at several hundred fps it is consumed in well under a second, so the log
		// only describes whatever was on screen the instant the mode was entered, which is grounded
		// props every time. The airborne caster this per-caster reach exists for would have to be in
		// view at that exact instant to be seen at all. So sample slowly in the background but log
		// immediately whenever a traced reach is large. 5 wu is comfortably above the 1.1-2.1 wu
		// grounded band measured live and well below the 50 wu fail-open.
		const bool reach_notable = g_stencil_shadow_reach_c[1][3] > 5.f;
		if (g_stencil_shadow_logged_reach < 60
			&& (reach_notable || (g_stencil_shadow_reach_tick++ % 240) == 0))
		{
			g_stencil_shadow_logged_reach++;
			// `expected_flat` is what the slope WAS before the receiver plane was used: |light_xy|,
			// the flat-ground value. On a flat receiver the two must still agree exactly, which is
			// the regression check. Where they differ, the difference is the receiver's tilt, and
			// SLOPE is the corrected one.
			const real32 expected_slope = sqrtf(
				toward_light_world.x * toward_light_world.x
				+ toward_light_world.y * toward_light_world.y);

			UNREFERENCED_PARAMETER(expected_slope);

			event(_event_verbose, "rasterizer:dx9:stencil:reach: active=%d caster=(%.2f,%.2f,%.2f) cam=(%.2f,%.2f,%.2f) SLOPE=%.3f expected_flat=%.3f bound_base=%.2fwu tan_h=%.3f tan_v=%.3f z_far=%.1f extrude_dir=(%.2f,%.2f,%.2f)",
				g_stencil_shadow_reach_active ? 1 : 0,
				object->object.center.x, object->object.center.y, object->object.center.z,
				g_stencil_shadow_reach_c[1][0], g_stencil_shadow_reach_c[1][1], g_stencil_shadow_reach_c[1][2],
				g_stencil_shadow_reach_c[7][3], expected_slope, g_stencil_shadow_reach_c[1][3],
				g_stencil_shadow_reach_c[3][3], g_stencil_shadow_reach_c[4][3],
				// Count these against the placeholders before editing. A stray argument here once shifted
				// extrude_dir by a slot, which read as a 10-degree light off a perfectly ordinary
				// 46-degree one - and now that this is vsprintf rather than fmt, one too FEW reads
				// whatever is next on the stack instead of throwing.
				g_stencil_shadow_reach_c[0][2],
				g_stencil_shadow_reach_c[6][0], g_stencil_shadow_reach_c[6][1], g_stencil_shadow_reach_c[6][2]);
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

	// VERIFY THE BIND, do not infer it: read the texture back off the device and compare its surface
	// size against the viewport the UVs are built from. A silently failed bind and a size mismatch
	// present identically as "no shadows", and a mismatch also invalidates the
	// `(vpos + 0.5) * (1/viewport)` mapping even when the bind succeeds.
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
		event(_event_verbose, "rasterizer:dx9:stencil:reach: bind hr=%#x texture=%d size=%ux%u viewport=%.0fx%.0f (sizes MUST match or the VPOS->UV mapping is wrong)",
			(uint32)hr, bound ? 1 : 0, tw, th,
			(g_stencil_shadow_reach_c[0][0] > 0.f ? 1.f / g_stencil_shadow_reach_c[0][0] : 0.f),
			(g_stencil_shadow_reach_c[0][1] > 0.f ? 1.f / g_stencil_shadow_reach_c[0][1] : 0.f));
	}
}

void stencil_shadow_reach_unbind(void)
{
	// RELEASE SAMPLER 0. The bind above leaves the depth target on s0 with POINT filtering, and either
	// left in place corrupts every later draw that samples s0 without setting its own texture - a wall
	// decal rendering as a flat black shape, in the case that found this. The texture is the part that
	// matters; the filter is restored as well so a later consumer relying on the engine's default is
	// not silently point-sampled.
	//
	// THROUGH THE CACHED SETTER, never the raw device. `rasterizer_dx9_device_set_texture` is the
	// setter `rasterizer_dx9_set_target_as_texture` itself ends in, a compare-and-set against the
	// engine's redundancy cache. A raw `device->SetTexture(0, NULL)` desyncs that cache: it still
	// believes the depth target is bound, so the next section's bind no-ops and every draw after the
	// first samples nothing.
	rasterizer_dx9_device_set_texture(0, NULL);
	rasterizer_dx9_set_sampler_state(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
	rasterizer_dx9_set_sampler_state(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
}
