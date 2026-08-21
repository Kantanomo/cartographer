#include "stdafx.h"
#include "rasterizer_dx9_stencil_shadow_lightmap_tier.h"

#include "rasterizer_dx9_stencil_shadow_debug_view.h"
#include "rasterizer_dx9_stencil_shadow_diagnostics.h"
#include "rasterizer_dx9_stencil_shadow_reach.h"
#include "rasterizer_dx9_stencil_shadow_skinning.h"
#include "rasterizer_dx9_stencil_shadow_tunables.h"
#include "rasterizer_dx9_stencil_shadows.h"
#include "cache/cache_files.h"
#include "main/interpolator.h"
#include "math/matrix_math.h"
#include "models/models.h"
#include "objects/objects.h"
#include "render/render.h"
#include "render/render_lod_new.h"
#include "render/render_objects.h"
#include "render/render_stencil_shadow_casters.h"
#include "rasterizer_dx9_main.h"
#include "rasterizer_dx9_targets.h"
#include "Util/Hooks/Hook.h"
#include "networking/network_event.h"

/* enums */

enum e_stencil_shadow_pose_result
{
	_stencil_shadow_pose_ok,
	_stencil_shadow_pose_animate_failed,
	_stencil_shadow_pose_palette_failed
};

/* structures */

// Per-section bookkeeping the seam-stitch pass needs after the section loop has finished. Storage is
// packed 0..N-1 in DRAW order and the reader maps section -> slot through dense_of_section: keying
// the arrays by the model's own section index overflows the cap while leaving it nearly empty, since
// section indices are sparse. Remapping at the reader also keeps the per-model cross cache valid,
// because cached quads then carry stable section indices that do not shift when a different LOD or
// permutation changes which sections draw.
//
// File scope rather than a local: `facing` alone is a quarter of a megabyte, and the draw path is
// single-threaded. Slots at or past `count` are never read, so only `count` and the two index maps
// need resetting per caster.
struct s_stencil_shadow_caster_sections
{
	s_stencil_shadow_section* shadows[k_stencil_shadow_max_cross_sections];
	const real_matrix4x3* matrices[k_stencil_shadow_max_cross_sections];
	real_matrix4x3 matrix_storage[k_stencil_shadow_max_cross_sections];
	uint32 facing[k_stencil_shadow_max_cross_sections][k_stencil_shadow_facing_bitvector_words];
	int16 dense_of_section[k_stencil_shadow_section_index_count];
	int16 section_of_dense[k_stencil_shadow_max_cross_sections];
	uint32 count;
};

struct s_stencil_shadow_resolved_section
{
	int16 section_index;
	int16 section_node;						// rigid_node, or 0
	s_stencil_shadow_section* shadow;		// the built and cached volume data
	bool articulated;
};

/* globals */

// Draw mode 0's handoff from the volumes pass to the darken. Volumes are laid between the
// lightmap-indirect and SH-PRT layers; SH-PRT itself then draws UNMASKED and a single darken
// attenuates the finished lightmap term. The `mask` in these names is a vestige of the bracketed
// design that was tried and removed - masking SH-PRT as well attenuates those pixels twice, which no
// darkness constant corrects. Do not read them as a plan to reinstate it.
static bool g_stencil_shadow_masking_pass = false;			// inside the volumes pass
static bool g_stencil_shadow_mask_pending = false;			// volumes were counted; the darken should run

// One-shot latches. FILE SCOPE on purpose, reset per map: as function-local statics these would cap
// per PROCESS, so only the first map of a session would ever be described.
static bool g_stencil_shadow_warned_no_static_bind = false;
static bool g_stencil_shadow_warned_shadows_off = false;
static bool g_stencil_shadow_warned_cross_cap = false;
static bool g_stencil_shadow_warned_caster_cap = false;

// How often the LOD fallback substitutes a level the engine did not request. A non-zero count means
// some shadows are cast from a mesh that is not the one being rendered.
static bool g_stencil_shadow_warned_lod_fallback = false;
static int32 g_stencil_shadow_lod_fallbacks = 0;

// Sections dropped for classification > skinned (tag debug casts from class 4; we do not). Latched
// per class value so output is bounded, with a running count for magnitude.
static uint32 g_stencil_shadow_skipped_class_mask = 0;
static uint32 g_stencil_shadow_skipped_class_count = 0;

static s_stencil_shadow_caster_sections g_stencil_shadow_caster_sections;

// Mutable so the lightmap tier can be suppressed live to look at the other tiers alone.
static bool g_stencil_shadow_lightmap_tier_enabled = k_stencil_shadow_lightmap_tier_enabled;

/* prototypes */

static void stencil_shadow_direction_to_model_space(
	const real_matrix4x3* model_matrix,
	const real_point3d* world_direction,
	real_point3d* out_model_direction);

static void stencil_shadow_caster_sections_reset(s_stencil_shadow_caster_sections* sections);

static void stencil_shadow_caster_sections_record(
	s_stencil_shadow_caster_sections* sections,
	int32 section_index,
	s_stencil_shadow_section* shadow,
	const uint32* facing_bitvector,
	const real_matrix4x3* draw_matrix);

static bool stencil_shadow_resolve_caster_light(
	const object_datum* object,
	datum object_index,
	s_stencil_shadow_frame_stats* dbg,
	real_point3d* out_toward_light_world,
	real32* out_shadow_opacity);

static real32 stencil_shadow_select_extrusion_distance(bool reach_encoded);

static bool stencil_shadow_resolve_region_section(
	datum object_index,
	datum render_model_index,
	const render_model_definition* render_model,
	int32 region_index,
	const int8* region_permutation_indices,
	int32 region_count,
	int8 object_lod,
	s_stencil_shadow_resolved_section* out);

static e_stencil_shadow_pose_result stencil_shadow_pose_articulated_section(
	s_stencil_shadow_section* shadow,
	datum object_index,
	const render_model_definition* render_model,
	int32 region_index,
	real_vector4d* out_palette,
	int32* out_palette_count);

static int32 stencil_shadow_draw_caster_sections(
	datum object_index,
	datum render_model_index,
	render_model_definition* render_model,
	const real_point3d* toward_light_world,
	real32 extrusion_distance,
	real32 shadow_opacity,
	s_stencil_shadow_frame_stats* dbg,
	s_stencil_shadow_caster_sections* sections);

