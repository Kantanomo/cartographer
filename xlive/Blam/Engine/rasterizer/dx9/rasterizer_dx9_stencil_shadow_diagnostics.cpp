#include "stdafx.h"
#include "rasterizer_dx9_stencil_shadow_diagnostics.h"

#include "rasterizer_dx9_stencil_shadow_debug_view.h"
#include "rasterizer_dx9_stencil_shadow_tunables.h"
#include "networking/network_event.h"

/* public code */

void stencil_shadow_stats_reset(s_stencil_shadow_frame_stats* stats)
{
	memset(stats, 0, sizeof(*stats));
	// -1.0, not 0: this tracks a MAXIMUM over negative values, so a zero start would report
	// "no shallow casters" as itself the shallowest reading.
	stats->shallowest_z = -1.f;
}

// Throttled to one report per 3600 calls. The throttle lives here rather than in the caster loop so
// the hook does not carry a frame counter of its own - and so that "how often does this print" is a
// property of the diagnostic, not of its caller.
void stencil_shadow_stats_report(
	const s_stencil_shadow_frame_stats* stats,
	int32 volumes_drawn,
	bool masking_pass,
	int32 lod_fallbacks)
{
	static uint32 frame = 0;
	if (++frame % 3600 != 0)
	{
		return;
	}

	// `balance` must be 0. Any other value means a caster left the loop through a path that increments
	// nothing, so this line is under-reporting and cannot be trusted to explain a missing shadow.
	// lodfail is deliberately absent from the sum, being informational rather than a drop.
	int32 balance = stats->iterated
		- (stats->no_definition + stats->shadowless + stats->no_lighting + stats->no_model + stats->far_culled
			+ stats->opacity + stats->no_render_model + stats->non_manifold + stats->no_matrix + stats->no_sections
			+ (int32)volumes_drawn);

	UNREFERENCED_PARAMETER(balance);

	// `lodsub` is the LOD-fallback count, shadows cast from a level the engine did not request;
	// distinct from `lodfail`, which is a failed LOD-block validation.
	event(_event_verbose, "rasterizer:dx9:stencil:frame: mode=%d masking=%d iter=%d nodef=%d shadowless=%d far=%d lodfail=%d lodsub=%d nolighting=%d cinematic=%d shallow=%d worst_z=%.3f opac=%d nomodel=%d normodel=%d nonmanifold=%d nomatrix=%d nosec=%d drawn=%d balance=%d",
		stencil_shadow_debug_draw_mode(), masking_pass ? 1 : 0, stats->iterated, stats->no_definition,
		stats->shadowless, stats->far_culled, stats->lod_fail, lod_fallbacks,
		stats->no_lighting, stats->cinematic, stats->shallow,
		stats->shallowest_z, stats->opacity, stats->no_model, stats->no_render_model, stats->non_manifold,
		stats->no_matrix, stats->no_sections, volumes_drawn, balance);
}
