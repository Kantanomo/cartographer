#include "stdafx.h"
#include "render_stencil_shadow_dynamic.h"

#include "cache/cache_files.h"
#include "memory/data.h"
#include "objects/objects.h"
#include "objects/object_definition.h"
#include "objects/light_definitions.h"
#include "models/render_model_definitions.h"
#include "geometry/geometry_definitions_new_runtime.h"
#include "rasterizer/dx9/rasterizer_dx9.h"
#include "rasterizer/dx9/rasterizer_dx9_main.h"
#include "rasterizer/dx9/rasterizer_dx9_stencil_shadows.h"
#include "rasterizer/dx9/rasterizer_dx9_stencil_shadow_skinning.h"
#include "rasterizer/dx9/rasterizer_dx9_stencil_shadow_reach.h"
#include "render/render.h"
#include "render/render_cameras.h"
#include "render/render_lod_new.h"
#include "main/interpolator.h"		// it. 617 — the render pose, not the tick pose
#include "tag_files/tag_reference.h"
#include "render/render_stencil_shadow_casters.h"
#include "render/render_stencil_shadow_environment.h"
#include "H2MOD/Modules/h2log/h2log.h"

/* globals */

// The tier is inert unless BOTH are set: `available` is the hard compile-time gate (F5 refuses while
// it is false), `default_on` only decides whether it starts enabled once it is available.
static bool g_stencil_shadow_dynamic_enabled =
	k_stencil_shadow_dynamic_available && k_stencil_shadow_dynamic_default_on;

// Per-map log budgets. LATCHES — they stop firing forever — so under the it. 310 rule they are file
// scope and reset per map. See docs/08-code-map.md, "Ownership rules".
static bool g_dyn_warned_no_light_data = false;
static bool g_dyn_warned_no_camera = false;
static int32 g_dyn_logged_rejects = 0;
static int32 g_dyn_logged_probe = 0;
static uint32 g_dyn_report_tick = 0;


/* the light datum, mirrored from the Vista IDB */

// halo2.exe `light_datum`, 272 bytes. Only the fields this tier reads are named; the rest is
// explicit padding so the layout is checkable rather than asserted. Offsets are the IDB's own —
// the struct is fully declared there, so these are read, not inferred.
//
// NOTE the two position/radius pairs, which are NOT interchangeable:
//   bounding_sphere_center/@+24 + bounding_sphere_radius/@+36 — the CULLING volume
//   position/@+40            + radius/@+52                    — the light ITSELF
// `light_get_bounding_sphere` (halo2.exe 0x54D4E8) returns the first pair; the shadow maths needs
// the second. Confusing them puts the light at the centre of its own bounding volume, which for an
// attached light is not where it is.
struct s_dyn_light_datum
{
	int8 pad_0[2];
	uint16 flags;						// +2   bit 1 (& 2) = produces light (td's render_lights gate)
	int32 definition_index;				// +4
	int32 volume_index;					// +8
	int32 marker_generation;			// +12
	int8 pad_16[8];
	real_point3d bounding_sphere_center;// +24
	real32 bounding_sphere_radius;		// +36
	real_point3d position;				// +40  THE LIGHT
	real32 radius;						// +52  THE LIGHT
	int8 pad_56[16];
	uint16 cluster_reference;			// +72
	int8 pad_74[2];
	int32 owner_object_index;			// +76
	int8 pad_80[4];
	uint16 attachment_marker_index;		// +84
	int8 pad_86[46];
	real_point3d projection_point;		// +132
	real_point3d look_at_point;			// +144
	int8 pad_156[16];
	real_vector3d forward;				// +172
	real_vector3d up;					// +184
	int8 pad_196[4];
	real32 fade;						// +200
	int8 pad_204[4];
	real32 scale;						// +208
	real_rgb_color color_a;				// +212
	real_rgb_color color_b;				// +224
	int8 pad_236[12];
	real32 shadow_fade;					// +248  td's analogue: light_state + 224
	real32 shadow_radius_fade;			// +252
	real32 illumination_fade;			// +256
	real32 specular_fade;				// +260
	real32 inner_fade;					// +264
	int8 pad_268[4];
};
ASSERT_STRUCT_SIZE(s_dyn_light_datum, 272);