static void stencil_shadow_stitch_caster_seams(
	datum render_model_index,
	const render_model_definition* render_model,
	const s_stencil_shadow_caster_sections* sections,
	const real_point3d* toward_light_world,
	real32 extrusion_distance,
	real32 shadow_opacity);

static void stencil_shadow_point_to_model_space(
	const real_matrix4x3* m, const real_point3d* world, real_point3d* out_model);

/* public code */

void stencil_shadow_lightmap_tier_reset_diagnostics(void)
{
	g_stencil_shadow_warned_no_static_bind = false;
	g_stencil_shadow_warned_shadows_off = false;
	g_stencil_shadow_warned_cross_cap = false;
	g_stencil_shadow_warned_caster_cap = false;
	g_stencil_shadow_warned_lod_fallback = false;
	g_stencil_shadow_lod_fallbacks = 0;
	g_stencil_shadow_skipped_class_mask = 0;
	g_stencil_shadow_skipped_class_count = 0;
}

// Iterate the object table, resolve each object's render model (object def -> hlmt -> render model),
// build or fetch its cached shadow sections, and draw their volumes under the object's own cached
// lighting, falling back to a fixed sun. Called only from the volumes pass, where masking_pass ==
// false lays the counts for the world term and true lays them for the model mask.
void __cdecl stencil_shadow_render_layer_hook(void)
{
	static bool logged_first_fire = false;
	if (!logged_first_fire)
	{
		logged_first_fire = true;
		event(_event_status, "rasterizer:dx9:stencil: render hook first fire");
	}

	if (!stencil_shadow_active() || !cache_file_is_loaded())
	{
		return;
	}

	// The engine's video-settings "Shadows" option (halo2.exe 0x8E6948, written by
	// rasterizer_settings_apply_settings and read by both engine shadow systems). Honour it, but say
	// so once: otherwise a user with shadows disabled sees our volumes never appear and reads it as
	// a bug.
	const uint8* render_shadows = Memory::GetAddress<uint8*>(0x4E6948);
	if (render_shadows && *render_shadows == 0)
	{
		if (!g_stencil_shadow_warned_shadows_off)
		{
			g_stencil_shadow_warned_shadows_off = true;
			event(_event_status, "rasterizer:dx9:stencil: suppressed - engine 'render_shadows' video setting is OFF");
		}
		return;
	}

	int32 volumes_drawn = 0;

	// Per-frame caster telemetry; the counters must sum (see the diagnostics header).
	s_stencil_shadow_frame_stats dbg;
	stencil_shadow_stats_reset(&dbg);

	c_object_iterator<object_datum> object_iterator;
	object_iterator.begin((e_object_type)k_stencil_shadow_caster_mask, 0);
	while (object_iterator.next() && volumes_drawn < k_stencil_shadow_max_casters_per_frame)
	{
		// Counted before any rejection so the frame stats' arithmetic closes: iterated equals the sum
		// of every drop term plus drawn (lodfail is not a drop - it continues).
		dbg.iterated++;
		const object_datum* object = object_iterator.get_datum();
		if (!object || object->definition_index == NONE)
		{
			dbg.no_definition++;
			continue;
		}
		const datum object_index = object_iterator.get_index();

		// Tag debug iterates the models actually submitted for rendering this frame; we walk the
		// object table directly, so the engine's own "don't draw / don't shadow" state is applied by
		// hand. The shadowless bit covers both the script call and the object definition's own
		// does-not-cast-shadow flag.
		if (object->object.flags.test(_object_hidden_bit)
			|| object->object.flags.test(_object_shadowless_bit))
		{
			dbg.shadowless++;
			continue;
		}

		// Camera distance drives the shadow fade, applied once the model tag is resolved. z-fail over
		// closed volumes is camera-position-correct, so there is no near skip.
		real32 camera_distance;
		{
			const s_render* render = render_get();
			real32 dx = object->object.position.x - render->camera.point.x;
			real32 dy = object->object.position.y - render->camera.point.y;
			real32 dz = object->object.position.z - render->camera.point.z;
			camera_distance = sqrtf(dx * dx + dy * dy + dz * dz);
		}

		real_point3d toward_light_world;
		real32 shadow_opacity;
		if (!stencil_shadow_resolve_caster_light(object, object_index, &dbg,
			&toward_light_world, &shadow_opacity))
		{
			continue;
		}

		// object definition -> hlmt -> render model (the first tag_reference in hlmt)
		const _object_definition* object_definition =
			(const _object_definition*)tag_get_fast(object->definition_index);
		if (!object_definition || object_definition->model.index == NONE)
		{
			dbg.no_model++;
			continue;
		}
		const s_model_definition* model_definition =
			(const s_model_definition*)tag_get_fast(object_definition->model.index);
		if (!model_definition || model_definition->render_model.index == NONE)
		{
			dbg.no_model++;
			continue;
		}
		datum render_model_index = model_definition->render_model.index;

		// Tag debug's distance fade: past the model's own shadow reach the object casts nothing, and
		// inside the last stretch it stipples out. Only applies when the model's LOD block validates;
		// otherwise a fixed reach stands in, counted so the substitution stays visible.
		real32 shadow_alpha = 1.f;
		if (stencil_shadow_compute_shadow_alpha(model_definition, camera_distance, &shadow_alpha))
		{
			if (shadow_alpha <= 0.f)
			{
				dbg.far_culled++;
				continue;
			}
		}
		else
		{
			dbg.lod_fail++;
			if (camera_distance > k_stencil_shadow_fallback_cull_distance)
			{
				dbg.far_culled++;
				continue;
			}
		}
		// Tag debug takes min(model shadow_alpha, global shadow density); ours folds in the object's
		// own render_lighting.shadow_opacity as a third term.
		if (shadow_alpha < shadow_opacity)
		{
			shadow_opacity = shadow_alpha;
		}
		if (shadow_opacity < 0.05f)
		{
			dbg.opacity++;
			continue;
		}

		render_model_definition* render_model =
			(render_model_definition*)tag_get_fast(render_model_index);
		if (!render_model)
		{
			dbg.no_render_model++;
			continue;
		}

		// Tag debug's tag-data cast gate: models with a non-manifold section pair cast nothing.
		if (!stencil_shadow_model_is_manifold(render_model, object_index))
		{
			dbg.non_manifold++;
			continue;
		}

		const bool reach_encoded = stencil_shadow_reach_encode(
			object, object_index, toward_light_world,
			stencil_shadow_debug_extrusion_override(), stencil_shadow_sm3_vertex_shader_ready());
		const real32 extrusion_distance = stencil_shadow_select_extrusion_distance(reach_encoded);

		s_stencil_shadow_caster_sections* sections = &g_stencil_shadow_caster_sections;
		stencil_shadow_caster_sections_reset(sections);
		const int32 sections_drawn = stencil_shadow_draw_caster_sections(
			object_index, render_model_index, render_model, &toward_light_world,
			extrusion_distance, shadow_opacity, &dbg, sections);

		// The caster's own size against what we extrude: a drawn shadow much larger than
		// radius + extrusion is getting its size from somewhere neither number explains.
		{
			static uint32 size_log_frame = 0;
			if (sections_drawn > 0 && (size_log_frame++ % 600) == 0)
			{
				event(_event_verbose, "rasterizer:dx9:stencil:caster: radius=%.3f extrusion=%.3f sections=%d light=(%.3f,%.3f,%.3f) opacity=%.2f",
					object->object.radius, extrusion_distance, sections_drawn,
					toward_light_world.x, toward_light_world.y, toward_light_world.z,
					shadow_opacity);
			}
		}

		if (sections_drawn <= 0)
		{
			dbg.no_sections++;
			continue;
		}

		// The stitch gate wraps ONLY the stitching: volumes_drawn below feeds mask_pending, and gating
		// that on the stitch flag would make the mode-0 apply early-return, so shadows would vanish
		// from the shipping path while draw mode 2 kept working.
		if constexpr (k_stencil_shadow_stitch_seams)
		{
			stencil_shadow_stitch_caster_seams(render_model_index, render_model, sections,
				&toward_light_world, extrusion_distance, shadow_opacity);
		}

		// No per-object darken here: the apply runs ONCE, fullscreen, after every volume is laid -
		// stencil_shadow_world_darken does it.
		volumes_drawn++;
	}

	// Casters past the frame cap are dropped entirely rather than truncated, so a busy scene loses the
	// shadows of whatever the iterator reached last. Tag debug has no such limit.
	if (volumes_drawn >= k_stencil_shadow_max_casters_per_frame)
	{
		if (!g_stencil_shadow_warned_caster_cap)
		{
			g_stencil_shadow_warned_caster_cap = true;
			event(_event_warning, "rasterizer:dx9:stencil: hit the %d-caster frame cap; later casters drew no shadow this frame",
				(int32)k_stencil_shadow_max_casters_per_frame);
		}
	}

	stencil_shadow_stats_report(&dbg, volumes_drawn, g_stencil_shadow_masking_pass,
		g_stencil_shadow_lod_fallbacks);

	if (g_stencil_shadow_masking_pass)
	{
		g_stencil_shadow_mask_pending = volumes_drawn > 0 && stencil_shadow_debug_draw_mode() == 0;
	}
}

