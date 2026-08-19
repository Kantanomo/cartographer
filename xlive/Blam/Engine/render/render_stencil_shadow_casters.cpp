#include "stdafx.h"
#include "render_stencil_shadow_casters.h"

#include "cache/cache_files.h"
#include "objects/objects.h"
#include "models/models.h"
#include "rasterizer/dx9/rasterizer_dx9_stencil_shadow_tunables.h"
#include "H2MOD/Modules/h2log/h2log.h"

/* globals */

// Per-map log budgets — "log the first N". LATCHES, not throttles: they stop firing forever, so
// under the it. 310 rule they must be file scope and reset per map. They are int32 rather than bool,
// which is why the documented `static bool` sweep could not see them (it. 480).
static int32 g_stencil_shadow_logged_lod_models = 0;
static int32 g_stencil_shadow_logged_manifold_models = 0;

/* public code */

// td's shadow fade (render_lod_compute_model_alpha, td 0xD5900), shadow half only:
//   cutoff = reduce_to_lod_distance[min(shadow_fade_lod_index, 4)]
//   d <= cutoff                  -> 1.0
//   cutoff < d < cutoff + 10     -> clamp01(1 - (d - cutoff) * 0.1)
//   d >= cutoff + 10             -> 0.0   (object casts nothing)
// (td then takes min(model_alpha, shadow_alpha); model_alpha is the object's own fade-out,
// which Vista already applies to the drawn model, so the shadow term is what we want here.)
//
// Returns false when the block does not look like an LOD block, so the caller can fall back
// to a fixed reach instead of trusting a bad read.
bool stencil_shadow_compute_shadow_alpha(
	const tag_reference* model_definition,
	real32 distance,
	real32* out_alpha)
{
	*out_alpha = 1.f;
	if (!model_definition)
	{
		return false;
	}
	const s_model_level_of_detail* lod =
		(const s_model_level_of_detail*)((const uint8*)model_definition + 0x28);

	// Validation. The offset itself is settled: td takes this block at hlmt+0x50, but that is
	// the TAG-BUILD layout where tag_reference is 16 bytes (5 refs = 0x50); Vista's cache
	// layout uses 8-byte references, so the same block sits at 5 * 8 = 0x28.
	//
	// Keep the checks to what actually catches a bad pointer. td itself validates NOTHING here
	// -- it just clamps the selector and indexes -- so anything stricter risks rejecting valid
	// data. An earlier version required the five distances to ascend and rejected 72 of 73
	// objects: "reduce to l1 (super low)" is the level used FARTHEST away, so the array runs
	// the other way, and unused slots are 0 besides.
	int32 fade_index = lod->shadow_fade_lod_index;
	if (fade_index > 4)
	{
		fade_index = 4;		// td: min(shadow_fade_lod_index, 4)
	}
	for (int32 i = 0; i < 5; i++)
	{
		real32 value = lod->reduce_to_lod_distance[i];
		if (!(value >= 0.f) || value > 100000.f)
		{
			return false;	// NaN, negative or absurd -> not an LOD block
		}
	}
	real32 cutoff = lod->reduce_to_lod_distance[fade_index];
	if (cutoff <= 0.f)
	{
		return false;	// model declares no shadow reach at this level
	}

	// one-shot per-model dump so the values can be eyeballed against the tag
	{
		if (g_stencil_shadow_logged_lod_models < 6)
		{
			g_stencil_shadow_logged_lod_models++;
			LOG_INFO_GAME("stencil lod: fade_index={} cutoff={:.2f} l=[{:.1f} {:.1f} {:.1f} {:.1f} {:.1f}] disappear={:.1f} begin_fade={:.1f}",
				lod->shadow_fade_lod_index, cutoff,
				lod->reduce_to_lod_distance[0], lod->reduce_to_lod_distance[1],
				lod->reduce_to_lod_distance[2], lod->reduce_to_lod_distance[3],
				lod->reduce_to_lod_distance[4],
				lod->disappear_distance, lod->begin_fade_distance);
		}
	}

	// --- shadow alpha: the 10wu fade band past the model's shadow cutoff ---
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

	// --- I2: model alpha, and td's combination of the two ---
	// render_lod_compute_model_alpha (td 0xD5900) computes BOTH alphas and finishes with
	//     *out_model_alpha  = model_alpha;
	//     *out_shadow_alpha = min(model_alpha, shadow_alpha);
	// so an object that is itself fading out drags its shadow down with it. We previously
	// implemented only the shadow half, leaving distant/fading objects casting a full-strength
	// shadow after the object had begun to disappear.
	//     if (lod->disappear_distance > 0) {
	//         if (distance < disappear) {
	//             if (distance > begin_fade)
	//                 model_alpha = 1 - (distance - begin_fade)/(disappear - begin_fade);
	//         } else model_alpha = 0;
	//     }
	real32 model_alpha = 1.f;
	if (lod->disappear_distance > 0.f)
	{
		if (distance < lod->disappear_distance)
		{
			if (distance > lod->begin_fade_distance)
			{
				real32 span = lod->disappear_distance - lod->begin_fade_distance;
				if (span > 0.f)
				{
					model_alpha = 1.f - (distance - lod->begin_fade_distance) / span;
					if (model_alpha < 0.f) model_alpha = 0.f;
					if (model_alpha > 1.f) model_alpha = 1.f;
				}
			}
		}
		else
		{
			model_alpha = 0.f;
		}
	}

	*out_alpha = shadow_alpha < model_alpha ? shadow_alpha : model_alpha;
	return true;
}

