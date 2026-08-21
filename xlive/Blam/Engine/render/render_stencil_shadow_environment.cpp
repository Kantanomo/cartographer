#include "stdafx.h"
#include "render_stencil_shadow_environment.h"

/* prototypes */

// Distance from the light to the bounds along one axis: zero while the light is between the two
// planes, otherwise the gap to the nearer one.
static real32 stencil_shadow_axis_gap(real32 light, real32 low, real32 high);

/* public code */

bool stencil_shadow_light_touches_bounds(
	const real_rectangle3d* bounds,
	const real_point3d* light_position,
	real32 light_radius)
{
	// Closest point on the AABB to the light, compared against the radius. The test is INCLUSIVE:
	// rejecting geometry the light does reach drops a caster that should have cast, and at cluster
	// scale that is a whole wall.
	const real32 dx = stencil_shadow_axis_gap(light_position->x, bounds->x0, bounds->x1);
	const real32 dy = stencil_shadow_axis_gap(light_position->y, bounds->y0, bounds->y1);
	const real32 dz = stencil_shadow_axis_gap(light_position->z, bounds->z0, bounds->z1);

	return (dx * dx + dy * dy + dz * dz) <= (light_radius * light_radius);
}

/* private code */

static real32 stencil_shadow_axis_gap(real32 light, real32 low, real32 high)
{
	if (light < low)
	{
		return low - light;
	}
	if (light > high)
	{
		return light - high;
	}
	return 0.f;
}