// One object's volumes for a POINT light, through the same region resolve the lightmap loop uses, so
// the tiers cannot drift on selector semantics. It differs only in what the point-light case needs:
// point_light = true, no LOD fade or lighting-cache machinery (these tiers carry a real light rather
// than a derived fake one), and the caller's own fade term.
int32 stencil_shadow_draw_object_volume_point_light(
	datum object_index,
	const real_point3d* light_world_position,
	real32 extrusion_distance,
	int32* out_articulated_skipped,
	real32 opacity)
{
	if (out_articulated_skipped)
	{
		*out_articulated_skipped = 0;
	}
	const object_datum* object = object_try_and_get(object_index);
	if (!object || object->definition_index == NONE)
	{
		return 0;
	}
	if (object->object.flags.test(_object_hidden_bit)
		|| object->object.flags.test(_object_shadowless_bit))
	{
		return 0;
	}
	const _object_definition* object_definition =
		(const _object_definition*)tag_get_fast(object->definition_index);
	if (!object_definition || object_definition->model.index == NONE)
	{
		return 0;
	}
	const s_model_definition* model_definition =
		(const s_model_definition*)tag_get_fast(object_definition->model.index);
	if (!model_definition || model_definition->render_model.index == NONE)
	{
		return 0;
	}
	const datum render_model_index = model_definition->render_model.index;
	const render_model_definition* render_model =
		(const render_model_definition*)tag_get_fast(render_model_index);
	if (!render_model)
	{
		return 0;
	}

	int32 region_count = 0;
	int8* region_permutation_indices = NULL;
	object_get_region_information(object_index, &region_count,
		&region_permutation_indices, NULL, NULL);
	const int8 object_lod = render_object_cache_get_level_of_detail(object_index);

	static uint32 facing_bitvector[k_stencil_shadow_facing_bitvector_words];
	int32 sections_drawn = 0;
	for (int32 region_index = 0; region_index < render_model->regions.count; region_index++)
	{
		s_stencil_shadow_resolved_section resolved;
		if (!stencil_shadow_resolve_region_section(object_index, render_model_index, render_model,
			region_index, region_permutation_indices, region_count, object_lod, &resolved))
		{
			continue;
		}
		const int16 section_node = resolved.section_node;
		s_stencil_shadow_section* shadow = resolved.shadow;
		if (shadow->articulated)
		{
			// out_articulated_skipped counts FAILURES only - either kind.
			static real_vector4d dyn_palette[k_stencil_shadow_palette_row_count];
			int32 dyn_palette_count = 0;
			if (stencil_shadow_pose_articulated_section(shadow, object_index, render_model,
				region_index, dyn_palette, &dyn_palette_count) != _stencil_shadow_pose_ok)
			{
				if (out_articulated_skipped)
				{
					(*out_articulated_skipped)++;
				}
				continue;
			}
			stencil_shadow_build_facing_bitvector(shadow, light_world_position, true, facing_bitvector);
			stencil_shadow_section_draw(shadow, facing_bitvector, light_world_position, true,
				NULL, extrusion_distance, opacity, k_stencil_shadow_self_shadow_bias,
				dyn_palette, dyn_palette_count);
			sections_drawn++;
			continue;
		}
		const real_matrix4x3* model_matrix = object_get_node_matrix(object_index, section_node);
		if (!model_matrix)
		{
			continue;
		}
		// Adopt the interpolated (render) pose when the interpolator ran; the tick pose otherwise.
		real_matrix4x3 interpolated_matrix;
		if (object_try_get_node_matrix_interpolated(object_index, section_node, &interpolated_matrix))
		{
			model_matrix = &interpolated_matrix;
		}

		real_matrix4x3 composed;
		const real_matrix4x3* draw_matrix = model_matrix;
		if (section_node >= 0 && (int32)section_node < render_model->nodes.count)
		{
			matrix4x3_multiply(model_matrix,
				&render_model->nodes[section_node]->default_inverse_matrix, &composed);
			draw_matrix = &composed;
		}

		real_point3d light_model;
		stencil_shadow_point_to_model_space(draw_matrix, light_world_position, &light_model);
		stencil_shadow_build_facing_bitvector(shadow, &light_model, true, facing_bitvector);
		stencil_shadow_section_draw(shadow, facing_bitvector, light_world_position, true,
			draw_matrix, extrusion_distance, opacity);
		sections_drawn++;
	}
	return sections_drawn;
}