static data_array* dynamic_light_data_get(void)
{
	return *Memory::GetAddress<data_array**>(0x4E6660);
}

/* a gathered light */

struct s_dyn_light
{
	datum light_index;
	const s_dyn_light_datum* light;
	const light_definition* definition;
	real_point3d position;
	real32 radius;			// the light's own radius — bounds both the volume and the caster search
	real32 extrusion;
	real32 darkness;
	real32 score;			// screen importance; higher wins a slot
};

/* private code */

// THE ONE NEW PIECE OF MATHS IN THIS TIER.
//
// `stencil_shadow_direction_to_model_space` rotates a DIRECTION: `d_model = R^T * d_world`. A point
// light needs its POSITION, which also needs the translation removed and the scale divided out:
//
//     p_model = R^T * (p_world - T) / scale
//
// Feeding a position through the rotation-only path drops the translation entirely and puts the
// light somewhere else in the model's frame. The facing bits would then be wrong in a way that
// looks like a silhouette-generation bug rather than a transform bug — which is exactly the class of
// error this project has lost the most time to, so it is called out here rather than trusted.
//
// The basis is orthonormal (the inverse binds measured live are scale 1, it. 413), so the transpose
// IS the inverse rotation. `scale` is separate and divided out explicitly; a zero scale would be a
// degenerate matrix and is guarded rather than producing an infinity.
static void stencil_shadow_point_to_model_space(
	const real_matrix4x3* model_matrix,
	const real_point3d* world_point,
	real_point3d* out_model_point)
{
	const real_vector3d* forward = &model_matrix->vectors.forward;
	const real_vector3d* left = &model_matrix->vectors.left;
	const real_vector3d* up = &model_matrix->vectors.up;

	const real32 dx = world_point->x - model_matrix->position.x;
	const real32 dy = world_point->y - model_matrix->position.y;
	const real32 dz = world_point->z - model_matrix->position.z;

	const real32 scale = (model_matrix->scale > 0.0001f) ? model_matrix->scale : 1.f;
	const real32 inv_scale = 1.f / scale;

	out_model_point->x = (forward->i * dx + forward->j * dy + forward->k * dz) * inv_scale;
	out_model_point->y = (left->i * dx + left->j * dy + left->k * dz) * inv_scale;
	out_model_point->z = (up->i * dx + up->j * dy + up->k * dz) * inv_scale;
}

