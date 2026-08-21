#include "stdafx.h"
#include "render_stencil_shadow_casters.h"

#include "cache/cache_files.h"
#include "objects/objects.h"
#include "models/models.h"
#include "rasterizer/dx9/rasterizer_dx9_stencil_shadow_tunables.h"
#include "networking/network_event.h"

/* globals */

// Per-map log budgets - "log the first N". LATCHES, not throttles: they stop firing forever, so they
// must be file scope and reset per map, or they describe only the first map of a session.
static int32 g_stencil_shadow_logged_lod_models = 0;
static int32 g_stencil_shadow_logged_manifold_models = 0;

/* public code */

// Tag debug's shadow fade (render_lod_compute_model_alpha, td 0xD5900):
//   cutoff = reduce_to_lod_distance[min(shadow_fade_lod_index, 4)]
//   d <= cutoff                  -> 1.0
//   cutoff < d < cutoff + 10     -> clamp01(1 - (d - cutoff) * 0.1)
//   d >= cutoff + 10             -> 0.0   (object casts nothing)
// then min'd with the object's own fade-out, below.
//
// Returns false when the block does not look like an LOD block, so the caller can fall back to a
// fixed reach instead of trusting a bad read.
bool stencil_shadow_compute_shadow_alpha(
	const s_model_definition* model_definition,
	real32 distance,
	real32* out_alpha)
{
	*out_alpha = 1.f;
	if (!model_definition)
	{
		return false;
	}

	// The five reduce-to distances are indexed by shadow_fade_distance, which is why they are read as
	// an array rather than by name. Contiguity is the tag layout's, not an assumption.
	static_assert(offsetof(s_model_definition, reduce_to_l5)
		- offsetof(s_model_definition, reduce_to_l1) == 4 * sizeof(real32),
		"the reduce-to distances must stay adjacent to be indexed by shadow_fade_distance");
	const real32* reduce_to_lod_distance = &model_definition->reduce_to_l1;

	// Keep the validation below to what actually catches a bad pointer. Tag debug validates nothing
	// here - it just clamps the selector and indexes - so anything stricter risks rejecting valid
	// data. Requiring the five distances to ascend, for instance, rejects almost everything: "reduce
	// to l1 (super low)" is the level used FARTHEST away, so the array runs the other way, and unused
	// slots are 0 besides.
	int32 fade_index = model_definition->shadow_fade_distance;
	if (fade_index > 4)
	{
		fade_index = 4;
	}
	for (int32 i = 0; i < 5; i++)
	{
		real32 value = reduce_to_lod_distance[i];
		if (!(value >= 0.f) || value > 100000.f)
		{
			return false;	// NaN, negative or absurd -> not an LOD block
		}
	}
	real32 cutoff = reduce_to_lod_distance[fade_index];
	if (cutoff <= 0.f)
	{
		return false;	// model declares no shadow reach at this level
	}

	// one-shot per-model dump so the values can be eyeballed against the tag
	{
		if (g_stencil_shadow_logged_lod_models < 6)
		{
			g_stencil_shadow_logged_lod_models++;
			event(_event_verbose, "rasterizer:dx9:stencil:lod: fade_index=%d cutoff=%.2f l=[%.1f %.1f %.1f %.1f %.1f] disappear=%.1f begin_fade=%.1f",
				(int32)model_definition->shadow_fade_distance, cutoff,
				reduce_to_lod_distance[0], reduce_to_lod_distance[1],
				reduce_to_lod_distance[2], reduce_to_lod_distance[3],
				reduce_to_lod_distance[4],
				model_definition->disappear_distance, model_definition->begin_fade_distance);
		}
	}

	// shadow alpha: the 10wu fade band past the model's shadow cutoff
	real32 shadow_alpha = 1.f;
	if (distance >= cutoff + k_stencil_shadow_fade_band)
	{
		shadow_alpha = 0.f;
	}
	else if (distance > cutoff)
	{
		real32 alpha = 1.f - (distance - cutoff) / k_stencil_shadow_fade_band;
		shadow_alpha = alpha < 0.f ? 0.f : (alpha > 1.f ? 1.f : alpha);
	}

	// The model's own fade-out, min'd with the shadow alpha above exactly as
	// render_lod_compute_model_alpha finishes, so an object that is fading out drags its shadow down
	// with it rather than casting at full strength while it disappears.
	real32 model_alpha = 1.f;
	if (model_definition->disappear_distance > 0.f)
	{
		if (distance < model_definition->disappear_distance)
		{
			if (distance > model_definition->begin_fade_distance)
			{
				real32 span = model_definition->disappear_distance
					- model_definition->begin_fade_distance;
				if (span > 0.f)
				{
					model_alpha = PIN(
						1.f - (distance - model_definition->begin_fade_distance) / span, 0.f, 1.f);
				}
			}
		}
		else
		{
			model_alpha = 0.f;
		}
	}

	*out_alpha = MIN(shadow_alpha, model_alpha);
	return true;
}

