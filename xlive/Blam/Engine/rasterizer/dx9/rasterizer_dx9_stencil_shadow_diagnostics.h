#pragma once

// Per-frame caster-loop telemetry — the `stencil dbg:` line and its counters.
//
// WHY A STRUCT AND NOT A PILE OF LOCALS
// -------------------------------------
// These twenty counters were function-locals declared at the top of a ~1300-line render hook,
// incremented from twenty scattered points, and consumed a thousand lines later. That made the hook
// impossible to break up: any extraction had to carry the whole set with it.
//
// THE COUNTERS MUST ACCOUNT FOR THEMSELVES
// ----------------------------------------
// Every field except `lod_fallback` is a REJECTION REASON, and they are meant to sum:
//
//     iterated − (all rejection reasons) − drawn  ==  0
//
// That is the `balance=` term in the log line, and a non-zero value is not cosmetic — it means a
// caster left the loop through a path that increments nothing, so the line is UNDER-REPORTING and
// cannot be trusted to explain a missing shadow. `lod_fallback` is deliberately outside the sum: it
// is informational (a shadow cast from a LOD level the engine did not request), not a drop.
//
// **If you add a rejection path to the caster loop, add a counter and put it in the sum.** The
// balance term exists to catch exactly that omission.

#include "cseries/cseries.h"

/* structures */

struct s_stencil_shadow_frame_stats
{
	int32 iterated;			// EVERY object the iterator yields, before any rejection
	int32 no_definition;
	int32 shadowless;		// hidden, or flagged shadowless
	int32 far_culled;
	int32 lod_fail;			// hlmt LOD block failed validation — td's fade did not run
	int32 no_lighting;		// render_object_cache_get_lighting returned NULL
	int32 cinematic;		// of those, how many took the CINEMATIC arm (flags bit 24)
	int32 shallow;			// shadow_direction shallower than the variants-path clamp (z > -0.6)
	real32 shallowest_z;	// the shallowest seen — read the VALUE, not just the count
	int32 opacity;			// faded below the draw threshold
	int32 no_model;
	int32 no_render_model;
	int32 non_manifold;		// rejected by the invalid_section_pair_bits gate
	int32 no_matrix;
	int32 no_sections;
};

/* prototypes */

// Zero the stats for a new pass. `shallowest_z` starts at -1.0, not 0 — it tracks a maximum over
// negative values, so a zero start would report "no shallow casters" as the shallowest reading.
void stencil_shadow_stats_reset(s_stencil_shadow_frame_stats* stats);

// Emit `stencil dbg:` and the visibility-list probe. Called on a throttle from the caster loop's
// tail; the throttle lives here so the hook does not carry a frame counter.
void stencil_shadow_stats_report(
	const s_stencil_shadow_frame_stats* stats,
	int32 volumes_drawn,
	bool masking_pass,
	int32 lod_fallbacks);