void stencil_shadow_lightmap_volumes_pass(void)
{
	// The colour view draws HERE rather than at the late hook: mid-scene c0-c3 is the current
	// window's wvp, while after render_lights_new the lights path leaves stale transforms and the
	// volumes render out of place. tag debug's own colour-write debug draws during this pass too.
	g_stencil_shadow_mask_pending = false;
	if (!g_stencil_shadow_lightmap_tier_enabled || !stencil_shadow_active() || !cache_file_is_loaded())
	{
		return;
	}
	IDirect3DDevice9Ex* device = rasterizer_dx9_device_get_interface();
	if (!device)
	{
		return;
	}

	// REACH CLIP RESOURCE CONFLICT. On SM3 the engine renders depth into a COLOUR texture during
	// the lightmap-indirect stage and keeps it bound as an MRT output until after this pass - so
	// the surface reach-clip wants to SAMPLE is still a render target, which D3D9 forbids. It fails
	// silently: undefined reads make texkill never fire, and the volume renders as unbounded
	// extrusion, indistinguishable from too large a reach constant.
	//
	// Suppressing the MRT z output for this pass is safe for the counting, because on SM3 that
	// surface carries depth-as-colour and is NOT the depth-stencil buffer z-fail depends on. (In the
	// non-SM3 path the same global IS a depth-stencil surface, which is why this had to be read
	// rather than assumed.) Re-applying the target is what actually rebinds without it.
	const bool reach_mode_needs_depth_texture =
		stencil_shadow_debug_extrusion_override() == k_stencil_shadow_reach_extrusion
		&& stencil_shadow_reach_shader_ready() && stencil_shadow_sm3_vertex_shader_ready();
	const bool saved_suppress_z_target = g_dx9_dont_draw_to_depth_target_if_mrt_is_used;
	if (reach_mode_needs_depth_texture && !saved_suppress_z_target)
	{
		g_dx9_dont_draw_to_depth_target_if_mrt_is_used = true;
		rasterizer_dx9_set_target((e_rasterizer_target)*rasterizer_dx9_main_render_target_get(), 0, true);
	}


	if (stencil_shadow_debug_draw_mode() != 1)
	{
		device->Clear(0, NULL, D3DCLEAR_STENCIL, 0, 1.f, 128);
	}
	LARGE_INTEGER perf_start;
	QueryPerformanceCounter(&perf_start);

	g_stencil_shadow_masking_pass = true;
	stencil_shadow_render_layer_hook();
	g_stencil_shadow_masking_pass = false;

	// Mode 2 visualizes the mask where the lit layer would test it: opaque green wherever the counts
	// mark a pixel shadowed.
	if (stencil_shadow_debug_draw_mode() == 2)
	{
		stencil_shadow_apply_and_clear(1.f, NULL);
	}

	// CPU cost per volumes pass, accumulated and reported every ~10s.
	LARGE_INTEGER perf_end, perf_frequency;
	QueryPerformanceCounter(&perf_end);
	QueryPerformanceFrequency(&perf_frequency);
	static real64 perf_accum_ms = 0.0;
	static real64 perf_max_ms = 0.0;
	static uint32 perf_samples = 0;
	real64 elapsed_ms = (real64)(perf_end.QuadPart - perf_start.QuadPart) * 1000.0
		/ (real64)perf_frequency.QuadPart;
	perf_accum_ms += elapsed_ms;
	if (elapsed_ms > perf_max_ms)
	{
		perf_max_ms = elapsed_ms;
	}
	if (++perf_samples >= 4800)
	{
		event(_event_status, "rasterizer:dx9:stencil:perf: volumes pass avg=%.3fms max=%.3fms over %u passes",
			perf_accum_ms / perf_samples, perf_max_ms, perf_samples);

		perf_accum_ms = 0.0;
		perf_max_ms = 0.0;
		perf_samples = 0;
	}

	// hand the MRT z output back exactly as we found it. Restoring the flag alone is not
	// enough - the binding only changes on the next set_target, so re-apply it here rather than
	// leaving the rest of the frame drawing without its depth-as-colour output.
	if (reach_mode_needs_depth_texture && !saved_suppress_z_target)
	{
		g_dx9_dont_draw_to_depth_target_if_mrt_is_used = saved_suppress_z_target;
		rasterizer_dx9_set_target((e_rasterizer_target)*rasterizer_dx9_main_render_target_get(), 0, true);
	}
}

// The lightmap tier's single application - tag debug's pass 7. ONE unscissored fullscreen quad
// wherever the stencil count differs from 128. td multiplies against a constant grey; we blend
// SRCALPHA/INVSRCALPHA against black, which is algebraically the same.
//
// It runs AFTER the SH-PRT layer, so the full lightmap term is accumulated before shadowed pixels
// are darkened ONCE - matching td's layer order. Applying it earlier while SH-PRT also drew under
// an EQUAL-128 mask attenuated a shadowed pixel twice, which no darkness constant can correct.
void stencil_shadow_world_darken(void)
{
	if (!stencil_shadow_active() || !g_stencil_shadow_mask_pending)
	{
		return;
	}

	if (stencil_shadow_debug_draw_mode() == 0)
	{
		stencil_shadow_apply_and_clear(k_stencil_shadow_darkness, NULL);
	}

	// Teardown runs in every draw mode: retire the counts and clear the buffer before the
	// per-light tiers reuse it, as tag debug's layer 13 re-clears once layer 7 has consumed them.
	g_stencil_shadow_mask_pending = false;
	IDirect3DDevice9Ex* device = rasterizer_dx9_device_get_interface();
	if (device)
	{
		device->Clear(0, NULL, D3DCLEAR_STENCIL, 0, 1.f, 0);
	}
}

