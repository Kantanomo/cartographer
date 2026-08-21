#pragma once

// Debug-mode state and the key handling that drives it - F6 / F7 / F8, those keys because the lower
// ones are taken elsewhere in the project. Behind accessors because a dozen places across the volume
// pass, the draw and the apply read them. Not here: `g_stencil_shadow_quad_winding_flip`, which stays
// a file-static beside the per-quad loop that reads it. The key map is in docs/09.

#include "cseries/cseries.h"

/* prototypes */

// UI-phase hook: key handling ONLY. Drawing happens in the render-layer hook, where the scene
// depth-stencil is still bound; the UI phase has no functional stencil.
void stencil_shadow_debug_update(void);

// F7: 0 = real shadows, 1 = translucent red volume visualisation, 2 = stencil plumbing probe.
int32 stencil_shadow_debug_draw_mode(void);

// F6: 0 = tag debug's stock 2.0 wu. Positive values are literal distances; NEGATIVE values are mode
// sentinels (-3 selects reach clip). A caller that treats a sentinel as a literal distance gets a
// -3 wu extrusion, which is why every read site checks the sentinel before falling through.
real32 stencil_shadow_debug_extrusion_override(void);

// F8 master latch. Also the signal other systems use to suppress Vista's own shadow renderers;
// declared in rasterizer_dx9_stencil_shadows.h for those callers, defined here beside the state.
bool stencil_shadow_active(void);