// Project the light's sphere to a screen rect for the scissored apply.
//
// Deliberately CONSERVATIVE: it takes the sphere's extent along the camera's right and up axes at
// the sphere's own depth, which over-estimates for a sphere off the view axis. An over-large scissor
// costs fill rate; an under-large one would clip a real shadow, and a clipped shadow reads as a
// broken one. Returns false when the sphere is behind the camera or the camera is unusable.
static bool stencil_shadow_dynamic_light_scissor(
	const render_camera* camera,
	const real_point3d* centre,
	real32 radius,
	RECT* out_rect)
{
	const real32 vw = (real32)rectangle2d_width(&camera->viewport_bounds);
	const real32 vh = (real32)rectangle2d_height(&camera->viewport_bounds);
	if (vw <= 0.f || vh <= 0.f || camera->vertical_field_of_view <= 0.01f)
	{
		return false;
	}

	const real32 dx = centre->x - camera->point.x;
	const real32 dy = centre->y - camera->point.y;
	const real32 dz = centre->z - camera->point.z;

	const real32 depth = camera->forward.i * dx + camera->forward.j * dy + camera->forward.k * dz;
	if (depth <= radius)
	{
		// the camera is inside or behind the sphere — the light can affect anywhere on screen
		out_rect->left = 0;
		out_rect->top = 0;
		out_rect->right = (LONG)vw;
		out_rect->bottom = (LONG)vh;
		return true;
	}

	// right = cross(forward, up). Halo's convention is forward=+x, LEFT=+y, up=+z, so
	// cross((1,0,0),(0,0,1)) = (0,-1,0) — i.e. +right. Same derivation as the reach encode.
	const real_vector3d right =
	{
		camera->forward.j * camera->up.k - camera->forward.k * camera->up.j,
		camera->forward.k * camera->up.i - camera->forward.i * camera->up.k,
		camera->forward.i * camera->up.j - camera->forward.j * camera->up.i
	};

	const real32 tan_half_v = tanf(camera->vertical_field_of_view * 0.5f);
	const real32 tan_half_h = tan_half_v * (vw / vh);

	const real32 off_x = right.i * dx + right.j * dy + right.k * dz;
	const real32 off_y = camera->up.i * dx + camera->up.j * dy + camera->up.k * dz;

	// normalised device offsets of the centre, and the sphere's angular half-extent
	const real32 ndc_x = off_x / (depth * tan_half_h);
	const real32 ndc_y = off_y / (depth * tan_half_v);
	const real32 ext_x = radius / (depth * tan_half_h);
	const real32 ext_y = radius / (depth * tan_half_v);

	real32 x0 = (ndc_x - ext_x + 1.f) * 0.5f * vw;
	real32 x1 = (ndc_x + ext_x + 1.f) * 0.5f * vw;
	// screen y is inverted relative to the up axis
	real32 y0 = (1.f - (ndc_y + ext_y)) * 0.5f * vh;
	real32 y1 = (1.f - (ndc_y - ext_y)) * 0.5f * vh;

	if (x0 < 0.f) { x0 = 0.f; }
	if (y0 < 0.f) { y0 = 0.f; }
	if (x1 > vw) { x1 = vw; }
	if (y1 > vh) { y1 = vh; }
	if (x1 <= x0 || y1 <= y0)
	{
		return false;		// entirely off screen
	}

	out_rect->left = (LONG)x0;
	out_rect->top = (LONG)y0;
	out_rect->right = (LONG)x1;
	out_rect->bottom = (LONG)y1;
	return true;
}

// td's per-light shadow gate, `render_lights` (td 0x11D560):
//     casts = (definition->flags & 8) == 0 && light_state[+224] > 0
// bit 3 is `_no_shadow_dont_cast_any_stencil_shadows`, which SURVIVES in Vista's tag format — it is
// the tag author's own switch for exactly this feature, so it is honoured rather than reinvented.
static bool stencil_shadow_dynamic_light_qualifies(
	const s_dyn_light_datum* light,
	const light_definition* definition)
{
	if ((light->flags & 2) == 0)
	{
		return false;			// does not produce light — td reads the same bit
	}
	if (!definition)
	{
		return false;
	}
	if (definition->flags.test(_light_definition_no_shadow_dont_cast_any_stencil_shadows))
	{
		return false;
	}
	if (light->shadow_fade <= k_stencil_shadow_dynamic_min_fade)
	{
		return false;			// td: light_state[+224] > 0
	}
	if (light->illumination_fade <= k_stencil_shadow_dynamic_min_fade)
	{
		return false;			// contributes nothing to darken away from
	}
	if (!(light->radius > 0.01f) || light->radius > 10000.f)
	{
		return false;			// degenerate or absurd — cannot bound a volume
	}
	return true;
}

// td's caster predicate, `render_object_should_cast_shadow` (td 0x103740). The parent exclusions are
// the light definition's own: a lamp attached to an object usually should not shadow that object
// with itself.
static bool stencil_shadow_dynamic_object_casts_for_light(
	datum object_index,
	const s_dyn_light* entry,
	datum light_owner,
	datum light_ultimate_parent)
{
	const light_definition* definition = entry->definition;
	if (object_index == light_owner
		&& (definition->flags.test(_light_definition_ignore_parent_object)
			|| definition->flags.test(_light_definition_dont_shadow_parent)))
	{
		return false;
	}
	if (object_index == light_ultimate_parent
		&& definition->flags.test(_light_definition_ignore_all_parents))
	{
		return false;
	}
	return true;
}

