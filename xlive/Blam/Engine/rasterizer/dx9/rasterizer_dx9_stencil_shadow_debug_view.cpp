#include "stdafx.h"
#include "rasterizer_dx9_stencil_shadow_debug_view.h"

#include "rasterizer_dx9_stencil_shadow_tunables.h"
#include "rasterizer_dx9_stencil_shadow_reach.h"
#include "render/render_lights.h"				// the F5 tier toggle
#include "rasterizer_dx9_stencil_shadows.h"	// stencil_shadow_sm3_vertex_shader_ready
#include "H2MOD/Modules/OnScreenDebug/OnscreenDebug.h"
#include "networking/network_event.h"

#include <windows.h>

/* globals */

// The three pieces of debug state the rest of the system reads, behind accessors so the key handling
// and the state it drives sit together. Not here: g_stencil_shadow_quad_winding_flip, which is read
// inside the per-quad emission loop where an accessor call would sit in the hottest path in the
// system for no benefit.
static bool g_active = false;              // F8 master latch
static int32 g_draw_mode = 0;              // F7: 0 = real, 1 = red sun volumes, 2 = stencil probe,
                                           //     3 = blue dynamic-tier volumes
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

// UI-phase hook: key handling ONLY - drawing happens in the render-layer hook where the scene
// depth-stencil is still bound, the UI phase having no functional stencil.
void stencil_shadow_debug_update(void)
{
	static bool key_was_down = false;
	bool key_down = (GetAsyncKeyState(VK_F8) & 0x8000) != 0;
	if (key_down && !key_was_down)
	{
		g_active = !g_active;
		event(_event_status, "rasterizer:dx9:stencil: shadows %s mode=%d", g_active ? "ENABLED" : "DISABLED", g_draw_mode);
	}
	key_was_down = key_down;

	// F5 toggles the DYNAMIC light tier (tag debug's layer 13), and Shift+F5 forces every light to
	// cast. A separate switch from F8 on purpose: that tier is the newer one, so it must be possible
	// to run the shipped lightmap tier without it.
	static bool dyn_key_was_down = false;
	bool dyn_key_down = (GetAsyncKeyState(VK_F5) & 0x8000) != 0;
	if (dyn_key_down && !dyn_key_was_down)
	{
		if ((GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0)
			render_lights_dyn_tier_force_all_toggle();
		else
			render_lights_dyn_tier_toggle();
	}
	dyn_key_was_down = dyn_key_down;

	// F4 toggles the ENVIRONMENT tier (BSP clusters) within the dynamic light tier, and Shift+F4 the
	// reach-cull A/B. Two switches for what tag debug treats as one layer, because the two halves fail
	// differently: a wrong model volume is a wrong silhouette, a wrong cluster volume darkens a whole
	// room. Removing one half while keeping the other is what makes a bad screenshot diagnosable.
	static bool env_key_was_down = false;
	bool env_key_down = (GetAsyncKeyState(VK_F4) & 0x8000) != 0;
	if (env_key_down && !env_key_was_down)
	{
		if ((GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0)
			render_lights_env_reach_cull_toggle();
		else
			render_lights_env_tier_toggle();
	}
	env_key_was_down = env_key_down;

	// F7 cycles draw modes
	static bool probe_key_was_down = false;
	bool probe_key_down = (GetAsyncKeyState(VK_F7) & 0x8000) != 0;
	if (probe_key_down && !probe_key_was_down)
	{
		// Mode 1 is the SUN tier's volumes alone and mode 3 the DYNAMIC tier's alone: in mode 3 the
		// lightmap tier falls through to its invisible stencil path and its apply is mode-0-gated, so
		// the frame isolates one tier's volumes at a time.
		g_draw_mode = (g_draw_mode + 1) % 4;
		event(_event_status, "rasterizer:dx9:stencil: shadows %s mode=%d (0=real 1=red-sun 2=green-probe 3=blue-dyn)",
			g_active ? "ENABLED" : "DISABLED", g_draw_mode);
	}
	probe_key_was_down = probe_key_down;

	// F6 cycles extrusion distance (diagnostic)
	static bool extrude_key_was_down = false;
	bool extrude_key_down = (GetAsyncKeyState(VK_F6) & 0x8000) != 0;
	if (extrude_key_down && !extrude_key_was_down)
	{
		// 2.0 (stock) -> REACH -> 500 -> 5 -> 0.3 -> 2.0. Reach comes first so the comparison that
		// matters, stock against reach, stays a single keypress rather than landing on a diagnostic
		// extreme.
		if (g_extrusion_override == 0.f)
		{
			g_extrusion_override = k_stencil_shadow_reach_extrusion;
		}
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
			// A step SHORTER than the caster itself (a biped is 0.705 wu tall), so the volume barely
			// leaves the caster and the receiver area it covers collapses. That separates leaks caused
			// by an open volume from leaks caused by its reach: an opening only produces a wrong count
			// where it falls between the eye and a receiver inside the volume, so if the leaks shrink
			// with the extrusion the openings are the defect and the length is only the exposure.
			g_extrusion_override = 0.3f;
		}
		else
		{
			g_extrusion_override = 0.f;
		}
		// Every mode change re-arms the per-caster diagnostics, so entering a mode always yields fresh
		// telemetry from the CURRENT scene rather than whatever the counters caught in the first few
		// milliseconds of the process.
		stencil_shadow_reach_reset_logs();

		// Print the RAW override, not the mode name: the on-screen text below is scrollback and
		// persists after later presses move the mode on, so it is not evidence of the current mode.
		event(_event_status, "rasterizer:dx9:stencil: extrusion override=%.1f (0=STOCK -3=REACH; anything else is a literal wu distance)",
			g_extrusion_override);

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
			event(_event_status, "rasterizer:dx9:stencil: extrusion=REACH-CLIP usable=%d extrude=%gwu reach=%gwu (infinite volume, per-pixel depth bound)",
				usable ? 1 : 0, k_stencil_shadow_reach_extrusion_distance,
				k_stencil_shadow_reach_distance);
		}
		else
		{
			// On screen unconditionally, like the reach branch above: the mode is only discoverable
			// while looking at the artefact, and reading the log file mid-game is not that.
			const real32 shown = g_extrusion_override == 0.f
				? k_stencil_shadow_extrusion_distance : g_extrusion_override;
			addDebugText("stencil extrusion: %.3f wu%s", shown,
				g_extrusion_override == 0.f ? " (STOCK, tag debug's value)" : " (fixed)");
			event(_event_status, "rasterizer:dx9:stencil: extrusion=%.3fwu", shown);
		}
	}
	extrude_key_was_down = extrude_key_down;
}
