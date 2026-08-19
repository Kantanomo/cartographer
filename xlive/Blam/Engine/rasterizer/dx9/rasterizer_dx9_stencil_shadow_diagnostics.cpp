#include "stdafx.h"
#include "rasterizer_dx9_stencil_shadow_diagnostics.h"

#include "rasterizer_dx9_stencil_shadow_debug_view.h"
#include "rasterizer_dx9_stencil_shadow_tunables.h"
#include "Util/Hooks/Hook.h"
#include "H2MOD/Modules/h2log/h2log.h"

/* public code */

void stencil_shadow_stats_reset(s_stencil_shadow_frame_stats* stats)
{
	memset(stats, 0, sizeof(*stats));
	// -1.0, not 0: this tracks a MAXIMUM over negative values, so a zero start would report
	// "no shallow casters" as itself the shallowest reading.
	stats->shallowest_z = -1.f;
}

// Throttled to one report per 3600 calls. The throttle lives here rather than in the caster loop so
// the hook does not carry a frame counter of its own — and so that "how often does this print" is a
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

		// `balance` must be 0. Any other value means a caster left the loop through a path that
		// increments nothing -- i.e. this line is under-reporting and cannot be trusted to explain a
		// missing shadow. lodfail is deliberately absent from the sum (it is informational, not a
		// drop). (it. 328)
		int32 balance = stats->iterated
			- (stats->no_definition + stats->shadowless + stats->no_lighting + stats->no_model + stats->far_culled
				+ stats->opacity + stats->no_render_model + stats->non_manifold + stats->no_matrix + stats->no_sections
				+ (int32)volumes_drawn);

		UNREFERENCED_PARAMETER(balance);

		// it. 538: `lodsub=` is the LOD-fallback count — shadows cast from a level the engine did not
		// request. Distinct from `lodfail=`, which is a failed LOD-block validation.
		LOG_INFO_GAME("stencil dbg: mode={} masking={} iter={} nodef={} shadowless={} far={} lodfail={} lodsub={} nolighting={} cinematic={} shallow={} worst_z={:.3f} opac={} nomodel={} normodel={} nonmanifold={} nomatrix={} nosec={} drawn={} balance={}",
			stencil_shadow_debug_draw_mode(), masking_pass, stats->iterated, stats->no_definition,
			stats->shadowless, stats->far_culled, stats->lod_fail, lod_fallbacks,
			stats->no_lighting, stats->cinematic, stats->shallow,
			stats->shallowest_z, stats->opacity, stats->no_model, stats->no_render_model, stats->non_manifold,
			stats->no_matrix, stats->no_sections, volumes_drawn, balance);

		// PROBE ONLY — no behaviour change. Answers the one open question blocking the
		// visibility-list caster proposal (td-caster-selection.md, it. 269-274): WHICH
		// c_visibility_collection instance is live and populated at THIS hook point.
		//
		// render_scene runs ~8 times per frame (main view, first-person, other windows) and there
		// are two instances, so an outside sample cannot answer it — only a read taken here can.
		// Layout, verified against the engine initialiser (halo2.exe 0x4BAF47):
		//   c_visibility_collection: m_lists[4] at +0x0C; objects live in m_lists[2] (it. 270)
		//   c_visibility_object_list: m_capacity +0x00, m_count +0x04 (uint16)
		// Expected capacities from that initialiser: primary list2 = 256, secondary list2 = 128 —
		// if the capacities do not read back as those, the addresses are wrong and the counts are
		// meaningless (this check is the reason capacity is logged at all).
		{
			struct s_probe { uint32 capacity; uint16 count; };
			auto read_list2 = [](uint32 collection_rva, uint32* out_capacity) -> int32
			{
				uint8* collection = Memory::GetAddress<uint8*>(collection_rva);
				if (!collection)
				{
					return -1;
				}
				const s_probe* list = *(const s_probe**)(collection + 0x0C + 2 * sizeof(void*));
				if (!list)
				{
					return -1;
				}
				*out_capacity = list->capacity;
				return (int32)list->count;
			};
			uint32 primary_capacity = 0, secondary_capacity = 0;
			int32 primary_count = read_list2(0x4D2D60, &primary_capacity);
			int32 secondary_count = read_list2(0x4D5840, &secondary_capacity);

			UNREFERENCED_PARAMETER(primary_count);
			UNREFERENCED_PARAMETER(secondary_count);

			LOG_INFO_GAME("stencil visprobe: primary count={} cap={} (expect 256) | secondary count={} cap={} (expect 128) | our iter={} drawn={}",
				primary_count, primary_capacity, secondary_count, secondary_capacity,
				stats->iterated, volumes_drawn);
		}
}
