#pragma once

// ENVIRONMENT (BSP cluster) stencil shadows - the level's own geometry casting per dynamic light.
// The tunables and the selection test; the volumes go into the per-light buffer render_lights.cpp
// already owns, so this tier never clears, applies, or selects lights. It is PARKED AT STAGE 2 -
// design, evidence and the reason why: docs/15-cluster-tier-redesign.md.

#include "cseries/cseries.h"
#include "math/real_math.h"

/* constants */

// EXTRUSION as a multiple of the light's radius, NOT the model tier's flat 1024. A cluster is its
// own receiver set: geometry past the light's radius is unlit and has nothing to shadow, so R past
// the caster always suffices and 2x is margin on that bound. The model tier's flat distance would
// spray a map-length quad from every silhouette edge of a room.
static const real32 k_stencil_shadow_environment_extrusion_scale = 2.f;

// Floor under the above so a very small light still yields a usable volume.
static const real32 k_stencil_shadow_environment_extrusion_minimum = 2.f;

// SELF-SHADOW BIAS for cluster volumes, 12x the model tier's. Cluster casters are the visible world
// seen from 5-100+ wu, where depth precision is orders coarser than at model range: the near cap
// stays coplanar with the surface that generated it and flips per triangle, which reads as large
// world-aligned shards. shadow_extrude.fx documents the mechanism and why the bias pushes away from
// the light. Too small and the shards return; too large and a shadow starts visibly off its wall.
static const real32 k_stencil_shadow_environment_self_shadow_bias = 0.25f;

/* prototypes */

// Does the light's sphere touch the bounds? This tier's cluster selection test, applied on top of the
// engine's own per-light cluster list - and the same predicate the lightmap tier's reach cull applies
// per triangle, so the two agree on what "the light reaches this" means at both scales.
bool stencil_shadow_light_touches_bounds(
	const real_rectangle3d* bounds,
	const real_point3d* light_position,
	real32 light_radius);