/* public code */

bool stencil_shadow_dynamic_enabled(void)
{
	return g_stencil_shadow_dynamic_enabled;
}

void stencil_shadow_dynamic_toggle(void)
{
	// if/else constexpr, not an early return: an early return leaves the rest of the function
	// provably unreachable, which is C4702 and /WX makes that an error. A discarded ELSE branch is
	// not "unreachable code", it is simply not instantiated.
	if constexpr (!k_stencil_shadow_dynamic_available)
	{
		// Say so rather than appearing to toggle nothing — a key that silently does nothing is
		// indistinguishable from a key that is broken.
		LOG_INFO_GAME("stencil dyn: DISABLED at compile time (k_stencil_shadow_dynamic_available = false) — F5 ignored");
	}
	else
	{
		g_stencil_shadow_dynamic_enabled = !g_stencil_shadow_dynamic_enabled;
		LOG_INFO_GAME("stencil dyn: enabled={} (F5) — max_lights={} casters/light={} darkness={:.2f}",
			g_stencil_shadow_dynamic_enabled ? 1 : 0,
			(int32)k_stencil_shadow_dynamic_max_lights,
			(int32)k_stencil_shadow_dynamic_max_casters_per_light,
			k_stencil_shadow_dynamic_darkness);
	}
}

void stencil_shadow_dynamic_reset_diagnostics(void)
{
	g_dyn_warned_no_light_data = false;
	g_dyn_warned_no_camera = false;
	g_dyn_logged_rejects = 0;
	g_dyn_logged_probe = 0;
}