/* private code */

// Rotate a world-space DIRECTION into model space: d_model = R^T * d_world (uniform scale does not
// affect facing signs).
//
// DIRECTIONS ONLY. There is no translation and no scale division, which is correct for a direction
// and wrong for a position - a light POSITION needs the full inverse, p_model = R^T * (p_world - T)
// / scale, which is what stencil_shadow_point_to_model_space does for the point-light tiers.
static void stencil_shadow_direction_to_model_space(
	const real_matrix4x3* model_matrix,
	const real_point3d* world_direction,
	real_point3d* out_model_direction)
{
	const real_vector3d* forward = &model_matrix->vectors.forward;
	const real_vector3d* left = &model_matrix->vectors.left;
	const real_vector3d* up = &model_matrix->vectors.up;
	out_model_direction->x = forward->i * world_direction->x + forward->j * world_direction->y + forward->k * world_direction->z;
	out_model_direction->y = left->i * world_direction->x + left->j * world_direction->y + left->k * world_direction->z;
	out_model_direction->z = up->i * world_direction->x + up->j * world_direction->y + up->k * world_direction->z;
}

static void stencil_shadow_caster_sections_reset(s_stencil_shadow_caster_sections* sections)
{
	sections->count = 0;
	for (int32 i = 0; i < k_stencil_shadow_section_index_count; i++)
	{
		sections->dense_of_section[i] = NONE;
	}
	for (int32 i = 0; i < k_stencil_shadow_max_cross_sections; i++)
	{
		sections->section_of_dense[i] = NONE;
	}
}

// `shadow` is deliberately NOT const: the stitch pass retags matched boundary quads through the
// stored pointer (stencil_shadow_model_cross_get writes tri_right on both sides of a pair), so a
// const parameter here would only be describing the recording rather than what the record is for.
static void stencil_shadow_caster_sections_record(
	s_stencil_shadow_caster_sections* sections,
	int32 section_index,
	s_stencil_shadow_section* shadow,
	const uint32* facing_bitvector,
	const real_matrix4x3* draw_matrix)
{
	if (sections->count >= k_stencil_shadow_max_cross_sections
		|| !VALID_INDEX(section_index, k_stencil_shadow_section_index_count))
	{
		return;
	}
	const uint32 slot = sections->count;
	sections->dense_of_section[section_index] = (int16)slot;
	sections->section_of_dense[slot] = (int16)section_index;
	memcpy(sections->facing[slot], facing_bitvector,
		((shadow->plane_count + 31) / 32) * sizeof(uint32));
	sections->shadows[slot] = shadow;
	if (draw_matrix)
	{
		sections->matrix_storage[slot] = *draw_matrix;
		sections->matrices[slot] = &sections->matrix_storage[slot];
	}
	else
	{
		sections->matrices[slot] = NULL;	// articulated: CPU-skinned to world, identity node constants
	}
	sections->count++;
}

// The caster's light: the cached render_lighting.shadow_direction points the way the shadow falls,
// and our convention wants the vector TOWARD the light. render_object_cache_get_lighting tests the
// entry's lighting-valid byte - the only valid gate, since the rasterizer's own geometry-cache fields
// pass while the lighting still holds a previous tenant's values. Returns false when the object has
// no computed lighting, which means no shadow.
static bool stencil_shadow_resolve_caster_light(
	const object_datum* object,
	datum object_index,
	s_stencil_shadow_frame_stats* dbg,
	real_point3d* out_toward_light_world,
	real32* out_shadow_opacity)
{
	out_toward_light_world->x = 0.408f;
	out_toward_light_world->y = 0.408f;
	out_toward_light_world->z = 0.816f;
	*out_shadow_opacity = 1.f;

	if (object->object.flags.test(_object_uses_cinematic_lighting_bit))
	{
		dbg->cinematic++;
	}
	const render_lighting* lighting = render_object_cache_get_lighting(object_index);
	if (!lighting)
	{
		dbg->no_lighting++;
		return false;
	}

	const real_vector3d* direction = &lighting->shadow_direction;
	real32 length_squared = direction->i * direction->i
		+ direction->j * direction->j + direction->k * direction->k;
	if (length_squared > 0.001f)
	{
		out_toward_light_world->x = -direction->i;
		out_toward_light_world->y = -direction->j;
		out_toward_light_world->z = -direction->k;
		// The engine clamps this vector steep before normalizing, which bounds the normalized z at
		// -0.6; a shallower one means the lighting was sampled rather than solved, which is
		// legitimate, so the shallowest value below is the real signal rather than the count.
		real32 inverse_length = 1.f / sqrtf(length_squared);
		real32 normalized_z = direction->k * inverse_length;
		if (normalized_z > -0.6f)
		{
			dbg->shallow++;
		}
		if (normalized_z > dbg->shallowest_z)
		{
			dbg->shallowest_z = normalized_z;
		}
		*out_shadow_opacity = lighting->shadow_opacity;
		if (*out_shadow_opacity < 0.f) *out_shadow_opacity = 0.f;
		if (*out_shadow_opacity > 1.f) *out_shadow_opacity = 1.f;
	}
	// else: a zero-length shadow_direction means the blob was never populated - an object flagged for
	// cinematic lighting in a map whose script never ran cinematic_lighting_set_primary_light leaves
	// the whole 84-byte render_lighting zeroed. The direction already falls back to the fixed sun
	// above, and the OPACITY has to fall back with it, because reading 0.0 out of that same dead blob
	// yields a fully transparent shadow and silently undoes the fallback. Trust the blob's fields
	// together or not at all.
	return true;
}

// Extrusion is tag debug's finite 2 world units by default (the .w of the light block its microcode
// uploads); F6 cycles alternatives for diagnostics. Reach mode is tested by its OVERRIDE value rather
// than by `reach_encoded`: the two differ when SM3 is missing, and the sentinel would otherwise fall
// through and be used as a literal negative distance.
static real32 stencil_shadow_select_extrusion_distance(bool reach_encoded)
{
	const real32 override_distance = stencil_shadow_debug_extrusion_override();
	if (override_distance == k_stencil_shadow_reach_extrusion)
	{
		return reach_encoded ? k_stencil_shadow_reach_extrusion_distance
			: k_stencil_shadow_extrusion_distance;
	}
	return (override_distance != 0.f) ? override_distance : k_stencil_shadow_extrusion_distance;
}

