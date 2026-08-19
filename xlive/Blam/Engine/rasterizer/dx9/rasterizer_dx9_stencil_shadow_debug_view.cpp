#include "stdafx.h"
#include "rasterizer_dx9_stencil_shadow_debug_view.h"

#include "rasterizer_dx9_stencil_shadow_tunables.h"
#include "rasterizer_dx9_stencil_shadow_reach.h"
#include "render/render_stencil_shadow_dynamic.h"
#include "render/render_stencil_shadow_environment.h"
#include "rasterizer_dx9_stencil_shadows.h"	// stencil_shadow_sm3_vertex_shader_ready
#include "H2MOD/Modules/OnScreenDebug/OnscreenDebug.h"
#include "H2MOD/Modules/h2log/h2log.h"

#include <windows.h>

/* globals */

// The three pieces of debug STATE the rest of the system reads. They live here, behind accessors,
// so the key handling and the state it drives sit together — it. 630.
//
// NOT moved: g_stencil_shadow_quad_winding_flip. It is also a debug toggle, but it is read inside
// the per-quad emission loop, where an accessor call would sit in the hottest path in the system for
// no benefit. It stays a file-static in the draw code that reads it.
static bool g_active = false;              // F8 master latch
static int32 g_draw_mode = 0;              // F7: 0 = real shadows, 1 = red volumes, 2 = stencil probe
static real32 g_extrusion_override = 0.f;  // F6: 0 = stock, negative = a mode sentinel

/* public code */

int32 stencil_shadow_debug_draw_mode(void)
{
	return g_draw_mode;
}

real32 stencil_shadow_debug_extrusion_override(void)
{
	return g_extrusion_override;
}

