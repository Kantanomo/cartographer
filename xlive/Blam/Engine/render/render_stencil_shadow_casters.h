#pragma once

// Caster ELIGIBILITY for stencil shadows — the per-object questions the volume pass asks before it
// builds anything: how faded is this caster's shadow, and is its geometry a closed manifold?
//
// WHY THIS IS IN render/ AND NOT rasterizer/dx9/
// ---------------------------------------------
// Because that is where tag debug puts it. `render_object_should_cast_shadow` is
// `render/render_objects.cpp` (td 0x103740) and `render_lod_shadow_casting_models_iterate` is the
// LOD module (td 0xD6F60) — caster selection is a RENDER-layer concern there, decided before the
// rasterizer is involved at all. The same argument that kept the facing test and the skinning
// rasterizer-side puts this here.
//
// WHAT IS STILL IN THE RASTERIZER, AND WHY
// ----------------------------------------
// The caster LOOP itself — object iteration, the rejection gates, the shadow-direction derivation
// and the LOD/section resolution — is still inside `stencil_shadow_render_layer_hook`. Extracting it
// means restructuring ~200 lines of control flow whose `continue` paths are interleaved with twenty
// telemetry counters, and that is the one part of this refactor that can change behaviour SILENTLY.
// It was deliberately left for a pass that can be tested (it. 632; see td-refactor-trace.md).

#include "models/render_model_definitions.h"

/* structures */

// td's per-model LOD block, `hlmt` render-model reference + 40. Field names and offsets recovered
// from td's own use in render_lod_compute_model_alpha (td 0xD5900).
struct s_model_level_of_detail
{
	real32 disappear_distance;			// td lod+0  -- model fade end (<= 0 disables model fade)
	real32 begin_fade_distance;			// td lod+4  -- model fade start
	int32 pad;							// td lod+8
	real32 reduce_to_lod_distance[5];	// td lod+12 -- l1 super low .. l5 super high
	int32 pad_1;						// td lod+32
	uint16 shadow_fade_lod_index;		// td lod+36 -- which reduce_to distance gates shadows
	uint16 pad_2;
};

/* prototypes */

// td's two-term shadow alpha: `min(model_alpha, distance_fade)`, both geometric.
// An object that is itself fading out drags its shadow down with it — without the model term, an
// object past `begin_fade_distance` kept casting a full-strength shadow while it faded away.
// Returns false when the caster is beyond its cutoff entirely (no shadow at all).
bool stencil_shadow_compute_shadow_alpha(
	const tag_reference* model_definition,
	real32 distance,
	real32* out_alpha);

// The manifold gate. td enforces this TWICE — a failed check sets `_render_lod_model_no_shadow_bit`,
// which removes the model from the caster iteration, and
// `render_model_check_shadow_manifold_by_section_pair` (td 0x186960) runs again at draw time. A
// non-manifold mesh cannot produce a closed volume, so its counts never cancel.
bool stencil_shadow_model_is_manifold(
	const render_model_definition* render_model,
	datum object_index);

// Reset this module's per-map log budgets. Called by stencil_shadow_cache_clear — a latch belongs to
// whichever module prints it (the ownership rule from it. 623).
void render_stencil_shadow_casters_reset_diagnostics(void);
