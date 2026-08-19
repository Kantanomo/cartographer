#pragma once

// REACH CLIP — the per-pixel bound on how far a stencil shadow volume may reach.
//
// WHY IT IS ITS OWN MODULE
// ------------------------
// The port is stuck between two measured facts: a finite extrusion puts the FAR CAP inside the
// scene, where it grazes receiver geometry and fragments the shadow (it. 525/539 — and tag debug
// has the same problem, it. 544 check 3); an effectively-infinite extrusion has no far cap and is
// visually clean, but the volume then reaches forever and leaks through every wall behind the
// caster.
//
// DYNAMIC and CLIPPED both tried to place a finite cap *somewhere better*. Any finite cap is in the
// scene somewhere, so both relocated the artefact instead of removing it (td-do-not-fix.md entry
// 15). This runs the volume infinite and bounds its REACH per pixel instead — it clips the PIXEL,
// not the geometry, so a pixel's stencil crossings are ALL counted or ALL discarded and closure is
// untouched.
//
// It is separated out because it. 600 established its structural ceiling: a per-caster bound cannot
// distinguish "shadow continuing onto a lower surface" from "shadow leaking through a floor" —
// both are "receiver further along the light". Only per-pixel occlusion (a shadow map) could. So
// this is the part of the system most likely to be replaced wholesale, and keeping it in one file
// makes that a swap rather than surgery.
//
// It also owns a piece of shared GPU state — sampler 0, bound to the engine's depth-as-colour MRT
// target — and that has bitten twice (it. 560, it. 568). Bind and release live next to each other
// here for that reason.
//
// Tunables: rasterizer_dx9_stencil_shadow_tunables.h. Shader: shaders/shadow_reach_clip.fx.

#include "objects/objects.h"
#include "math/real_math.h"

/* prototypes */

// Create / destroy the ps_3_0. Failure is non-fatal: it disables the mode rather than the system.
bool stencil_shadow_reach_shader_create(IDirect3DDevice9Ex* device);
void stencil_shadow_reach_shader_dispose(void);
bool stencil_shadow_reach_shader_ready(void);

// True while a caster's constants are encoded and the mode should draw through the reach shader.
bool stencil_shadow_reach_is_active(void);

// Force it inactive. it. 636 — `stencil_shadow_section_draw` consults `is_active()`, and this flag
// is PER CASTER: the encode sets it false then true, so after a volumes pass it retains whatever the
// LAST caster left. Any other tier that draws through `section_draw` without encoding its own reach
// would therefore clip its volumes against a bound describing a different caster and a different
// light. The dynamic light tier calls this before drawing for exactly that reason.
void stencil_shadow_reach_deactivate(void);

// Reset the log budgets. Called on every F6 press so entering the mode ALWAYS produces fresh
// evidence (it. 581 — a per-process cap is consumed by one caster in milliseconds at ~500 fps).
void stencil_shadow_reach_reset_logs(void);

// Per-caster encode: traces the caster's own receiver, fits the bound, and packs the camera basis.
// Returns whether reach mode is USABLE (selected and the SM3 pair exists) — the caller needs that
// to choose the extrusion distance, and it is deliberately distinct from "active", which also
// requires the camera to have validated.
bool stencil_shadow_reach_encode(
	const object_datum* object,
	datum object_index,
	const real_point3d& toward_light_world,
	real32 extrusion_override,
	bool sm3_vertex_ready);		// D3D9 forbids mixing shader models, so reach needs the SM3 VS too

// Bind the reach pixel shader, its constants, and the depth target to sampler 0.
void stencil_shadow_reach_bind(IDirect3DDevice9Ex* device);

// Release sampler 0 THROUGH THE ENGINE'S CACHED SETTER. Never use a raw device->SetTexture here —
// it. 568 did and desynced the engine's redundancy cache, so every draw after the first sampled
// nothing while the engine believed s0 was populated (which is what corrupted the wall decals).
void stencil_shadow_reach_unbind(void);