void stencil_shadow_dynamic_volumes_pass(void)
{
	if (!g_stencil_shadow_dynamic_enabled)
	{
		return;
	}
	// Shares the lightmap tier's shaders, index buffer and section cache, all of which are created
	// and owned there. Running without it would draw through uninitialised resources.
	if (!stencil_shadow_active() || !cache_file_is_loaded())
	{
		return;
	}

	IDirect3DDevice9Ex* device = rasterizer_dx9_device_get_interface();
	if (!device)
	{
		return;
	}

	const s_frame* frame = global_window_parameters_get();
	const render_camera* camera = frame ? &frame->camera : NULL;
	if (!camera || camera->z_far <= 1.f)
	{
		if (!g_dyn_warned_no_camera)
		{
			g_dyn_warned_no_camera = true;
			LOG_INFO_GAME("stencil dyn: no usable camera this frame — pass skipped");
		}
		return;
	}

	data_array* lights = dynamic_light_data_get();
	if (!lights || !lights->valid || !lights->data)
	{
		if (!g_dyn_warned_no_light_data)
		{
			g_dyn_warned_no_light_data = true;
			LOG_INFO_GAME("stencil dyn: light_data unavailable (valid={} data={}) — pass skipped",
				lights ? (lights->valid ? 1 : 0) : -1, lights ? (lights->data ? 1 : 0) : -1);
		}
		return;
	}

	/* ---- 1. gather and score the lights ------------------------------------------------- */

	s_dyn_light chosen[k_stencil_shadow_dynamic_max_lights] = {};
	int32 chosen_count = 0;
	int32 lights_seen = 0;
	int32 lights_eligible = 0;

	for (int32 light_index = data_next_index(lights, NONE);
		light_index != NONE;
		light_index = data_next_index(lights, light_index))
	{
		// datum_try_and_get, NOT datum_get: the latter `vassert`s on a zero identifier ("tried to
		// access %s using datum_get() with an absolute index"), and ASSERTS_ENABLED is defined in
		// this build — so a malformed light datum would HALT the game instead of being skipped.
		// This walk visits every light every frame; it must degrade, not assert.
		const s_dyn_light_datum* light =
			(const s_dyn_light_datum*)datum_try_and_get(lights, light_index);
		if (!light)
		{
			continue;
		}
		lights_seen++;

		const light_definition* definition =
			(const light_definition*)tag_get_fast(light->definition_index);
		if (!stencil_shadow_dynamic_light_qualifies(light, definition))
		{
			continue;
		}

		const real32 dx = light->position.x - camera->point.x;
		const real32 dy = light->position.y - camera->point.y;
		const real32 dz = light->position.z - camera->point.z;
		const real32 distance_sq = dx * dx + dy * dy + dz * dz;
		const real32 cull = k_stencil_shadow_dynamic_max_light_distance + light->radius;
		if (distance_sq > cull * cull)
		{
			continue;
		}
		lights_eligible++;

		// Screen importance: a bright, large, near light matters more. Crude on purpose — it only
		// has to ORDER the candidates, and a wrong order costs a less useful shadow, not a wrong one.
		const real32 score = (light->illumination_fade * light->radius)
			/ (distance_sq > 1.f ? distance_sq : 1.f);

		s_dyn_light entry = {};
		entry.light_index = (datum)light_index;
		entry.light = light;
		entry.definition = definition;
		entry.position = light->position;
		entry.radius = light->radius;
		// it. 640: a FLAT extrusion, as td does. Tying it to the light's radius truncated tall
		// casters — see the constant's note.
		entry.extrusion = k_stencil_shadow_dynamic_extrusion;
		entry.darkness = k_stencil_shadow_dynamic_darkness
			* light->shadow_fade * light->illumination_fade;
		entry.score = score;

		// insertion sort into the fixed slot list, best first
		int32 slot = chosen_count;
		if (slot >= k_stencil_shadow_dynamic_max_lights)
		{
			if (score <= chosen[k_stencil_shadow_dynamic_max_lights - 1].score)
			{
				continue;
			}
			slot = k_stencil_shadow_dynamic_max_lights - 1;
		}
		else
		{
			chosen_count++;
		}
		while (slot > 0 && chosen[slot - 1].score < score)
		{
			chosen[slot] = chosen[slot - 1];
			slot--;
		}
		chosen[slot] = entry;
	}

	/* ---- 2. one pass per light ----------------------------------------------------------- */

	int32 total_casters = 0;
	int32 total_sections = 0;
	int32 total_clusters = 0;
	real32 last_extrusion = 0.f;
	real32 last_darkness = 0.f;

	// it. 641 — PER LIGHT, as td does. The combined buffer was measured wrong.
	//
	// The first version merged every chosen light into ONE stencil and applied one uniform darken,
	// because the apply had to follow render_lights_new while the volumes had to precede it. The
	// user's run showed what that costs: with two lights on opposite sides of a caster, BOTH
	// shadows render at full strength — one on the floor, one on the ceiling — because the buffer
	// cannot tell which light marked a pixel, so neither can be weighted by the light that made it.
	//
	// That is also why it could never be fixed by attenuating the apply: a combined buffer has
	// already thrown away the attribution the attenuation would need.
	//
	// So the apply moves back beside the volumes and everything is per light again — td's own
	// structure (rasterizer_stencilshadow_shadows_begin clears to 128 at every layer-13 open). The
	// cost is stated at the apply below.

	// it. 636 — DO NOT INHERIT THE LIGHTMAP TIER'S REACH STATE.
	//
	// `stencil_shadow_section_draw` consults `stencil_shadow_reach_is_active()`, and that flag is set
	// PER CASTER by the lightmap tier's encode. When this pass runs it still holds whatever the last
	// lightmap caster left — so with F6 on reach mode, every dynamic volume would be clipped against
	// a bound built for a DIFFERENT caster and a DIFFERENT (directional) light, discarding fragments
	// essentially at random.
	//
	// This tier does not use reach clip at all: its extrusion is already bounded by the light's own
	// radius, which is the point-light equivalent of what reach clip does for the sun. Turn it off
	// explicitly rather than relying on it happening to be off.
	stencil_shadow_reach_deactivate();

	for (int32 chosen_index = 0; chosen_index < chosen_count; chosen_index++)
	{
		const s_dyn_light* entry = &chosen[chosen_index];

		RECT scissor = {};
		if (!stencil_shadow_dynamic_light_scissor(camera, &entry->position, entry->radius, &scissor))
		{
			continue;		// off screen — nothing this light shadows can be seen
		}

		const datum light_owner = (datum)entry->light->owner_object_index;
		const datum light_ultimate_parent = (light_owner != NONE)
			? (datum)object_get_ultimate_parent(light_owner) : (datum)NONE;

		// This light owns the stencil for its own pass — td re-clears at every layer-13 open
		// (rasterizer_stencilshadow_shadows_begin, td 0x21D820).
		device->Clear(0, NULL, D3DCLEAR_STENCIL, 0, 1.f, 128);

		int32 casters_this_light = 0;
		int32 sections_this_light = 0;

		// ENVIRONMENT VOLUMES FIRST, then the models — td's own order, read off the layer dispatch
		// (render_layer_draw, td 0x10DBB0):
		//
		//     case 13: rasterizer_draw_environment_shadows(); rendered_models_draw(scene_type); break;
		//
		// Both halves count into the stencil this light just cleared, and one apply below darkens
		// whatever either of them marked. The order itself does not affect the counting — stencil
		// increments commute — but matching it costs nothing and keeps the comparison against the
		// reference honest.
		// NOTE it does NOT get entry->extrusion. That is the model tier's flat 1024, which the first
		// run showed is 170x too long for a cluster (r=6.00 vs extrude=1024.0) — the tier derives its
		// own from the radius. See k_stencil_shadow_environment_extrusion_scale.
		const int32 clusters_this_light = stencil_shadow_environment_draw_for_light(
			&entry->position, entry->radius);
		sections_this_light += clusters_this_light;
		total_clusters += clusters_this_light;

		// `if constexpr`, not a constant in the while condition: a constant there is C4127 and /WX
		// makes that an error (it. 621 hit exactly this). A discarded branch is not dead code.
		if constexpr (k_stencil_shadow_dynamic_draw_model_casters)
		{
		// td's layer-13 caster rule is _object_mask_all — object_get_and_verify_type(object,
		// 0xFFFFFFFF) in render_object_should_cast_shadow. No type restriction at all.
		c_object_iterator<object_datum> object_iterator;
		object_iterator.begin((e_object_type)_object_mask_all, 0);
		while (object_iterator.next()
			&& casters_this_light < k_stencil_shadow_dynamic_max_casters_per_light)
		{
			const datum object_index = object_iterator.get_index();
			const object_datum* object = object_iterator.get_datum();
			if (!object)
			{
				continue;
			}

			// our existing gates, same as the lightmap tier
			if (object->object.flags.test(_object_hidden_bit)
				|| object->object.flags.test(_object_shadowless_bit))
			{
				continue;
			}
			if (!stencil_shadow_dynamic_object_casts_for_light(
				object_index, entry, light_owner, light_ultimate_parent))
			{
				continue;
			}

			// sphere-vs-sphere: does the light reach this object at all?
			const real32 ox = object->object.center.x - entry->position.x;
			const real32 oy = object->object.center.y - entry->position.y;
			const real32 oz = object->object.center.z - entry->position.z;
			const real32 reach = entry->radius + object->object.radius;
			if ((ox * ox + oy * oy + oz * oz) > reach * reach)
			{
				continue;
			}

			// object definition -> hlmt -> render model (first tag_reference in hlmt), the same
			// two-hop the lightmap tier does.
			const _object_definition* object_definition =
				(const _object_definition*)tag_get_fast(object->definition_index);
			if (!object_definition || object_definition->model.index == NONE)
			{
				continue;
			}
			const tag_reference* hlmt_render_model_reference =
				(const tag_reference*)tag_get_fast(object_definition->model.index);
			if (!hlmt_render_model_reference || hlmt_render_model_reference->index == NONE)
			{
				continue;
			}
			const datum render_model_index = hlmt_render_model_reference->index;
			const render_model_definition* render_model =
				(const render_model_definition*)tag_get_fast(render_model_index);
			if (!render_model)
			{
				continue;
			}
			if (!stencil_shadow_model_is_manifold(render_model, object_index))
			{
				continue;
			}

			// SIMPLIFIED SECTION WALK, and deliberately not the hook's.
			//
			// The lightmap tier resolves region -> permutation -> lod_sections[lod] inline inside
			// stencil_shadow_render_layer_hook, with an LOD fallback and its own telemetry. Sharing
			// that would mean extracting it from a hook that currently WORKS and has not been
			// re-verified since the it. 623-632 refactor. Duplicating ~40 lines that touch nothing
			// shipped is the cheaper risk while this tier is unproven.
			//
			// Unify the two once the caster loop is extracted (td-refactor-remaining.md, the one
			// deferred item). Until then the difference is deliberate: this walk has NO LOD
			// fallback — if the object has no cached LOD, it casts no dynamic shadow this frame
			// rather than casting from a mesh the engine may not be drawing (it. 538).
			int32 region_count = 0;
			int8* region_permutation_indices = NULL;
			object_get_region_information(object_index, &region_count,
				&region_permutation_indices, NULL, NULL);
			const int8 object_lod = render_object_cache_get_level_of_detail(object_index);
			if (object_lod < 0 || object_lod >= 6)
			{
				continue;
			}

			bool drew_any = false;
			for (int32 region_index = 0; region_index < render_model->regions.count; region_index++)
			{
				const render_model_region* region = render_model->regions[region_index];
				if (region->permutations.count <= 0)
				{
					continue;
				}
				int32 permutation_index = (region_permutation_indices && region_index < region_count)
					? region_permutation_indices[region_index] : 0;
				if (permutation_index < 0)
				{
					permutation_index = 0;
				}
				else if (permutation_index > region->permutations.count - 1)
				{
					permutation_index = region->permutations.count - 1;
				}
				const render_model_permutation* permutation = region->permutations[permutation_index];
				const int16 section_index = (&permutation->l1_section_index)[object_lod];
				if (section_index == NONE || section_index < 0
					|| section_index >= render_model->sections.count)
				{
					continue;
				}
				const render_model_section* section = render_model->sections[section_index];
				if (section->section_info.total_vertex_count == 0
					|| section->section_info.total_triangle_count == 0)
				{
					continue;
				}

				s_stencil_shadow_section* shadow =
					stencil_shadow_section_get(render_model_index, section_index);
				if (!shadow || !shadow->valid)
				{
					continue;
				}

				// Pose exactly as the lightmap tier does — same accessors, same order, INCLUDING
				// the it. 617 interpolation adoption. Without that the volume is built from the raw
				// TICK pose while the model renders interpolated, and volumes LEAD moving objects.
				int16 section_node = 0;
				if (section->rigid_node != NONE)
				{
					section_node = section->rigid_node;
				}
				const real_matrix4x3* model_matrix =
					object_get_node_matrix(object_index, section_node);
				if (!model_matrix && !shadow->articulated)
				{
					continue;
				}
				real_matrix4x3 interpolated_static;
				if (model_matrix
					&& halo_interpolator_interpolate_object_node_matrix(
						object_index, section_node, &interpolated_static))
				{
					model_matrix = &interpolated_static;
				}
				// it. 660 — the GPU-skinned palette, same contract as the hook's articulated branch:
				// when the skinned pair serves, the animate skips the dynamic-VB refresh, so a
				// palette failure must SKIP the section rather than fall back to a stale buffer.
				static real_vector4d dyn_palette[201];
				int32 dyn_palette_count = 0;
				if (shadow->articulated)
				{
					// CPU-skinned to WORLD; draws with identity node constants, so the world light
					// IS the model light for these sections.
					if (!stencil_shadow_section_animate(shadow, object_index, render_model,
						region_index))
					{
						continue;
					}
					model_matrix = NULL;

					if (shadow->skinned_vb && stencil_shadow_skinned_ready())
					{
						dyn_palette_count = stencil_shadow_pool_build_palette(
							object_index, render_model, region_index, shadow, dyn_palette);
						if (dyn_palette_count == 0)
						{
							continue;	// dynamic VB was not refreshed — see the note above
						}
					}
				}

				// The light in the space the facing test wants. Articulated sections are already in
				// world space, so the world light IS the model light for them.
				real_point3d light_model = entry->position;
				if (model_matrix)
				{
					stencil_shadow_point_to_model_space(model_matrix, &entry->position, &light_model);
				}

				if (g_dyn_logged_probe < 4)
				{
					g_dyn_logged_probe++;
					LOG_INFO_GAME("stencil dyn probe: light world=({:.2f},{:.2f},{:.2f}) model=({:.2f},{:.2f},{:.2f}) obj=({:.2f},{:.2f},{:.2f}) r={:.2f} articulated={} — model-space light should sit near the ORIGIN when the light is close to the object; a large value means the point transform is wrong",
						entry->position.x, entry->position.y, entry->position.z,
						light_model.x, light_model.y, light_model.z,
						object->object.center.x, object->object.center.y, object->object.center.z,
						object->object.radius, shadow->articulated ? 1 : 0);
				}

				static uint32 facing_bitvector[k_stencil_shadow_facing_bitvector_words];
				stencil_shadow_build_facing_bitvector(shadow, &light_model, true, facing_bitvector);

				// POINT light: c254.w = 1, and the shader computes normalize(world_pos*1 - light),
				// i.e. vertex -> light. This is the path that has always been plumbed and never run.
				stencil_shadow_section_draw(shadow, facing_bitvector, &entry->position, true,
					model_matrix, entry->extrusion, 1.f,
					k_stencil_shadow_self_shadow_bias, dyn_palette, dyn_palette_count);
				sections_this_light++;
				drew_any = true;
			}

			if (drew_any)
			{
				casters_this_light++;
			}
		}
		}	// if constexpr (k_stencil_shadow_dynamic_draw_model_casters)

		if (sections_this_light > 0)
		{
			// THIS light's apply, bounded to THIS light's screen extent and weighted by THIS
			// light's own fade terms. That per-light attribution is the whole reason the combined
			// buffer had to go.
			//
			// The cost of applying here rather than after render_lights_new: a light whose
			// contribution is added by that later pass gets partially re-lit over its own shadow.
			// That is a smaller and more local error than darkening every light's shadow by the
			// strongest light's darkness, which is what the combined version did — and unlike that
			// one, it degrades gracefully rather than putting a full-strength shadow on the ceiling.
			stencil_shadow_apply_and_clear(entry->darkness, &scissor);
			last_extrusion = entry->extrusion;
			last_darkness = entry->darkness;
		}

		total_casters += casters_this_light;
		total_sections += sections_this_light;
	}

	/* ---- 3. telemetry -------------------------------------------------------------------- */

	if ((++g_dyn_report_tick % 600) == 0 || (total_sections > 0 && g_dyn_logged_rejects < 8))
	{
		if (total_sections > 0)
		{
			g_dyn_logged_rejects++;
		}
		// `sections` INCLUDES the clusters — it is the apply's gate, so it has to. `clusters` is
		// broken out beside it because the two have completely different costs and completely
		// different failure modes, and one number covering both would hide either.
		LOG_INFO_GAME("stencil dyn: lights_seen={} eligible={} drawn={} casters={} sections={} (clusters={}) extrude={:.2f} darkness={:.3f}",
			lights_seen, lights_eligible, chosen_count, total_casters, total_sections,
			total_clusters, last_extrusion, last_darkness);
	}
}

// it. 641: `stencil_shadow_dynamic_apply` is now a NO-OP kept only so both render.cpp call sites
// stay valid. The apply happens per light inside the volumes pass, beside the counts it consumes —
// see the note there for why the combined late apply had to go.
void stencil_shadow_dynamic_apply(void)
{
}
