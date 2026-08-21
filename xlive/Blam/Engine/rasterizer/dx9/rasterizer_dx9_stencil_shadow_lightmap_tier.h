#pragma once

// THE LIGHTMAP TIER - tag debug's layer 6, the shipped shadow system: every eligible object casts one
// volume against the baked world lighting. Caster selection and the per-caster pipeline; the volume
// draw every tier shares is rasterizer_dx9_stencil_shadows.cpp. It also hosts the point-light
// per-object entry, which reuses this file's resolve so the tiers cannot drift on selector semantics.

#include "cseries/cseries.h"

/* prototypes */

// Reset this module's per-map log budgets. Called by stencil_shadow_cache_clear; a latch belongs to
// whichever module prints it.
void stencil_shadow_lightmap_tier_reset_diagnostics(void);