// Tag debug's authoritative "may this model cast a stencil shadow" gate.
// rasterizer_model_compute_fake_lighting (td 0x1F4060) sets the pass-6 draw flag only when
// render_model_check_shadow_manifold (td 0x1869F0) passes, and rasterizer_model_draw refuses to draw
// the model in pass 6 without it. That check walks every pair of the object's active sections at all
// six LODs and tests bit `min + max*(max-1)/2` of the render_model's invalid_section_pair_bits; a
// single set bit disqualifies the object entirely. Tag-authored data that Vista caches keep, so it
// replaces guessing at manifoldness.
//
// This is the second half of tag debug's gate only. The first half is the object-type test, applied
// at the iterator via k_stencil_shadow_caster_mask.
bool stencil_shadow_model_is_manifold(
	const render_model_definition* model,
	datum object_index)
{
	// Is the gate even live? The struct field survives in Vista's render_model_definition, but that
	// is not proof the cache build populates it - ISQ/DSQ are stripped exactly this way. Log the
	// first few models' block sizes so a permanently-zero count is visible rather than silently
	// reading as "every model is manifold".
	{
		if (model && g_stencil_shadow_logged_manifold_models < 8)
		{
			g_stencil_shadow_logged_manifold_models++;
			event(_event_verbose, "rasterizer:dx9:stencil:manifold: regions=%d sections=%d invalid_pair_words=%d",
				model->regions.count, model->sections.count,
				model->invalid_section_pair_bits.count);
		}
	}

	if (!model || model->invalid_section_pair_bits.count <= 0 || model->regions.count <= 1)
	{
		return true;	// nothing marked, or nothing to pair
	}

	int32 region_count = 0;
	int8* region_permutation_indices = NULL;
	object_get_region_information(object_index, &region_count, &region_permutation_indices, NULL, NULL);

	const int32 bit_word_count = model->invalid_section_pair_bits.count;

	for (int32 lod = 0; lod < 6; lod++)
	{
		for (int32 region_a = 0; region_a < model->regions.count; region_a++)
		{
			const render_model_region* a = model->regions[region_a];
			if (a->permutations.count <= 0)
			{
				continue;
			}
			int32 permutation_a = (region_permutation_indices && region_a < region_count)
				? region_permutation_indices[region_a] : 0;
			// Tag debug's clamp, verbatim from rasterizer_model_compute_region_section_indices
			// (td 0x1F4200): an out-of-range POSITIVE index clamps to the LAST permutation, and
			// only a NEGATIVE one falls back to 0. It has to match the draw path exactly, or for
			// an object whose permutation index exceeds a region's permutation count this gate
			// validates a different section pair than the one actually drawn.
			if (permutation_a >= 0)
			{
				if (permutation_a > a->permutations.count - 1)
				{
					permutation_a = a->permutations.count - 1;
				}
			}
			else
			{
				permutation_a = 0;
			}
			int16 section_a = (&a->permutations[permutation_a]->l1_section_index)[lod];
			if (section_a == NONE)
			{
				continue;	// this permutation has no geometry at this LOD -> cannot pair
			}

			for (int32 region_b = region_a + 1; region_b < model->regions.count; region_b++)
			{
				const render_model_region* b = model->regions[region_b];
				if (b->permutations.count <= 0)
				{
					continue;
				}
				int32 permutation_b = (region_permutation_indices && region_b < region_count)
					? region_permutation_indices[region_b] : 0;
				// same clamp as region_a above
				if (permutation_b >= 0)
				{
					if (permutation_b > b->permutations.count - 1)
					{
						permutation_b = b->permutations.count - 1;
					}
				}
				else
				{
					permutation_b = 0;
				}
				int16 section_b = (&b->permutations[permutation_b]->l1_section_index)[lod];
				if (section_b == NONE)
				{
					continue;
				}

				int32 low = section_a < section_b ? section_a : section_b;
				int32 high = section_a < section_b ? section_b : section_a;
				int32 bit = low + high * (high - 1) / 2;
				int32 word = bit >> 5;
				// The bound is NOT redundant, though td's version of this loop has no equivalent. The
				// tool sizes this block by the pairs it actually recorded, not by the triangular range
				// the section count implies, so shipped models exist whose range overruns it - a
				// 17-section biped ships one word where the range needs five, and a 4-section one ships
				// none at all. td reads past the end for those; we skip and treat the pair as manifold.
				// The bit decides whether a unit casts a shadow at all, so a garbage 1 read out of
				// bounds silently deletes that caster's shadow. If td parity is wanted here, keep the
				// out-of-range pair manifold - do not drop the check.
				if (VALID_INDEX(word, bit_word_count)
					&& (*model->invalid_section_pair_bits[word] & (1u << (bit & 31))) != 0)
				{
					return false;
				}
			}
		}
	}

	return true;
}

void render_stencil_shadow_casters_reset_diagnostics(void)
{
	g_stencil_shadow_logged_lod_models = 0;
	g_stencil_shadow_logged_manifold_models = 0;
}
