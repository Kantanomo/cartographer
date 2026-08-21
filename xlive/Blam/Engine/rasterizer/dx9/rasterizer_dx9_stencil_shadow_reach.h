#pragma once

// REACH CLIP - the per-pixel bound on how far a stencil shadow volume may reach. It clips the PIXEL,
// not the geometry, so closure is untouched. Its own module because it owns shared GPU state - sampler
// 0 on the engine's depth target - and because its ceiling makes it the likeliest part to be replaced
// wholesale. Both, and the shader in shaders/shadow_reach_clip.fx: docs/05-reach-clip.md.

#include "objects/objects.h"
#include "math/real_math.h"

/* prototypes */

// Create / destroy the ps_3_0. Failure is non-fatal: it disables the mode rather than the system.
bool stencil_shadow_reach_shader_create(IDirect3DDevice9Ex* device);
void stencil_shadow_reach_shader_dispose(void);
bool stencil_shadow_reach_shader_ready(void);

// True while a caster's constants are encoded and the mode should draw through the reach shader.
//
// The flag is PER CASTER and STICKY: the encode sets it false then true, so after a volumes pass it
// retains whatever the last caster left. `stencil_shadow_section_draw` consults it, but only for
// DIRECTIONAL draws - the point-light tiers are excluded there, which is what currently keeps them
// from clipping against a bound describing a different caster and a different light. A directional
// caller that does not encode its own reach would inherit one, so encode per caster.
bool stencil_shadow_reach_is_active(void);

// Reset the log budgets. Called on every F6 press so entering the mode always produces fresh evidence;
// a per-process cap is consumed by one caster in milliseconds at several hundred fps.
void stencil_shadow_reach_reset_logs(void);

// Per-caster encode: traces the caster's own receiver, fits the bound, and packs the camera basis.
// Returns whether reach mode is USABLE (selected and the SM3 pair exists), which the caller needs to
// choose the extrusion distance. Deliberately distinct from "active", which also requires the camera
// to have validated.
bool stencil_shadow_reach_encode(
	const object_datum* object,
	datum object_index,
	const real_point3d& toward_light_world,
	real32 extrusion_override,
	bool sm3_vertex_ready);		// D3D9 forbids mixing shader models, so reach needs the SM3 VS too

// Bind the reach pixel shader, its constants, and the depth target to sampler 0.
void stencil_shadow_reach_bind(IDirect3DDevice9Ex* device);

// Release sampler 0 THROUGH THE ENGINE'S CACHED SETTER. Never use a raw device->SetTexture here: it
// desyncs the engine's redundancy cache, so every draw after the first samples nothing while the
// engine believes s0 is populated, which is what corrupted the wall decals.
void stencil_shadow_reach_unbind(void);
