#pragma once

// Debug-mode STATE and the key handling that drives it — F6 / F7 / F8.
//
// These three values are read from a dozen places across the volume pass, the draw, and the apply.
// Before it. 630 they were file-statics in the middle of the render code, which meant the key
// handling and the state it set were ~1000 lines apart. They are here behind accessors instead.
//
// NOT here: `g_stencil_shadow_quad_winding_flip`. It is also a debug toggle, but it is read inside
// the per-quad emission loop — the hottest path in the system — so it stays a file-static beside the
// code that reads it. It is meant to be flipped from the debugger anyway, not from a key.
//
// The keys are F6/F7/F8 because F10/F9/F2/F1 are taken by other parts of the project.

#include "cseries/cseries.h"

/* prototypes */

// UI-phase hook: key handling ONLY. Drawing happens in the render-layer hook, where the scene
// depth-stencil is still bound — the UI phase has no functional stencil (verified).
void stencil_shadow_debug_update(void);

// F7: 0 = real shadows, 1 = translucent red volume visualisation, 2 = stencil plumbing probe.
int32 stencil_shadow_debug_draw_mode(void);

// F6: 0 = td's stock 2.0 wu. Positive values are literal distances; NEGATIVE values are mode
// sentinels (−3 selects reach clip). A caller that treats a sentinel as a literal distance gets a
// −3 wu extrusion, which is why every read site checks the sentinel before falling through.
real32 stencil_shadow_debug_extrusion_override(void);

// F8 master latch. Also the signal other systems use to suppress Vista's own shadow renderers —
// declared in rasterizer_dx9_stencil_shadows.h for those callers, defined here beside the state.
bool stencil_shadow_active(void);