// One region's drawable shadow section, resolved exactly as the Vista renderer resolves it.
//
// BOTH TIERS GO THROUGH HERE, and that is the point: the lightmap tier and the point-light tier drew
// the same object from two copies of this walk, which is a divergence waiting to happen rather than
// the parity the copies claimed.
//
// Only the ACTIVE LOD's section per region draws; iterating every section would shadow the union of
// all LOD levels and permutations, a hull larger than the rendered object. The selector semantics are
// the renderer's: the per-region byte from object_get_region_information is already variant- and
// damage-state-resolved, and the engine's own resolve consumes it directly, treating NONE as hidden
// and an out-of-range value as not drawable this frame. Tag debug's rule - clamp into range,
// substitute permutation 0 - would shadow geometry the renderer is not drawing.
//
// Returns false when this region contributes no volume, for any of the reasons above.
static bool stencil_shadow_resolve_region_section(
	datum object_index,
	datum render_model_index,
	const render_model_definition* render_model,
	int32 region_index,
	const int8* region_permutation_indices,
	int32 region_count,
	int8 object_lod,
	s_stencil_shadow_resolved_section* out)
{
	const render_model_region* region = render_model->regions[region_index];
	if (region->permutations.count <= 0)
	{
		return false;
	}
	// The byte is int8, so 0xFF reads as NONE; any other negative is garbage the engine's unsigned
	// read would treat as out of range. Either way the renderer draws nothing.
	const int32 permutation_selector = (region_permutation_indices && region_index < region_count)
		? region_permutation_indices[region_index] : 0;
	if (permutation_selector == NONE)
	{
		return false;	// region HIDDEN by the active variant / damage state
	}
	if (!VALID_INDEX(permutation_selector, region->permutations.count))
	{
		return false;	// not drawable this frame - the engine skips it too
	}
	const render_model_permutation* permutation = region->permutations[permutation_selector];
	const int16* lod_sections = &permutation->l1_section_index;

	// Tag debug indexes info->level_of_detail directly; that is the LOD the model is actually
	// rendering at. A NONE there means this region draws nothing at this level - honour it instead of
	// hunting for a substitute.
	int16 section_index;
	if (VALID_INDEX(object_lod, 6))
	{
		section_index = lod_sections[object_lod];
	}
	else
	{
		// No cached LOD for this window (the accessor yields NONE past window 3), so fall back to the
		// lowest detail level that exists - shadows do not need detail. Counted and reported, because
		// substituting a level the engine is not rendering casts the shadow from a DIFFERENT mesh,
		// which reads as shadows from invisible geometry.
		section_index = NONE;
		int32 fallback_level = NONE;
		for (int32 level = 0; level < 6 && section_index == NONE; level++)
		{
			section_index = lod_sections[level];
			fallback_level = level;
		}
		g_stencil_shadow_lod_fallbacks++;
		if (!g_stencil_shadow_warned_lod_fallback && section_index != NONE)
		{
			g_stencil_shadow_warned_lod_fallback = true;
			event(_event_warning, "rasterizer:dx9:stencil:lod: no cached LOD for this object (requested %d), substituted level %d - the shadow is cast from a mesh the engine may not be rendering",
				(int32)object_lod, fallback_level);
		}
	}
	if (section_index == NONE || section_index < 0
		|| section_index >= render_model->sections.count)
	{
		return false;
	}

	const render_model_section* section = render_model->sections[section_index];
	// Tag debug skips a section unless its vertex, triangle and part counts are all non-zero.
	if (section->section_info.total_vertex_count == 0
		|| section->section_info.total_triangle_count == 0
		|| section->section_info.total_part_count == 0)
	{
		return false;
	}
	if (section->section_info.shadow_casting_triangle_count == 0)
	{
		return false;
	}
	// A known divergence, latched per class so it stays observable but bounded: tag debug remaps
	// classification 4 to skinned and casts from it, while we drop it. Failing safe beats a wrong
	// shadow, but if this ever fires we are losing shadows tag debug would draw.
	if (section->global_geometry_classification > _geometry_classification_skinned)
	{
		const int32 dropped_class = (int32)section->global_geometry_classification;
		g_stencil_shadow_skipped_class_count++;
		const uint32 class_bit = 1u << ((uint32)dropped_class & 31);
		if ((g_stencil_shadow_skipped_class_mask & class_bit) == 0)
		{
			g_stencil_shadow_skipped_class_mask |= class_bit;
			event(_event_warning, "rasterizer:dx9:stencil: section %d SKIPPED for classification %d (>%d) - tag debug remaps class 4 to skinned and casts from it; count so far %u",
				section_index, dropped_class,
				(int32)_geometry_classification_skinned,
				g_stencil_shadow_skipped_class_count);
		}
		return false;
	}

	s_stencil_shadow_section* shadow = stencil_shadow_section_get(render_model_index, section_index);
	if (!shadow)
	{
		return false;
	}

	out->section_index = section_index;
	// rigid_node is applied on the SENTINEL, with no classification test - tag debug's rule verbatim.
	// Gating on the classification instead leaves every rigid section with a non-zero rigid_node
	// riding node 0, which detaches the shadow of a turret or hatch from the part that casts it.
	out->section_node = (section->rigid_node != NONE) ? section->rigid_node : (int16)0;
	out->shadow = shadow;
	out->articulated = shadow->articulated;
	return true;
}

