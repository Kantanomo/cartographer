#pragma once

// Per-frame caster-loop telemetry - the `rasterizer:dx9:stencil:frame` line and its counters.

#include "cseries/cseries.h"

/* structures */

// THE COUNTERS MUST ACCOUNT FOR THEMSELVES. The fields marked "drop" are the rejection reasons, and
// they are meant to sum:
//
//     iterated - (every drop) - drawn  ==  0
//
// That is the `balance=` term in the log line, and a non-zero value is not cosmetic: it means a caster
// left the loop through a path that increments nothing, so the line is UNDER-REPORTING and cannot be
// trusted to explain a missing shadow. The unmarked fields are informational and stay out of the sum -
// a caster can be counted `cinematic`, `shallow` or `lod_fail` and still draw.
//
// If you add a rejection path to the caster loop, add a counter, mark it, and put it in the sum. The
// balance term exists to catch exactly that omission.
struct s_stencil_shadow_frame_stats
{
	int32 iterated;			// EVERY object the iterator yields, before any rejection
	int32 no_definition;	// drop
	int32 shadowless;		// drop: hidden, or flagged shadowless
	int32 far_culled;		// drop
	int32 no_lighting;		// drop: render_object_cache_get_lighting returned NULL
	int32 opacity;			// drop: faded below the draw threshold
	int32 no_model;			// drop
	int32 no_render_model;	// drop
	int32 non_manifold;		// drop: rejected by the invalid_section_pair_bits gate
	int32 no_matrix;		// drop
	int32 no_sections;		// drop

	// informational - a caster counted here can still draw
	int32 lod_fail;			// hlmt LOD block failed validation, so the fade did not run
	int32 cinematic;		// flagged for cinematic lighting
	int32 shallow;			// shadow_direction shallower than the variants-path clamp (z > -0.6)
	real32 shallowest_z;	// the shallowest seen - read the VALUE, not just the count
};

/* prototypes */

// Zero the stats for a new pass. `shallowest_z` starts at -1.0, not 0: it tracks a maximum over
// negative values, so a zero start would report "no shallow casters" as the shallowest reading.
void stencil_shadow_stats_reset(s_stencil_shadow_frame_stats* stats);

// Emit the frame line and the visibility-list probe. Called on a throttle from the caster loop's
// tail; the throttle lives here so the hook does not carry a frame counter.
void stencil_shadow_stats_report(
	const s_stencil_shadow_frame_stats* stats,
	int32 volumes_drawn,
	bool masking_pass,
	int32 lod_fallbacks);