bool stencil_shadow_active(void)
{
	return g_active;
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
		g_active = !g_active;

#ifdef LOG_STENCIL
		LOG_INFO_GAME("stencil shadows: active={} mode={}", g_active, g_draw_mode);
#endif
	}
	key_was_down = key_down;

	// F5 toggles the DYNAMIC light tier (td layer 13). Separate switch from F8 on purpose: that
	// tier is unverified, so it must be possible to run the shipped lightmap tier without it.
	static bool dyn_key_was_down = false;
	bool dyn_key_down = (GetAsyncKeyState(VK_F5) & 0x8000) != 0;
	if (dyn_key_down && !dyn_key_was_down)
	{
		stencil_shadow_dynamic_toggle();
	}
	dyn_key_was_down = dyn_key_down;

	// F4 toggles the ENVIRONMENT tier (BSP clusters) WITHIN the dynamic light tier. Two switches for
	// what td treats as one layer, because the two halves fail differently: a wrong model volume is a
	// wrong silhouette, a wrong cluster volume darkens a whole room. Being able to remove one half
	// while keeping the other is what makes the first bad screenshot diagnosable.
	static bool env_key_was_down = false;
	bool env_key_down = (GetAsyncKeyState(VK_F4) & 0x8000) != 0;
	if (env_key_down && !env_key_was_down)
	{
		stencil_shadow_environment_toggle();
	}
	env_key_was_down = env_key_down;

	// F7 cycles draw modes
	static bool probe_key_was_down = false;
	bool probe_key_down = (GetAsyncKeyState(VK_F7) & 0x8000) != 0;
	if (probe_key_down && !probe_key_was_down)
	{
		g_draw_mode = (g_draw_mode + 1) % 3;

#ifdef LOG_STENCIL
		LOG_INFO_GAME("stencil shadows: active={} mode={} (0=real 1=red 2=green)",
			g_active, g_draw_mode);
#endif
	}
	probe_key_was_down = probe_key_down;

	// F6 cycles extrusion distance (diagnostic)
	static bool extrude_key_was_down = false;
	bool extrude_key_down = (GetAsyncKeyState(VK_F6) & 0x8000) != 0;
	if (extrude_key_down && !extrude_key_was_down)
	{
		// it. 527: DYNAMIC is the FIRST step, so one press from the default lands on the mode under
		// test rather than on a 500 wu diagnostic extreme.
		//   2.0 (stock) -> DYNAMIC -> 500 -> 5 -> 0.3 -> 2.0
		// it. 573: REACH restored to the cycle, first after STOCK so the comparison that matters
		// (2.0 vs reach) stays a single keypress.
		if (g_extrusion_override == 0.f)
		{
			g_extrusion_override = k_stencil_shadow_reach_extrusion;
		}
		// it. 603: CLIPPED (-2) and DYNAMIC (-1) removed from the cycle. Both were MEASURED to
		// relocate the far cap rather than remove it (it. 522/525/540/541) and are superseded by
		// REACH-CLIP, which has no finite cap to relocate. Their reasoning is preserved in
		// td-do-not-fix.md entry 15 — written BEFORE this removal, because deleting them destroys
		// the ability to re-demonstrate the failures.
		//
		// Unreachable from here as of this change; their implementation bodies come out next, as a
		// separate step so this user-visible simplification can be verified on its own.
		else if (g_extrusion_override == k_stencil_shadow_reach_extrusion)
		{
			g_extrusion_override = 500.f;
		}
		else if (g_extrusion_override == 500.f)
		{
			g_extrusion_override = 5.f;
		}
		else if (g_extrusion_override == 5.f)
		{
			// it. 521: a SHORT step, added to test whether the seam leaks (it. 517-520) and the
			// original through-geometry over-reach COMPOUND, as the user proposed.
			//
			// The mechanism: an unbridged seam is an OPEN volume, and an opening only produces a wrong
			// count where it falls between the eye and a receiver **inside the volume**. So the seam
			// defect's visible area scales with how much receiver surface the volume covers — which is
			// what extrusion length sets. Symptom 3 compounds it by putting volume on BOTH sides of
			// thin geometry, roughly doubling the exposed surface.
			//
			// Every previous F6 step went UP (2 -> 500 -> 5), which cannot test this. 0.3 wu is well
			// under a caster's own size (a biped is 0.705 wu tall, it. 415), so the volume barely
			// leaves the caster and the covered receiver area collapses.
			//
			//   leaks shrink with the volume -> COMPOUNDING CONFIRMED: the seams are the defect, the
			//                                   extrusion is the exposure. Restoring stitching fixes it,
			//                                   and no extrusion change is warranted (it is td's own 2.0)
			//   leaks persist unchanged       -> extrusion is NOT a factor; the seam openings leak
			//                                   wherever they are, and the compounding idea is wrong
			g_extrusion_override = 0.3f;
		}
		else
		{
			g_extrusion_override = 0.f;
		}
		// it. 527: the sentinel is negative, so printing it raw would read as "extrusion=-1" and look
		// like a bug. Name the mode instead; the per-section values it resolves to are visible on the
		// `stencil size:` line (`extrusion=` computed, `radius=` the caster it belongs to).
		// it. 528: ON SCREEN, not just in the log. The mode was only discoverable by reading the log
		// file mid-game, which is useless while looking at the artefact — the user could not tell which
		// F6 step was the dynamic one. `addDebugText` is the same facility the rest of the mod uses for
		// in-game state.
		// it. 581: every mode change re-arms the per-caster diagnostics, so entering a mode always
		// yields fresh telemetry from the CURRENT scene rather than whatever the counters caught in
		// the first few milliseconds of the process.
		stencil_shadow_reach_reset_logs();
		// it. 581: print the RAW override. Two iterations were lost to "the screen says REACH-CLIP but
		// no reach telemetry appears" — the on-screen text is scrollback and persists after later
		// presses move the mode on, so it is NOT evidence of the current mode. This value is.

#ifdef LOG_STENCIL
		LOG_INFO_GAME("stencil mode: override={:.1f} (sentinels: 0=STOCK -1=DYNAMIC -2=CLIPPED -3=REACH; anything else is a literal wu distance)",
			g_extrusion_override);
#endif

		if (g_extrusion_override == k_stencil_shadow_reach_extrusion)
		{
			// Say on screen whether the mode is actually RUNNING. Without SM3 it silently falls back
			// to stock 2.0, and a user comparing modes would otherwise see "REACH" while looking at
			// the stock artefact and conclude the idea failed.
			const bool usable =
				stencil_shadow_reach_shader_ready() && stencil_shadow_sm3_vertex_shader_ready();
			addDebugText("stencil extrusion: REACH-CLIP %s (infinite volume, %.1f wu reach)",
				usable ? "active" : "UNAVAILABLE (no SM3) - running stock 2.0",
				k_stencil_shadow_reach_distance);

#ifdef LOG_STENCIL
			LOG_INFO_GAME("stencil shadows: extrusion=REACH-CLIP usable={} extrude={}wu reach={}wu (it. 557 — infinite volume, per-pixel depth bound)",
				usable ? 1 : 0, k_stencil_shadow_reach_extrusion_distance,
				k_stencil_shadow_reach_distance);
#endif
		}
		else
		{
#ifdef LOG_STENCIL
			const real32 shown = g_extrusion_override == 0.f
				? k_stencil_shadow_extrusion_distance : g_extrusion_override;
			
			addDebugText("stencil extrusion: %.3f wu%s", shown,
				g_extrusion_override == 0.f ? " (STOCK, td's value)" : " (fixed)");
			LOG_INFO_GAME("stencil shadows: extrusion={}", shown);
#endif
		}
	}
	extrude_key_was_down = extrude_key_down;
}

// P2-1 — td's s_model_level_of_detail, the hlmt block that follows the tag's five leading
// tag references (render_model, collision_model, animation_graph, physics, physics_model),
// i.e. hlmt+0x28. Field offsets are td's render_lod_compute_model_alpha (td 0xD5900) rebased
// to the tag: its lod+0 is hlmt+0x28, lod+12 is the reduce-to-LOD distance array, lod+36 the
// shadow-fade LOD selector. Reads are VALIDATED before use (see below) because these offsets
// are reconstructed rather than taken from a Vista symbol.