// Pose an articulated section for this frame and build the c50 palette its GPU-skinned draw needs.
// After this the section's planes, VB positions and facing light are all world-space and it draws
// with identity node constants.
//
// A palette count of 0 with an OK result means the CPU path draws from the dynamic VB, which the
// animate refreshed for exactly that case. A palette FAILURE is different and must not draw: the
// animate skipped that refresh under the same condition, so the CPU fallback would put a stale pose
// on screen, and a section missing for one frame beats one posed wrongly.
//
// Both tiers pose through here so the two cannot answer that question differently; what they do with
// a failure is their own - the lightmap tier reports it, the point-light entry counts it.
static e_stencil_shadow_pose_result stencil_shadow_pose_articulated_section(
	s_stencil_shadow_section* shadow,
	datum object_index,
	const render_model_definition* render_model,
	int32 region_index,
	real_vector4d* out_palette,
	int32* out_palette_count)
{
	*out_palette_count = 0;
	if (!stencil_shadow_section_animate(shadow, object_index, render_model, region_index))
	{
		return _stencil_shadow_pose_animate_failed;
	}
	if (shadow->skinned_vb && stencil_shadow_skinned_ready())
	{
		*out_palette_count = stencil_shadow_pool_build_palette(
			object_index, render_model, region_index, shadow, out_palette);
		if (*out_palette_count == 0)
		{
			return _stencil_shadow_pose_palette_failed;
		}
	}
	return _stencil_shadow_pose_ok;
}

// One caster's volumes, section by section, through the shared region resolve above.
// Returns the number of sections drawn, and fills `sections` for the seam-stitch pass.
static int32 stencil_shadow_draw_caster_sections(
	datum object_index,
	datum render_model_index,
	render_model_definition* render_model,
	const real_point3d* toward_light_world,
	real32 extrusion_distance,
	real32 shadow_opacity,
	s_stencil_shadow_frame_stats* dbg,
	s_stencil_shadow_caster_sections* sections)
{
	static uint32 facing_bitvector[k_stencil_shadow_facing_bitvector_words];

	int32 region_count = 0;
	int8* region_permutation_indices = NULL;
	object_get_region_information(object_index, &region_count, &region_permutation_indices, NULL, NULL);

	int8 object_lod = render_object_cache_get_level_of_detail(object_index);
	int32 sections_drawn = 0;
	for (int32 region_index = 0; region_index < render_model->regions.count; region_index++)
	{
		s_stencil_shadow_resolved_section resolved;
		if (!stencil_shadow_resolve_region_section(object_index, render_model_index, render_model,
			region_index, region_permutation_indices, region_count, object_lod, &resolved))
		{
			continue;
		}
		const int16 section_index = resolved.section_index;
		const int16 section_node = resolved.section_node;
		s_stencil_shadow_section* shadow = resolved.shadow;

		// The node matrix is only needed by the static path: articulated sections are CPU-skinned to
		// world and draw with identity node constants, so a missing matrix must not drop them.
		const real_matrix4x3* model_matrix = object_get_node_matrix(object_index, section_node);
		if (!model_matrix && !shadow->articulated)
		{
			dbg->no_matrix++;
			continue;
		}

		// object_get_node_matrix returns the TICK pose while the model is rendered at the INTERPOLATED
		// one, so a fast-moving object's volume runs ahead of it. Call unconditionally and adopt only
		// when the interpolator reports it ran - it declines for objects that cannot interpolate and
		// past the teleport cutoff, where the raw matrix is the right answer.
		real_matrix4x3 interpolated_static;
		if (model_matrix
			&& halo_interpolator_interpolate_object_node_matrix(
				object_index, (int16)section_node, &interpolated_static))
		{
			model_matrix = &interpolated_static;
		}

		if (shadow->articulated)
		{
			// Static rather than stack: 201 float4s in single-threaded render code.
			static real_vector4d articulated_palette[k_stencil_shadow_palette_row_count];
			int32 articulated_palette_count = 0;
			const e_stencil_shadow_pose_result pose = stencil_shadow_pose_articulated_section(
				shadow, object_index, render_model, region_index,
				articulated_palette, &articulated_palette_count);
			if (pose != _stencil_shadow_pose_ok)
			{
				if (pose == _stencil_shadow_pose_palette_failed)
				{
					static uint32 palette_failed_log = 0;
					if ((palette_failed_log++ % 600) == 0)
					{
						event(_event_warning, "rasterizer:dx9:stencil:skinning: palette build failed for section %d - section skipped this frame (count %u)",
							section_index, palette_failed_log);
					}
				}
				continue;
			}
			stencil_shadow_build_facing_bitvector(shadow, toward_light_world, false, facing_bitvector);

			stencil_shadow_section_draw(shadow, facing_bitvector, toward_light_world, false,
				NULL, extrusion_distance,
				shadow_opacity, k_stencil_shadow_self_shadow_bias,
				articulated_palette, articulated_palette_count);
			if constexpr (k_stencil_shadow_stitch_seams)
			{
				stencil_shadow_caster_sections_record(sections, section_index, shadow,
					facing_bitvector, NULL);
			}
		}
		else
		{
			// STATIC TRANSFORM = node_world x inverse bind, as for the articulated path. Both engines
			// compose it that way: tag debug's rigid draw reads the skinning pool, whose entries are
			// already node_world x inverse_bind, and Vista's own transform-constant path sends model
			// records down the same pool branch. Binding a raw node matrix instead displaces the volume
			// by the node's offset from the model origin.
			//
			// The composed matrix feeds BOTH consumers: the facing test needs the light in the same
			// space as the plane data, so it uses the composed rotation, not the raw one.
			real_matrix4x3 composed_static;
			const real_matrix4x3* draw_matrix = model_matrix;

			// Prefer the ENGINE's pool entry: it already holds this exact product for every node of
			// every visible object, including the render-time corrections our own multiply cannot
			// reproduce. On Vista content the pool is region palettes in local node_map order, hence
			// the region index and slot translation. An invalid entry (off-screen, cinematic-lit,
			// first-person, LOD-divergent palette) falls through to the multiply.
			bool composed_from_pool = false;
			if (k_stencil_shadow_use_skinning_pool)
			{
				s_stencil_shadow_pool_ref pool_ref = {};
				if (stencil_shadow_pool_resolve(object_index, render_model, region_index, shadow, &pool_ref))
				{
					const int32 pool_slot = stencil_shadow_pool_slot_for_node(
						&pool_ref, shadow, (int32)section_node);
					if (pool_slot != NONE)
					{
						model_skinning_get_node_matrix(
							pool_ref.pool, (int16)pool_slot, (real32*)&composed_static);
						stencil_shadow_pool_parity_probe(object_index,
							(int32)section_node, render_model, &composed_static);
						draw_matrix = &composed_static;
						composed_from_pool = true;
					}
				}
			}

			if (!composed_from_pool)
			{
				// The >= 0 test guards a corrupt negative rigid_node from indexing nodes[-2] - an OOB
				// read feeding a matrix multiply, i.e. a plausible wrong transform rather than a clean
				// crash. Unreachable on valid data; corrupt data routes to the warned fallback.
				if (section_node >= 0 && (int32)section_node < render_model->nodes.count)
				{
					matrix4x3_multiply(model_matrix,
						&render_model->nodes[section_node]->default_inverse_matrix, &composed_static);
					draw_matrix = &composed_static;
				}
				// Falling back to the raw node matrix displaces the volume by the node's bind offset,
				// so it must not be silent.
				else if (!g_stencil_shadow_warned_no_static_bind)
				{
					g_stencil_shadow_warned_no_static_bind = true;
					event(_event_warning, "rasterizer:dx9:stencil: static section %d node %d >= nodes.count %d - no inverse bind, volume will be displaced",
						section_index, (int32)section_node, render_model->nodes.count);
				}
			}

			// The facing test runs in section space (where the plane data lives); the shader light
			// stays world-space, since extrusion happens after the node transform.
			real_point3d toward_light_model;
			stencil_shadow_direction_to_model_space(draw_matrix, toward_light_world, &toward_light_model);
			stencil_shadow_build_facing_bitvector(shadow, &toward_light_model, false, facing_bitvector);
			stencil_shadow_section_draw(shadow, facing_bitvector, toward_light_world, false,
				draw_matrix, extrusion_distance, shadow_opacity);
			if constexpr (k_stencil_shadow_stitch_seams)
			{
				stencil_shadow_caster_sections_record(sections, section_index, shadow,
					facing_bitvector, draw_matrix);
			}
		}

		// Sections past the seam-stitch array still draw their own volume but are excluded from
		// stitching, which leaks light through any seam touching them. The cap is reached by SPARSE
		// section indices, not by drawing too many sections - a model with 161 sections across 9
		// regions draws at most 9, spread over the whole index range. The fix would be a dense 0..N-1
		// key, not a bigger array.
		if constexpr (k_stencil_shadow_stitch_seams)
		if (section_index >= k_stencil_shadow_max_cross_sections)
		{
			if (!g_stencil_shadow_warned_cross_cap)
			{
				g_stencil_shadow_warned_cross_cap = true;
				event(_event_warning, "rasterizer:dx9:stencil: section INDEX %d is past the %d-slot seam-stitch array (only %d sections drew) - sparse indexing, not too many sections; its seams will not be bridged",
					section_index, (int32)k_stencil_shadow_max_cross_sections, sections_drawn + 1);
			}
		}
		sections_drawn++;
	}
	return sections_drawn;
}

