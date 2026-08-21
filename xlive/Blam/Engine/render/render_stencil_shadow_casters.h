#pragma once

// Caster ELIGIBILITY for stencil shadows - the per-object questions the volume pass asks before it
// builds anything: how faded is this caster's shadow, and is its geometry a closed manifold? The
// caster LOOP itself is still inside `stencil_shadow_render_layer_hook`. Why these live in render/
// while the facing test and the skinning do not: docs/08-code-map.md.

#include "models/models.h"
#include "models/render_model_definitions.h"

/* prototypes */

// Tag debug's two-term shadow alpha: `min(model_alpha, distance_fade)`, both geometric. An object that
// is itself fading out drags its shadow down with it; without the model term, an object past
// `begin_fade_distance` keeps casting a full-strength shadow while it fades away. Returns false when
// the caster is beyond its cutoff entirely (no shadow at all).
bool stencil_shadow_compute_shadow_alpha(
	const s_model_definition* model_definition,
	real32 distance,
	real32* out_alpha);

// The manifold gate. Tag debug enforces this TWICE: a failed check sets
// `_render_lod_model_no_shadow_bit`, which removes the model from the caster iteration, and
// `render_model_check_shadow_manifold_by_section_pair` (td 0x186960) runs again at draw time. A
// non-manifold mesh cannot produce a closed volume, so its counts never cancel.
bool stencil_shadow_model_is_manifold(
	const render_model_definition* render_model,
	datum object_index);

// Reset this module's per-map log budgets. Called by stencil_shadow_cache_clear; a latch belongs to
// whichever module prints it.
void render_stencil_shadow_casters_reset_diagnostics(void);