// P1-2 — td's authoritative "may this model cast a stencil shadow" gate.
// rasterizer_model_compute_fake_lighting (td 0x1F4060) sets the pass-6 draw flag only when
// render_model_check_shadow_manifold (td 0x1869F0) passes, and rasterizer_model_draw refuses
// to draw the model in pass 6 without it. That check walks EVERY pair of the object's active
// sections at ALL SIX LODs and tests bit `min + max*(max-1)/2` of the render_model's
// invalid_section_pair_bits; a single set bit disqualifies the object entirely. This is
// tag-authored data that Vista caches keep, so it replaces guessing at manifoldness.
//
// td's gate is `object+104 <= 1 && render_model_check_shadow_manifold(...)`. The first half is
// NOT omitted -- object+104 is e_object_type (proved from object_get_and_verify_type, td
// 0x85170), so `type <= 1` is _object_mask_unit, and that half is applied at the iterator via
// k_stencil_shadow_caster_mask. This function is the second half only.
bool stencil_shadow_model_is_manifold(
	const render_model_definition* model,
	datum object_index)
{
	// Is the gate even live? The struct field survives in Vista's render_model_definition, but
	// that is not proof the CACHE BUILD populates it -- isq/dsq are stripped exactly this way.
	// Log the first few models' block sizes so a permanently-zero count is visible rather than
	// silently reading as "every model is manifold".
	{
		if (model && g_stencil_shadow_logged_manifold_models < 8)
		{
			g_stencil_shadow_logged_manifold_models++;
			LOG_INFO_GAME("stencil manifold: regions={} sections={} invalid_pair_words={}",
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
			// td's clamp, verbatim from rasterizer_model_compute_region_section_indices
			// (td 0x1F4200): an out-of-range POSITIVE index clamps to the LAST permutation;
			// only a NEGATIVE one falls back to 0.
			//
			// This has to match the draw path below exactly. It previously reset any
			// out-of-range index to 0, so for an object whose permutation index exceeded a
			// region's permutation count the gate evaluated a DIFFERENT section pair than the
			// one actually drawn -- validating geometry that never reaches the volume pass.
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
				// same td clamp as region_a above (td 0x1F4200)
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
				if (word >= 0 && word < bit_word_count
					&& (*model->invalid_section_pair_bits[word] & (1u << (bit & 31))) != 0)
				{
					return false;
				}
			}
		}
	}

	return true;
}

// Render-phase draw: called from Cartographer's NATIVE render_scene (render/render.cpp)
// right after render_lights_new() — the engine's per-frame shadow phase slot, scene render
// target and depth-stencil bound. (Engine-byte patching was a dead end: the native
// render_scene replaces the engine function, so its bytes never execute.)

void render_stencil_shadow_casters_reset_diagnostics(void)
{
	g_stencil_shadow_logged_lod_models = 0;
	g_stencil_shadow_logged_manifold_models = 0;
}