// Bridge matched seams between this caster's sections (tag debug's shared-edge stitches). The pairing
// reads the sections actually built for this caster and retags matched boundary quads in place.
static void stencil_shadow_stitch_caster_seams(
	datum render_model_index,
	const render_model_definition* render_model,
	const s_stencil_shadow_caster_sections* sections,
	const real_point3d* toward_light_world,
	real32 extrusion_distance,
	real32 shadow_opacity)
{
	s_stencil_shadow_model_cross* cross = stencil_shadow_model_cross_get(
		render_model_index, render_model, sections->shadows, sections->section_of_dense,
		(int32)sections->count);
	if (!cross || cross->quads.empty())
	{
		return;
	}

	static std::vector<uint16> cross_indices;
	bool owner_handled[k_stencil_shadow_max_cross_sections] = {};
	for (uint32 seed = 0; seed < cross->quads.size(); seed++)
	{
		// Quads carry SECTION indices while storage is dense - map before every use.
		const uint8 owner = cross->quads[seed].owner_section;
		const int16 owner_slot = sections->dense_of_section[owner];
		if (owner_slot < 0 || owner_handled[owner_slot] || !sections->shadows[owner_slot])
		{
			continue;
		}
		owner_handled[owner_slot] = true;
		cross_indices.clear();
		for (uint32 entry_index = 0; entry_index < cross->quads.size(); entry_index++)
		{
			const s_stencil_shadow_cross_quad* entry = &cross->quads[entry_index];
			const int16 partner_slot = sections->dense_of_section[entry->partner_section];
			if (entry->owner_section != owner
				|| partner_slot < 0
				|| !sections->shadows[partner_slot])
			{
				continue;
			}
			const uint32* owner_bits = sections->facing[owner_slot];
			const uint32* partner_bits = sections->facing[partner_slot];
			bool left_faces = (owner_bits[entry->owner_triangle >> 5]
				>> (entry->owner_triangle & 31)) & 1;
			bool right_faces = (partner_bits[entry->partner_triangle >> 5]
				>> (entry->partner_triangle & 31)) & 1;
			if (left_faces == right_faces)
			{
				continue;
			}
			uint16 vert_a = entry->vert_a;
			uint16 vert_b = entry->vert_b;
			if (right_faces)
			{
				uint16 swap = vert_a;
				vert_a = vert_b;
				vert_b = swap;
			}
			uint16 sheet[6];
			stencil_shadow_emit_silhouette_sheet(vert_a, vert_b, sheet);
			cross_indices.insert(cross_indices.end(), sheet, sheet + 6);
		}
		if (!cross_indices.empty())
		{
			stencil_shadow_draw_cross_indices(sections->shadows[owner_slot], cross_indices,
				toward_light_world, sections->matrices[owner_slot], extrusion_distance, shadow_opacity);
		}
	}
}

// Facing runs against the light POSITION transformed into section space, where the plane data lives.
// There is no translation or scale division in the direction transform above, which is correct for a
// direction and wrong for a position, so the point path needs this full inverse instead.
static void stencil_shadow_point_to_model_space(
	const real_matrix4x3* m, const real_point3d* world, real_point3d* out_model)
{
	const real32 dx = world->x - m->position.x;
	const real32 dy = world->y - m->position.y;
	const real32 dz = world->z - m->position.z;
	const real32 inverse_scale = (m->scale != 0.f) ? (1.f / m->scale) : 1.f;
	out_model->x = (dx * m->vectors.forward.i + dy * m->vectors.forward.j + dz * m->vectors.forward.k) * inverse_scale;
	out_model->y = (dx * m->vectors.left.i + dy * m->vectors.left.j + dz * m->vectors.left.k) * inverse_scale;
	out_model->z = (dx * m->vectors.up.i + dy * m->vectors.up.j + dz * m->vectors.up.k) * inverse_scale;
}
