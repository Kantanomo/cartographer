#pragma once

// DYNAMIC LIGHT TIER — stencil shadows cast by in-game light sources (tag debug's render layer 13).
//
// The lightmap tier we ship reproduces td's layer 6: one synthetic light derived from each object's
// lighting sample. This is the OTHER tier — shadows from real `'ligh'` tags: lamps, flashlights,
// muzzle flashes, anything the scenario places.
//
// WHY THIS IS A DRIVER, NOT A SECOND SHADOW SYSTEM
// ------------------------------------------------
// tag debug's `rasterizer_model_section_draw` (td 0x10F0E0) case-folds layers 6 and 13 onto ONE
// volume generator. The generator is light-agnostic; the tiers differ only in their light setup,
// their caster rule, and their application. So this module gathers lights and casters and then
// calls the SAME draw API the lightmap tier uses — `stencil_shadow_section_get`,
// `stencil_shadow_build_facing_bitvector`, `stencil_shadow_section_draw`,
// `stencil_shadow_apply_and_clear`. None of those were modified for this tier.
//
// DELIBERATELY A SEPARATE OPERATION
// ---------------------------------
// It owns its own pass, its own stencil clear, its own apply, its own telemetry line
// (`stencil dyn:`) and its own on/off switch. Nothing in the lightmap tier's path branches on it.
// That is so it can be disabled outright, or diagnosed without disturbing a tier that is known good.
//
// **DEFAULT OFF, and NEVER RUN.** Written against the reference with no runtime verification.
// Press F5 to enable, or set `g_stencil_shadow_dynamic_enabled` from a debugger.
// Read docs/12-dynamic-light-tier.md — especially "Troubleshooting" — before the first run.

#include "cseries/cseries.h"

/* tunables — kept HERE rather than in the shared tunables header, because this subsystem is
   deliberately self-contained: disabling it should not mean reasoning about a shared file. */

// HARD compile-time gate. While this is false the tier is completely inert — the pass early-returns
// and F5 refuses to enable it, so it cannot be switched on by accident. Set true to make the tier
// available again; k_stencil_shadow_dynamic_default_on then decides whether it starts on.
// it. 652: FALSE — the whole of layer 13 is inert, dynamic light shadows AND the BSP/cluster tier
// that runs inside this tier's per-light loop. F5 refuses and says so.
//
// Parked, not abandoned: the cluster tier has an unresolved defect (volumes still cover the map after
// four separate fixes — see it. 645-651). Set true to resume; nothing else needs changing.
static const bool k_stencil_shadow_dynamic_available = false;

// Whether it starts enabled once available. F5 toggles from here.
static const bool k_stencil_shadow_dynamic_default_on = false;

// Draw the MODEL casters. Set false to run this tier's light gather, stencil and apply for the
// CLUSTER volumes alone.
//
// The isolation matters on the cluster tier's first runs: the model half has known appearance
// defects whose fix (it. 641, per-light stencils) has itself never been tested, so a bad frame with
// both halves on is ambiguous between the two. F4 removes the clusters; this removes the models, so
// either half can be looked at by itself.
static const bool k_stencil_shadow_dynamic_draw_model_casters = true;

// Lights shadowed per frame, best-scoring first. OURS, not td's — td shadows every visible light,
// but each one here costs a full stencil clear, a volume pass and an apply.
static const int32 k_stencil_shadow_dynamic_max_lights = 2;

// Casters per light. Separate from the lightmap tier's frame cap: a single lamp with many objects
// around it should not consume the whole frame's budget.
static const int32 k_stencil_shadow_dynamic_max_casters_per_light = 8;

// Ignore lights further than this from the camera. A cost bound td did not need — it had a
// per-light visibility gather (`render_visibility_find_shadow_casting_objects`) we do not implement.
static const real32 k_stencil_shadow_dynamic_max_light_distance = 30.f;

// How far the volumes extrude. td pushes a flat 1024.0 for dynamic lights (render_dynamic_light,
// td 0x11D452) and we now follow it.
//
// it. 640 — THE RADIUS THEORY WAS WRONG, AND THE FIRST RUN KILLED IT.
//
// The original design used the LIGHT'S OWN RADIUS, reasoning that beyond it the light contributes
// nothing so a longer shadow is a shadow of light that was not there. That confuses two different
// distances. The light's radius bounds where the light REACHES; the extrusion has to cover the
// distance from the CASTER to whatever RECEIVES its shadow, and for a caster standing near a light
// those are unrelated — a lamp with a 3 wu radius still throws a biped's shadow the length of a room.
//
// The failure was exactly diagnostic. A caster's highest parts are furthest from the floor, so their
// volumes need the LONGEST extrusion to reach it — a short extrusion drops the HEAD first and leaves
// mid-height parts as detached fragments where their volumes only just arrive. That is precisely
// what the first run showed: a headless silhouette with floating pieces.
//
// The leak this was trying to avoid is already handled differently in this tier: the apply is
// SCISSORED to the light's projected screen bounds, so a volume that reaches further than the light
// does cannot darken anything outside the light's own area. That containment is what makes td's
// generous value safe here without reach-clip.
static const real32 k_stencil_shadow_dynamic_extrusion = 1024.f;

// How dark a shadowed pixel goes, before the light's own fade terms scale it.
//
// td does not have this number: it MASKS the light's accumulation, so a shadowed pixel receives
// exactly none of that light. We darken the composite instead (the reason is in the doc), which
// means this is a tuning value with no reference to check against. Start conservative — an
// over-dark dynamic shadow reads as a hole in the world.
static const real32 k_stencil_shadow_dynamic_darkness = 0.35f;

// A light contributing less than this is not worth a pass.
static const real32 k_stencil_shadow_dynamic_min_fade = 0.05f;

/* prototypes */

// ONE CALL, PER LIGHT — clear, volumes, apply, for each chosen light in turn. td's own structure.
//
// it. 641: this was briefly split into a mid-scene volumes pass and a late apply, so the apply could
// follow render_lights_new. That forced every light into ONE stencil buffer (it had to survive the
// gap), and the user's run showed the cost immediately: two lights on opposite sides of a caster
// both rendered at full strength, floor and ceiling, because a combined buffer cannot attribute a
// pixel to the light that marked it — and therefore cannot weight it by that light either.
//
// Per light restores the attribution. The trade is that the apply now precedes render_lights_new, so
// a light whose contribution is added there gets partially re-lit over its own shadow. That error is
// local and degrades gracefully; the combined version's did not.
//
// Call mid-scene, immediately after stencil_shadow_world_darken() — the lightmap tier has just
// retired its counts, and c0-c3 (which the extrusion shader INHERITS) are the current window's.
// After render_lights_new they are stale and volumes draw out of place.
void stencil_shadow_dynamic_volumes_pass(void);

// Retained as a no-op so both render.cpp call sites stay valid. The apply is per light, inside the
// pass above.
void stencil_shadow_dynamic_apply(void);

// Runtime on/off. Starts at k_stencil_shadow_dynamic_default_on; F5 toggles it.
bool stencil_shadow_dynamic_enabled(void);
void stencil_shadow_dynamic_toggle(void);

// Reset this module's per-map log budgets. Called by stencil_shadow_cache_clear — a latch belongs
// to whichever module prints it.
void stencil_shadow_dynamic_reset_diagnostics(void);
