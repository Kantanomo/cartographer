#include "stdafx.h"
#include "render_lights.h"

#include "cache/cache_files.h"
#include "cutscene/cinematics.h"
#include "math/real_math.h"
#include "memory/data.h"
#include "objects/light_definitions.h"
#include "objects/objects.h"
#include "rasterizer/dx9/rasterizer_dx9_main.h"						// device (the per-light wvp re-upload)
#include "rasterizer/dx9/rasterizer_dx9_stencil_shadow_debug_view.h"	// F7 draw mode (stage 1's red view)
#include "rasterizer/dx9/rasterizer_dx9_stencil_shadow_tunables.h"	// LOG_STENCIL
#include "rasterizer/dx9/rasterizer_dx9_stencil_shadows.h"			// the per-object volume entry
#include "networking/network_event.h"
#include "render/render_layers.h"
#include "render/render_stencil_shadow_environment.h"				// the light-touches-cluster test
#include "structures/structure_bsp_definitions.h"					// cluster bounds

// ============================ dynamic-light tier scaffolding ============================
// Written from the retail loop; every layout claim below cites the address it came from.

/* structures */

// File-local, mirroring the retail loop's own locals.

// A visible light entry (20 bytes - the retail store pattern: flag bytes +0..+3,
// object_index +4, shadow_object_index +8, transparency +12 (= illumination_fade copy),
// projected_radius +16; stride from `add edi, 14h`).
struct s_visible_light
{
	int8 first_person;				// +0: render_object_is_first_person(ultimate parent)
	int8 first_person_only;			// +1: def flags bit 5 (0x20)
	int8 is_weapon;					// +2: owner header type byte == 2
	int8 framerate_killer;			// +3: def flags bit 20 (0x100000)
	datum object_index;				// +4: the light datum handle
	datum shadow_object_index;		// +8: object_get_ultimate_parent(owner)
	real32 transparency;			// +12: illumination_fade copy - the 0.07 loop gate reads this
	real32 projected_radius;		// +16
};
ASSERT_STRUCT_SIZE(s_visible_light, 20);

// The per-volume visibility-frustum descriptor - OPAQUE by design: our code only
// stack-allocates frustums[6] and passes pointers into INVOKE'd children. Size 444 = the retail
// frame's [ebp-0xA68] span / 6.
struct s_light_visibility_frustum
{
	int8 opaque[444];
};
ASSERT_STRUCT_SIZE(s_light_visibility_frustum, 444);

// the engine's visibility collection - MINIMAL surface

// Only what stage 0 touches: the object-list pointer array at +0x0C (the add path derefs
// collection+0x14 = m_lists[2]) and the one list method the walk below calls. Extend on need.
class c_visibility_object_list
{
public:
	// Layout from the add_object store bytes (halo2.exe 0x501B2F): four PARALLEL
	// arrays indexed by m_count. The +0x08 int16 slot is PACKED - a cluster index in the low
	// bits with the 0x1000/0x2000 shadow flag bits on top (0x2000 = casts shadow, docs/14 section 5;
	// shadow_add_object_to_visibility writes 0x3000/0x1000 through this same slot).
	int32 m_capacity;			// +0x00 (append refuses at m_capacity - 1)
	uint16 m_count;				// +0x04
	int16 pad_6;
	int16* m_cluster_flags;		// +0x08 (stored *2)
	datum* m_objects;			// +0x0C (stored *4)
	int32* m_field_10;			// +0x10 (stored *4; semantics not yet identified)
	int16* m_field_14;			// +0x14 (stored *2)

	// halo2.exe 0x501B2F (__thiscall): appends one entry (object index + the per-entry flags word).
	// Signature from the call-site bytes: push -1, push 0, push flags, push index, ecx=list.
	bool add_object(datum object_index, int16 flags, int32 a4, int16 a5);
};
static_assert(offsetof(c_visibility_object_list, m_cluster_flags) == 0x08, "c_visibility_object_list.m_cluster_flags");
static_assert(offsetof(c_visibility_object_list, m_objects) == 0x0C, "c_visibility_object_list.m_objects");

class c_visibility_collection
{
public:
	int8 pad_0[0x0C];
	c_visibility_object_list* m_lists[4];	// +0x0C.
											// Casters for stage 1: m_lists[2] entries with the
											// 0x2000 casts flag (docs/14 section 5).
};
static_assert(offsetof(c_visibility_collection, m_lists) == 0x0C, "c_visibility_collection.m_lists");

/* constants */

enum { k_render_section_cache_stride = 1032 };

// BISECT SWITCH: false = the retail INVOKE (pure engine), true = the native loop. It exists because
// a native loop here once crashed where the INVOKE did not, through the transparent-plane
// render-queue callback into rasterizer_setup_light_projection and the constants keystone - with
// render_lights_new in neither faulting frame. The cause was the reimplemented STACK FRAME:
// rasterizer_setup_light_projection retains the raw shadow_geometry POINTER, copying only one word
// by value, and the transparent queue re-enters it after the loop's frame is dead, where retail
// survives on its own deterministic dead-stack residue. The static storage below fixes it; the
// switch stays for future bisects.
static const bool k_stage0_native_loop = true;

/* globals */

// the dynamic-light stencil shadow tier (docs/14) - gates and stage state

// Mutable globals rather than consts: a const in a runtime gate is C4127, which /WX promotes to
// an error, and a variable can be flipped from a debugger mid-session.
static bool g_dyn_tier_available = true;	// master gate
static int32 g_dyn_tier_stage = 3;			// rollout stage (docs/14 section 12)
static bool g_dyn_tier_enabled = true;		// F5: false = the tier stands down, retail-exact
static int32 g_dyn_tier_max_lights = 3;		// per-frame cap, matching the engine's own
											// top-3-per-window selection - a sanity bound, not a
											// selector

// the cluster/environment tier (docs/15)
static bool g_env_tier_enabled = true;		// the tier master, toggled by F4
static int32 g_env_tier_stage = 1;			// docs/15 section 8: 1 = one cluster's volumes, blue view
static bool g_env_reach_cull_disable = false;	// the reach-cull A/B lever: true = facing reach 0
											// (no cull), so the map-spanning volume returns

// FORCE every processed light to tier-shadow, overriding the tag-side
// qualification (the no-shadow definition flag, the fade gate) - a testing lever so a map's
// lights need no individual tag edits. The engine's 0.07 transparency gate, the per-frame cap
// and the radius sanity still apply (force is not a bypass of sanity). Shift+F5.
static bool g_dyn_tier_force_all_lights = false;

// The per-frame tier-light counter: every shadow-flagged light in the engine's selected order gets
// tier-handled up to g_dyn_tier_max_lights. render_lights_new resets it at entry each frame.
static int32 g_dyn_tier_lights_taken = 0;

// One-shot latch for the gate line. FILE SCOPE and reset per map for the reason every other module's
// latches are: as a function-local static it caps per PROCESS, so only the first map of a session
// would ever be described.
static bool g_dyn_tier_gates_logged = false;

// c0-c3 captured from the engine mirror at render_lights_new entry (docs/14 section 3 MUST: c0-c3 go
// stale per light inside the loop, and the mirror is authoritative and stall-free).
// The volume pass re-uploads these MIRROR+SET before drawing.
static real_vector4d g_dyn_captured_wvp[4];
static bool g_dyn_wvp_captured = false;

/* prototypes */

// Module offsets = IDB VA - 0x400000.

static data_array* light_data_get(void);

static bool* g_render_shadows_enabled_get(void);

static uint8* g_render_light_object_markers_suppressed_get(void);

static int32* g_render_light_default_cluster_get(void);

static int32* g_render_section_deferred_slot_get(void);

static int8* g_render_section_cache_get(void);

static uint32* g_render_shadow_flags_get(void);

static uint8* g_render_shadow_flags_active_get(void);

static int32* g_render_cinematic_object_shadow_count_get(void);

static real_rgb_color* g_render_shadow_color_get(void);

static real32* g_render_shadow_range_get(void);

static real32* g_vs_constants_mirror_get(void);

static real32* g_light_staged_bounds_get(void);

static void compute_perpendicular_vector(const real_vector3d* direction, real_vector3d* io_vector);

static void shadow_add_object_to_visibility(bool shadow_flag, datum object_index, c_visibility_collection* visibility);

static datum object_get_ultimate_parent_safe(datum object_index);

static void __cdecl create_visible_render_primitives(int32 a1);

static bool render_primitive_list_empty(e_render_layer layer);

static void __cdecl rasterizer_setup_light_projection(int32 light_index, const real32* light_descriptor,
	int32 light_definition_ptr, int16* shadow_geometry, real32 depth_bias, char shadow_flag, real32 intensity);

static void __cdecl rasterizer_build_light_depth_projection_constants(int32 light_index,
	const real32* camera_frame, const real32* light_geometry, const real32* shadow_geometry);

static char __cdecl draw_specific_render_layer(int32 a1, e_render_layer layer);

static void __cdecl render_cinematic_object_shadows(void);

static void __cdecl rasterizer_dx9_disable_scissor_restore_target(void);

static int32 __cdecl shadow_build_light_list(s_visible_light* lights);

static bool __cdecl light_build_render_geometry(uint16 light_index, s_light_render_geometry* out_geometry,
	bool check_game_engine, bool* out_valid);

static string_id __cdecl object_get_attachment_marker_name(datum object_index, int16 marker_index);

static void __cdecl sub_628B7A(datum object_index, string_id marker_name, int32* out_position,
	int32* out_forward, int32* out_up);

static void __cdecl light_build_visibility_projections(real32* projection_point,
	s_light_render_geometry* geometry, s_light_visibility_frustum* out_frustums, int16* out_count);

static c_visibility_collection* __cdecl rasterizer_build_visibility(int32 a1, int32 flags,
	s_light_visibility_frustum* frustums, int16 frustum_count, real_point3d* bounds_center,
	real32 bounds_radius, void* marker_env, int32 a8, real32 a9, int32 cluster);

static void __cdecl apply_light_shadow_flags(c_visibility_collection* visibility, uint16 light_index,
	uint16 definition_index, datum ultimate_parent_1, datum ultimate_parent, bool projection_is_omni,
	int32* shadow_flag);

static void __cdecl render_build_visible_objects(int32 a1);

static void __cdecl process_visibility_marked_sections(void);

static void* __cdecl visibility_collection_primary_get(void);

static uint8* g_rasterizer_disable_stencil_get(void);

static bool dyn_tier_scissor_rect_from_staged_bounds(RECT* out_rect);

static int32 render_light_shadow_pass(int32 light_index, const real32* light_descriptor, datum definition_index,
	datum ultimate_parent_1, datum ultimate_parent, s_light_render_geometry* geometry, int32 visibility,
	bool shadow_flag, real32 intensity);

/* public code */

bool c_visibility_object_list::add_object(datum object_index, int16 flags, int32 a4, int16 a5)
{
	typedef bool(__thiscall* add_object_t)(c_visibility_object_list*, datum, int16, int32, int16);
	return Memory::GetAddress<add_object_t>(0x101B2F)(this, object_index, flags, a4, a5);
}

void render_lights_reset_diagnostics(void)
{
	g_dyn_tier_gates_logged = false;
}

void render_lights_env_tier_toggle(void)
{
	g_env_tier_enabled = !g_env_tier_enabled;
#ifdef LOG_STENCIL
	event(_event_status, "rasterizer:dx9:stencil:environment: tier %s (F4)", g_env_tier_enabled ? "ENABLED" : "DISABLED");
#endif
}

void render_lights_env_reach_cull_toggle(void)
{
	g_env_reach_cull_disable = !g_env_reach_cull_disable;
#ifdef LOG_STENCIL
	event(_event_status, "rasterizer:dx9:stencil:environment: reach cull %s (Shift+F4) - expect %s", g_env_reach_cull_disable ? "OFF" : "ON",
		g_env_reach_cull_disable ? "a map-spanning volume" : "the volume hugging the light radius");
#endif
}

void render_lights_dyn_tier_toggle(void)
{
	g_dyn_tier_enabled = !g_dyn_tier_enabled;
#ifdef LOG_STENCIL
	event(_event_status, "rasterizer:dx9:stencil:dynamic: tier %s (F5)", g_dyn_tier_enabled ? "ENABLED" : "DISABLED -> retail");
#endif
}

void render_lights_dyn_tier_force_all_toggle(void)
{
	g_dyn_tier_force_all_lights = !g_dyn_tier_force_all_lights;
#ifdef LOG_STENCIL
	event(_event_status, "rasterizer:dx9:stencil:dynamic: force-all-lights %s (Shift+F5)", g_dyn_tier_force_all_lights ? "ON" : "OFF");
#endif
}

void __cdecl render_lights(void)
{
	INVOKE(0x14CB17, 0x0, render_lights);
	return;
}

void __cdecl render_light_clear_data(void)
{
	INVOKE(0x1A0F39, 0x0, render_light_clear_data);
	return;
}

void __cdecl render_cinematic_lightmap_shadows(int32 effect_flag)
{
	INVOKE(0x193F9D, 0x0, render_cinematic_lightmap_shadows, effect_flag);
	return;
}

// The per-frame dynamic-shadow driver, a native reimplementation of retail render_dynamic_shadows
// (halo2.exe 0x593072). Gathers the selected shadow lights, and per light resolves the attachment, builds the
// render geometry, builds the per-light visibility, and runs the shadow-buffer plus
// light-accumulation pass.
//
// THREE REQUIRED DIVERGENCES from retail (docs/14 section 3), each fixing a proven race:
//   1. the geometry build result is CHECKED, where retail ignores it - a dying light is skipped and
//      counted rather than feeding garbage into the constants keystone;
//   2. the parent walks are guarded (object_get_ultimate_parent_safe and the biped walk above),
//      where retail's read freed slots blind;
//   3. datum_try_and_get on FULL handles, since datum_get vasserts on absolute indices.
void __cdecl render_lights_new(void)
{
	if (!k_stage0_native_loop)
	{
		INVOKE(0x193072, 0x0, render_lights_new);
		return;
	}

	g_dyn_tier_lights_taken = 0;		// the per-frame tier-light counter

	// Capture c0-c3 from the engine mirror at loop entry (docs/14 section 3): the mirror @0xE3C7B0
	// is the authoritative full c0-c255 file. The volume pass re-uploads these mirror+set before each
	// draw, because the loop's own children leave c0-c3 stale.
	{
		const real32* mirror = g_vs_constants_mirror_get();
		if (mirror)
		{
			memcpy(g_dyn_captured_wvp, mirror, sizeof(g_dyn_captured_wvp));
			g_dyn_wvp_captured = true;
		}
		else
		{
			g_dyn_wvp_captured = false;
		}
	}

	// STATIC storage, deliberately NOT stack locals (a divergence with evidence):
	// the 2026-08-20 live catch crashed in the engine's lit-TRANSPARENT path (queue callback ->
	// setup -> the constants keystone) dereferencing per-light data AFTER the loop's frame died;
	// the transparent queue captures a 36-byte context by value at submit time (halo2.exe 0x678530)
	// and retail survives its own dangling reads only on deterministic dead-stack residue. The
	// bisect (INVOKE = no crash, native = crash, zero guard counters) pinned the perturbation to
	// the reimplemented FRAME itself. Statics make any late read find the last light's VALID
	// data instead of frame garbage - deterministic where retail is lucky. Single-threaded use
	// (render_lights_new runs once per window on the render thread).
	static s_visible_light lights[128];						// the retail 2560B gather buffer
	static s_light_render_geometry geometry;
	static s_light_visibility_frustum frustums[6];

	int32 drawn_count = 0;									// parity counters
	int32 shadowed_count = 0;
	static int32 geoskip_count = 0;							// divergence 1: dying-light geometry skips
	static int32 stale_owner_count = 0;						// divergence 2: owners dead by render time

	const int32 light_count = shadow_build_light_list(lights);
	for (int32 i = 0; i < light_count; ++i)
	{
		const s_visible_light* entry = &lights[i];
		const datum light_index = entry->object_index;
		if (entry->transparency <= 0.07f)					// the retail faint-light gate
			continue;

		// Divergence 3: full-handle validated resolve - the handle comes from the engine's own
		// list, so a mismatch means the light died since the gather; skip, never assert.
		light_datum* light = (light_datum*)datum_try_and_get(light_data_get(), light_index);
		if (!light)
			continue;
		const datum definition_index = light->definition_index;
		const light_definition* definition = (const light_definition*)tag_get_fast(definition_index);
		const uint32 light_flags = definition->flags.get_unsafe();

		datum ultimate_parent_1 = NONE;
		datum ultimate_parent = NONE;
		bool render_geometry_valid = false;

		// Marker-attached lights (e.g. muzzle flashes, weapon lights) follow their marker.
		if (light->attachment_marker_index != 0xFFFF)
		{
			ultimate_parent_1 = light->owner_object_index;
			ultimate_parent = object_get_ultimate_parent_safe(ultimate_parent_1);	// divergence 2

			if (ultimate_parent == NONE)
			{
				// The owner died between gather and render (the ~1-frame muzzle-flash class).
				// Treat the light as unattached this frame: no marker refresh (it would walk the
				// same dead handle inside engine code), no parent exclusions, no biped add.
				ultimate_parent_1 = NONE;
				++stale_owner_count;
			}
			else if (definition->flags.test(_light_definition_only_render_in_first_person)
				&& !definition->flags.test(_light_definition_first_person_from_camera))
			{
				const string_id marker_name = object_get_attachment_marker_name(ultimate_parent_1, (int16)light->attachment_marker_index);
				sub_628B7A(ultimate_parent_1, marker_name,
					reinterpret_cast<int32*>(&light->projection_point),
					reinterpret_cast<int32*>(&light->forward),
					reinterpret_cast<int32*>(&light->up));

				// Endpoint = marker position + marker forward, then rebuild the up basis.
				light->look_at_point.x = light->projection_point.x + light->forward.i;
				light->look_at_point.y = light->projection_point.y + light->forward.j;
				light->look_at_point.z = light->projection_point.z + light->forward.k;
				compute_perpendicular_vector(&light->forward, &light->up);
			}
		}

		// Divergence 1: retail ignores this result and later divides through whatever the stack
		// held. A failed build means a dying light, so skip it.
		const bool geometry_built = light_build_render_geometry((uint16)light_index, &geometry, true, &render_geometry_valid);
		if (!geometry_built || !render_geometry_valid)
		{
			++geoskip_count;
			continue;
		}

		// The deferred render-section cache slot (258-dword entries). ORDER IS LOAD-BEARING: retail
		// bytes 0x5931BB-0x5931CC write cache[1032 * OLD slot] THEN store slot+1, so the light lands
		// at the CURRENT slot and the entries occupy 0..N-1. The decompile reads as
		// increment-then-index, and transcribing it that way leaves slot 0 stale:
		// first_person_weapon_render_midpass (halo2.exe 0x5A13A9) iterates 0..N-1, raw-resolves the stale
		// slot-0 index and publishes uninitialized geometry through setup. Increment-first also lets
		// slot 8, out of range, be written at 9 or more lights.
		int32* deferred_slot = g_render_section_deferred_slot_get();
		if ((uint32)*deferred_slot <= 7)
		{
			*reinterpret_cast<int32*>(g_render_section_cache_get() + k_render_section_cache_stride * (*deferred_slot)) = light_index;
			++(*deferred_slot);
		}

		void* marker_env = (*g_render_light_object_markers_suppressed_get() == 0) ? visibility_collection_primary_get() : NULL;

		// The shadow qualification: not flagged no-shadow, shadows enabled, positive shadow fade,
		// non-omni projection. Read narrow, as retail does.
		const int32 shadow_flag = (!definition->flags.test(_light_definition_no_shadow_dont_cast_any_stencil_shadows)
			&& *g_render_shadows_enabled_get()
			&& light->shadow_fade > 0.0f && (uint16)geometry.projection_type) ? 1 : 0;

		int32 cluster = (int16)light->cluster_reference;
		if (cluster == -1 && definition->flags.test(_light_definition_only_render_in_first_person)
			&& definition->flags.test(_light_definition_first_person_from_camera))
			cluster = *g_render_light_default_cluster_get();

		// def flag bit 22 (_only_on_parent_bipeds) extracted into the 0x400 visibility-build flag,
		// verbatim from retail's (flags >> 12) & 0x400.
		const int32 visibility_flag = (light_flags >> 12) & 0x400;

		int16 frustum_count = 0;
		light_build_visibility_projections(reinterpret_cast<real32*>(&light->projection_point), &geometry, frustums, &frustum_count);
		c_visibility_collection* visibility = rasterizer_build_visibility(1, visibility_flag, frustums, frustum_count,
			&light->bounding_sphere_center, light->bounding_sphere_radius, marker_env, 0, 0.0f, cluster);

		int32 shadow_flag_box = shadow_flag;				// real int32, low byte live
		apply_light_shadow_flags(visibility, (uint16)light_index, (uint16)definition_index,
			ultimate_parent_1, ultimate_parent, (uint16)geometry.projection_type == 0, &shadow_flag_box);

		if (definition->flags.test(_light_definition_only_on_parent_bipeds))
			shadow_add_object_to_visibility(shadow_flag_box != 0, ultimate_parent_1, visibility);

		render_build_visible_objects(1);
		process_visibility_marked_sections();

		++drawn_count;
		if (shadow_flag_box != 0)
			++shadowed_count;

		render_light_shadow_pass(light_index, reinterpret_cast<const real32*>(&light->projection_point), definition_index,
			ultimate_parent_1, ultimate_parent, &geometry, reinterpret_cast<int32>(visibility), shadow_flag_box != 0, entry->transparency);
	}

#ifdef LOG_STENCIL
	// The parity line (docs/14 section 11): with the native
	// loop live and UNMODIFIED, these counts must track the engine's behaviour - a silent zero
	// on a lit map, or geoskip/stale never ticking under weapon fire, are the named outcomes
	// this line exists to expose. Logged on change + a slow heartbeat.
	{
		static int32 last_gathered = -1;
		static int32 last_drawn = -1;
		static int32 last_shadowed = -1;
		static int32 last_geoskip = 0;
		static int32 last_stale = 0;
		static uint32 heartbeat = 0;
		const bool changed = light_count != last_gathered || drawn_count != last_drawn
			|| shadowed_count != last_shadowed || geoskip_count != last_geoskip
			|| stale_owner_count != last_stale;
		if (changed || (++heartbeat % 600) == 0)
		{
			event(_event_verbose, "rasterizer:dx9:stencil:dynamic: stage0 lights=%d drawn=%d shadowed=%d geoskip=%d stale=%d",
				light_count, drawn_count, shadowed_count, geoskip_count, stale_owner_count);
			last_gathered = light_count;
			last_drawn = drawn_count;
			last_shadowed = shadowed_count;
			last_geoskip = geoskip_count;
			last_stale = stale_owner_count;
		}
	}
#endif
	return;
}

/* private code */

// The 272B light_datum data array. Its data pointer at data_array+0x44 matches banana's data.h
// layout. Use datum_try_and_get, which validates full handles; datum_get vasserts on absolute indices.
static data_array* light_data_get(void)
{
	return *Memory::GetAddress<data_array**>(0x4E6660);
}

// Shadow master enable.
static bool* g_render_shadows_enabled_get(void)
{
	return Memory::GetAddress<bool*>(0x4E6948);
}

// Nonzero = object markers suppressed -> NULL marker env.
static uint8* g_render_light_object_markers_suppressed_get(void)
{
	return Memory::GetAddress<uint8*>(0x4E6938);
}

// The default cluster for first-person-from-camera lights.
static int32* g_render_light_default_cluster_get(void)
{
	return Memory::GetAddress<int32*>(0x4E680C);
}

// The deferred render-section cache render_lights_new records each light into: a slot
// counter + 258-dword (1032B) entries - the stride from the retail dword indexing.
static int32* g_render_section_deferred_slot_get(void)
{
	return Memory::GetAddress<int32*>(0x4F80DC);
}
static int8* g_render_section_cache_get(void)
{
	return Memory::GetAddress<int8*>(0x4F6458);
}

// The per-light shadow state block render_light_shadow_pass drives:
// flags @0x8E693C (bit0 omni, bit1 bright shadow rgb, bit2|bit5 shadow buffer, bit3 soft tap),
// the in-light-pass marker @0x8E6940, the cinematic object-shadow count @0x8E6A5C.
static uint32* g_render_shadow_flags_get(void)
{
	return Memory::GetAddress<uint32*>(0x4E693C);
}
static uint8* g_render_shadow_flags_active_get(void)
{
	return Memory::GetAddress<uint8*>(0x4E6940);
}
static int32* g_render_cinematic_object_shadow_count_get(void)
{
	return Memory::GetAddress<int32*>(0x4E6A5C);
}

// The shadow colour the setup stages (real_rgb_color @0xE3E2C0) and the shadow range float the tap
// gate reads (flt_E3E3B0).
static real_rgb_color* g_render_shadow_color_get(void)
{
	return Memory::GetAddress<real_rgb_color*>(0xA3E2C0);
}
static real32* g_render_shadow_range_get(void)
{
	return Memory::GetAddress<real32*>(0xA3E3B0);
}

// The engine's VS-constant MIRROR (main_vertex_shader_constants @0xE3C7B0, the full
// c0-c255 file; every tier constant write goes MIRROR+SET through this base).
static real32* g_vs_constants_mirror_get(void)
{
	return Memory::GetAddress<real32*>(0xA3C7B0);
}

// The per-light STAGED screen bounds (byte_E3E2D4 = x0,x1,y0,y1 + two z values,
// pixel-space floats - staged UNCONDITIONALLY by rasterizer_setup_light_projection for every
// light; full-screen fallback when the sphere projection fails). The tier never inherits the
// device scissor rect - it applies from these.
static real32* g_light_staged_bounds_get(void)
{
	return Memory::GetAddress<real32*>(0xA3E2D4);
}

// the per-light loop's __usercall children - reimplemented because register args cannot be
// INVOKE'd (halo2.exe 0x592F40 / 0x59301B)

// compute_perpendicular_vector (halo2.exe 0x592F40): direction in EDI read-only,
// io_vector in ESI written+normalized in place). temp = normalize(io x dir);
// io = normalize(dir x temp) - the operand pairs and subtraction order from the disasm.
static void compute_perpendicular_vector(const real_vector3d* direction, real_vector3d* io_vector)
{
	const real32 dx = direction->i;
	const real32 dy = direction->j;
	const real32 dz = direction->k;
	const real32 ix = io_vector->i;
	const real32 iy = io_vector->j;
	const real32 iz = io_vector->k;

	real_vector3d temp;
	temp.i = (iy * dz) - (iz * dy);
	temp.j = (dx * iz) - (ix * dz);
	temp.k = (ix * dy) - (dx * iy);
	normalize3d(&temp);

	io_vector->k = (dx * temp.j) - (dy * temp.i);
	io_vector->j = (dz * temp.i) - (dx * temp.k);
	io_vector->i = (dy * temp.k) - (dz * temp.j);
	normalize3d(io_vector);
}

// shadow_add_object_to_visibility (halo2.exe 0x59301B): flag in AL -> add_flags =
// al ? 0x3000 : 0x1000, handle in ECX, one stack arg = the collection). The retail walk climbs
// parent_object_index (+0x14) while the datum type byte (+0xAA) != 0 (= _object_type_biped) with
// NO validity checks.
//
// REQUIRED DIVERGENCE (docs/14 section 3): try-based FULL-handle
// resolution per hop - banana's accessors validate what retail walks blind, and a muzzle-flash
// light's owner can be a frame dead by render time. Owner gone -> no add; dead mid-chain link
// -> stop at the last live object.
static void shadow_add_object_to_visibility(bool shadow_flag, datum object_index, c_visibility_collection* visibility)
{
	const int16 add_flags = shadow_flag ? 0x3000 : 0x1000;
	if (object_index == NONE)
		return;

	while (true)
	{
		const object_datum* object = object_try_and_get(object_index);
		if (!object)
			return;											// owner already gone: nothing to add
		if (object->object.object_identifier.get_type() == _object_type_biped)
			break;
		const datum parent = object->object.parent_object_index;
		if (parent == NONE)
			break;
		if (!object_try_and_get(parent))
			break;											// dead link: stop at the last live object
		object_index = parent;
	}

	visibility->m_lists[2]->add_object(object_index, add_flags, 0, NONE);
}

// The engine's object_get_ultimate_parent (halo2.exe 0x5305F8) walks headers with NO null check per hop -
// the walk that AV'd at [NULL+0x14] on a one-frame-dead muzzle-flash owner. Same walk, try-get per
// hop: returns the last LIVE object in the chain, or NONE if the handle is already dead. The second
// half of the required divergence above.
static datum object_get_ultimate_parent_safe(datum object_index)
{
	datum last_valid = NONE;
	while (object_index != NONE)
	{
		const object_datum* object = object_try_and_get(object_index);
		if (!object)
			break;
		last_valid = object_index;
		object_index = object->object.parent_object_index;
	}
	return last_valid;
}

// INVOKE children of render_light_shadow_pass - engine code, strangler stubs

static void __cdecl create_visible_render_primitives(int32 a1)
{
	INVOKE(0x19BDBC, 0x0, create_visible_render_primitives, a1);
}

// c_render_primitive_list::empty (halo2.exe 0x59F53C, __thiscall) on the global primitive list
// @0x8F4EF0 - the per-light caster-primitive check.
static bool render_primitive_list_empty(e_render_layer layer)
{
	typedef bool(__thiscall* empty_t)(void* list, int32 layer);
	return Memory::GetAddress<empty_t>(0x19F53C)(Memory::GetAddress<void*>(0x4F4EF0), layer);
}

// The light-projection setup (halo2.exe 0x67F2AF): stages the per-light screen bounds unconditionally,
// stores shadow_flag, uploads c31..c39 mirror+set. The 1024.0 fifth argument's Vista role is
// unconfirmed - passed verbatim.
static void __cdecl rasterizer_setup_light_projection(int32 light_index, const real32* light_descriptor,
	int32 light_definition_ptr, int16* shadow_geometry, real32 depth_bias, char shadow_flag, real32 intensity)
{
	INVOKE(0x27F2AF, 0x0, rasterizer_setup_light_projection, light_index, light_descriptor,
		light_definition_ptr, shadow_geometry, depth_bias, shadow_flag, intensity);
}

// The shadow-buffer depth/scissor block builder (halo2.exe 0x67F7FC - writes c40..c48).
static void __cdecl rasterizer_build_light_depth_projection_constants(int32 light_index,
	const real32* camera_frame, const real32* light_geometry, const real32* shadow_geometry)
{
	INVOKE(0x27F7FC, 0x0, rasterizer_build_light_depth_projection_constants, light_index,
		camera_frame, light_geometry, shadow_geometry);
}

// The layer trio dispatcher (halo2.exe 0x590B05: begin -> draw -> end, the chain verified
// end to end). The mask brackets CALLS to this, with the disable-stencil latch, never
// the draws inside it.
static char __cdecl draw_specific_render_layer(int32 a1, e_render_layer layer)
{
	return INVOKE(0x190B05, 0x0, draw_specific_render_layer, a1, layer);
}

static void __cdecl render_cinematic_object_shadows(void)
{
	INVOKE(0x19280A, 0x0, render_cinematic_object_shadows);
}

// The engine's per-light restore: scissor-enable off + main target rebind ONLY, never stencil, so
// the tier's own stencil restore stays ours.
static void __cdecl rasterizer_dx9_disable_scissor_restore_target(void)
{
	INVOKE(0x27E76E, 0x0, rasterizer_dx9_disable_scissor_restore_target);
}

// INVOKE children of render_lights_new - engine code, strangler stubs

// Gathers up to 128 qualifying shadow lights from the primary visibility collection's light
// list (m_lists[1]); top-3-per-window selection with cross-frame hysteresis inside.
static int32 __cdecl shadow_build_light_list(s_visible_light* lights)
{
	return INVOKE(0x192D6F, 0x0, shadow_build_light_list, lights);
}

// Builds a light's shadow render geometry from its datum + definition (halo2.exe 0x54E1E1).
// RETAIL IGNORES the result, which is the dying-light race; the loop below checks it.
static bool __cdecl light_build_render_geometry(uint16 light_index, s_light_render_geometry* out_geometry,
	bool check_game_engine, bool* out_valid)
{
	return INVOKE(0x14E1E1, 0x0, light_build_render_geometry, light_index, out_geometry,
		check_game_engine, out_valid);
}

// Resolves an attachment marker's name string id (halo2.exe 0x52FCDF).
static string_id __cdecl object_get_attachment_marker_name(datum object_index, int16 marker_index)
{
	return INVOKE(0x12FCDF, 0x0, object_get_attachment_marker_name, object_index, marker_index);
}

// The first-person marker-transform fill (halo2.exe 0x628B7A - writes projection_point/forward/up).
static void __cdecl sub_628B7A(datum object_index, string_id marker_name, int32* out_position,
	int32* out_forward, int32* out_up)
{
	INVOKE(0x228B7A, 0x0, sub_628B7A, object_index, marker_name, out_position, out_forward, out_up);
}

// Builds the light's visibility frustum descriptors (halo2.exe 0x54CCFB; opaque to us).
static void __cdecl light_build_visibility_projections(real32* projection_point,
	s_light_render_geometry* geometry, s_light_visibility_frustum* out_frustums, int16* out_count)
{
	INVOKE(0x14CCFB, 0x0, light_build_visibility_projections, projection_point, geometry,
		out_frustums, out_count);
}

// Builds the light's per-frame visibility collection (halo2.exe 0x4BCCC7).
static c_visibility_collection* __cdecl rasterizer_build_visibility(int32 a1, int32 flags,
	s_light_visibility_frustum* frustums, int16 frustum_count, real_point3d* bounds_center,
	real32 bounds_radius, void* marker_env, int32 a8, real32 a9, int32 cluster)
{
	return INVOKE(0xBCCC7, 0x0, rasterizer_build_visibility, a1, flags, frustums, frustum_count,
		bounds_center, bounds_radius, marker_env, a8, a9, cluster);
}

// Applies the definition's shadow-exclusion flags onto the collection's entries (halo2.exe
// 0x592C2E; the owner-exclusion table). The shadow_flag box is a real int32, low byte live.
static void __cdecl apply_light_shadow_flags(c_visibility_collection* visibility, uint16 light_index,
	uint16 definition_index, datum ultimate_parent_1, datum ultimate_parent, bool projection_is_omni,
	int32* shadow_flag)
{
	INVOKE(0x192C2E, 0x0, apply_light_shadow_flags, visibility, light_index, definition_index,
		ultimate_parent_1, ultimate_parent, projection_is_omni, shadow_flag);
}

static void __cdecl render_build_visible_objects(int32 a1)
{
	INVOKE(0x19BE9E, 0x0, render_build_visible_objects, a1);
}

static void __cdecl process_visibility_marked_sections(void)
{
	INVOKE(0x1A3BC7, 0x0, process_visibility_marked_sections);
}

// The primary visibility collection accessor (halo2.exe 0x4BAA54).
static void* __cdecl visibility_collection_primary_get(void)
{
	return INVOKE(0xBAA54, 0x0, visibility_collection_primary_get);
}

// g_rasterizer_disable_stencil (halo2.exe 0xE3C63A) - the engine's own "stencil is externally owned"
// latch. set_stencil_mode, the only stencil writer in the accum bracket, early-outs while it is set;
// WITHOUT it every masked layer's begin would silently wipe the EQUAL-128 mask. Latched around the
// layer CALLS, released regardless.
static uint8* g_rasterizer_disable_stencil_get(void)
{
	return Memory::GetAddress<uint8*>(0xA3C63A);
}

// The staged bounds -> a device scissor RECT, floor/ceil + degenerate rejection, matching what tag
// debug's clip_d3d_rect_to_screen does to its projected_bounds. Returns false when the staged rect
// is empty or inverted, in which case skip the draw.
static bool dyn_tier_scissor_rect_from_staged_bounds(RECT* out_rect)
{
	const real32* bounds = g_light_staged_bounds_get();
	if (!bounds)
		return false;
	const real32 x0 = bounds[0];
	const real32 x1 = bounds[1];
	const real32 y0 = bounds[2];
	const real32 y1 = bounds[3];
	if (!(x1 > x0) || !(y1 > y0))
		return false;
	out_rect->left = (LONG)floorf(x0);
	out_rect->right = (LONG)ceilf(x1);
	out_rect->top = (LONG)floorf(y0);
	out_rect->bottom = (LONG)ceilf(y1);
	return true;
}

// the per-light draw - NATIVE, written from the retail body at 0x592A30

// The per-light shadow-buffer + light-accumulation pass: sets up the light projection, computes
// the shadow render flags (generate/apply the shadow buffer - from the shadow flag, the shadow
// colour, the light type + tap bias), draws the shadow-buffer layers when shadowing, then the
// albedo / diffuse / specular accumulation layers. Returns a vestigial value the caller
// discards.
//
// docs/14 section 3: the tier's insertion point is between the shadow-buffer half and the lightaccum
// draws below.
static int32 render_light_shadow_pass(int32 light_index, const real32* light_descriptor, datum definition_index,
	datum ultimate_parent_1, datum ultimate_parent, s_light_render_geometry* geometry, int32 visibility,
	bool shadow_flag, real32 intensity)
{
	const light_definition* definition = static_cast<const light_definition*>(tag_get_fast(definition_index));

	create_visible_render_primitives(0);
	if (shadow_flag)
		shadow_flag = !render_primitive_list_empty(_render_layer_shadow_buffer_generate);

	// the tier-take decision, BEFORE setup: when the tier masks this light the
	// ENGINE's shadow buffer stands down (docs/14 section 3 - no double shadowing), so setup and the
	// flags chain must already see shadow_flag=false. The take (first shadow-flagged light,
	// per-frame latch) is mode-independent; what happens with the light is per F7 mode:
	// 3 = the stage-1 blue diagnostics, 0 + stage>=2 = THE MASK. F8 (stencil_shadow_active)
	// masters the whole tier.
	bool tier_light = false;
	bool tier_mask_active = false;		// set once the mask's field is actually established
	int32 tier_light_ordinal = -1;		// which tier light of the frame this is
	// ALL-LIGHT-TYPES qualification: tag debug shadows every light type, while retail Vista never
	// shadows omnis at all. The loop's
	// shadow_flag chain carries the engine's omni exclusion (conditional on dword_7DC17C,
	// halo2.exe 0x59320F), so spherical lights arrive here with shadow_flag=false. They
	// RE-QUALIFY through the same chain MINUS the omni term: not flagged no-shadow, shadows
	// enabled, positive shadow fade (read at descriptor+116 = light_datum+248 - the field the
	// loop's own chain tests). Everything downstream is type-agnostic: the engine's loop
	// already ran its 6-face gather for omnis (m_lists[2] populated), the point-light volume
	// path is omni-native (w=1), and the staged bounds are unconditional.
	// shadow_flag itself STAYS false for omnis - the engine's shadow-buffer half never runs
	// for them (retail-exact); only OUR mask takes the light.
	bool tier_qualifies = shadow_flag;
	if (!tier_qualifies && (uint16)geometry->projection_type == 0)
	{
		const real32 omni_shadow_fade = *reinterpret_cast<const real32*>(
			reinterpret_cast<const int8*>(light_descriptor) + 116);
		tier_qualifies = !definition->flags.test(_light_definition_no_shadow_dont_cast_any_stencil_shadows)
			&& *g_render_shadows_enabled_get()
			&& omni_shadow_fade > 0.0f;
	}
	if (!tier_qualifies && g_dyn_tier_force_all_lights)
		tier_qualifies = true;		// the Shift+F5 testing lever - every light shadows
	if (g_dyn_tier_available && g_dyn_tier_enabled && g_dyn_tier_stage >= 1
		&& stencil_shadow_active() && tier_qualifies
		&& g_dyn_tier_lights_taken < g_dyn_tier_max_lights)
	{
		// The radius sanity guard, 0.01-10000 wu. The descriptor the pass receives IS datum+132 (the
		// loop passes &light_datum.projection_point), so bounding_sphere_radius @ datum+36 sits at
		// descriptor-96 - offset-derived, so the pass signature does not change.
		const real32 radius = *reinterpret_cast<const real32*>(
			reinterpret_cast<const int8*>(light_descriptor) - 96);
		if (radius >= 0.01f && radius <= 10000.f)
		{
			tier_light_ordinal = g_dyn_tier_lights_taken++;
			tier_light = true;
		}
	}
	const bool tier_masks = tier_light && g_dyn_tier_stage >= 2
		&& stencil_shadow_debug_draw_mode() == 0;
	if (tier_masks)
		shadow_flag = false;	// the engine's shadow-buffer half stands down for the tier light

	rasterizer_setup_light_projection(light_index, light_descriptor, reinterpret_cast<int32>(definition),
		reinterpret_cast<int16*>(geometry), 1024.0f, shadow_flag, intensity);

	// The tier light's caster set comes from the engine's own per-light gather: m_lists[2] entries
	// carrying the 0x2000 casts-shadow flag (docs/14 section 5; list layout from the 0x501B2F store
	// bytes). Placed after setup so the light's staged state is fresh. The volumes draw through the
	// shared module entry at extrusion 1024, with the light position taken from the descriptor base
	// (&light_datum.projection_point, what the loop passes as light_descriptor), and c0-c3 re-uploaded
	// mirror+set first.
	if (tier_light)
	{
#ifdef LOG_STENCIL
		if (!g_dyn_tier_gates_logged)
		{
			g_dyn_tier_gates_logged = true;
			event(_event_verbose, "rasterizer:dx9:stencil:dynamic: gates avail=%d stage=%d f5=%d",
				g_dyn_tier_available, g_dyn_tier_stage, g_dyn_tier_enabled);
		}
#endif
		{
			const c_visibility_collection* collection = reinterpret_cast<const c_visibility_collection*>(visibility);
			c_visibility_object_list* list = collection ? collection->m_lists[2] : NULL;
			int32 total = 0;
			int32 casters = 0;
			int32 volumes = 0;
			int32 articulated_skipped = 0;
			// The tier draws in its own F7 mode (3 = blue-dyn); mode 1 is the sun tier's red view
			// alone. The tint override doubles as the colour-state switch for these draws, so the
			// base mode reads as "real" to everyone else. The MASK path draws the same volumes as
			// real stencil counts (mode 0, no tint - section_draw's z-fail counting branch).
			const bool red_view = (stencil_shadow_debug_draw_mode() == 3);
			const bool draw_volumes = red_view || tier_masks;
			if (list && list->m_objects && list->m_cluster_flags)
			{
				total = list->m_count;
				// CPU cost per tier light: the frame is CPU-bound, so measure before deciding about
				// plane hoisting. Timed: wvp/scissor/clear, the caster volume draws, the mask states.
				LARGE_INTEGER perf_start;
				QueryPerformanceCounter(&perf_start);
				if (draw_volumes && g_dyn_wvp_captured)
				{
					// The loop's children leave c0-c3 stale: restore the entry capture, writing the
					// mirror first so the engine's redundancy cache stays coherent.
					IDirect3DDevice9Ex* device = rasterizer_dx9_device_get_interface();
					real32* mirror = g_vs_constants_mirror_get();
					if (device && mirror)
					{
						memcpy(mirror, g_dyn_captured_wvp, sizeof(g_dyn_captured_wvp));
						device->SetVertexShaderConstantF(0, (const real32*)g_dyn_captured_wvp, 4);
					}
				}
				// The per-light scissor, applied by US from the engine's own staged bounds and never
				// inherited from the device. The rect is identical to what the engine's downstream
				// draws for this light use, so leaving it set is idempotent and the pass-end restore
				// covers the enable. The full-screen staged fallback, when the camera is near or
				// inside the light, clips nothing. At distance this doubles as verification of the
				// rect: volumes visibly clip to a box around the light only if the read is right.
				RECT scissor_rect = { 0, 0, 0, 0 };
				if (draw_volumes && dyn_tier_scissor_rect_from_staged_bounds(&scissor_rect))
				{
					IDirect3DDevice9Ex* device = rasterizer_dx9_device_get_interface();
					if (device)
					{
						device->SetScissorRect(&scissor_rect);
						rasterizer_dx9_set_render_state(D3DRS_SCISSORTESTENABLE, TRUE);
						// The mask's stencil field: clear to 128 WITHIN the light rect, matching tag
						// debug's per-light clear. Clear ignores the scissor, so the rect rides as
						// pRects. A degenerate staged rect gives no field, so the mask stands down for
						// this light - shadow_flag is already suppressed, and one unshadowed frame
						// beats an unbounded whole-screen clear.
						if (tier_masks)
						{
							const D3DRECT clear_rect = { scissor_rect.left, scissor_rect.top,
								scissor_rect.right, scissor_rect.bottom };
							device->Clear(1, &clear_rect, D3DCLEAR_STENCIL, 0, 1.f, 128);
							tier_mask_active = true;
						}
					}
				}
				// The tier's volumes draw CYAN-BLUE in the red view so they are tellable from the
				// lightmap tier's red in the same frame (caller-owned storage, cleared after the
				// loop). Not green: mode 2 already means green, and red/green is the worst
				// colourblind pair.
				static const real32 k_dyn_tier_debug_tint[4] = { 0.125f, 0.5f, 1.f, 0.375f };
				if (red_view)
					stencil_shadow_debug_tint_override_set(k_dyn_tier_debug_tint);
				// The shadow fade, tag debug's stipple: opacity = min(shadow_fade@descriptor+116,
				// intensity). shadow_fade is the engine's own per-frame DISTANCE fade for this
				// light's shadow, produced by render_light at scale 5.0 and clamped to illumination;
				// intensity carries the selection/illumination ramp that also scales the light's
				// rendered brightness, so the shadow dims in step with both. The shipped stipple path
				// screen-doors the stencil counts below ~0.995. The F7 blue view stays full-strength,
				// diagnostics wanting visibility.
				real32 fade_opacity = *reinterpret_cast<const real32*>(
					reinterpret_cast<const int8*>(light_descriptor) + 116);
				if (intensity < fade_opacity)
					fade_opacity = intensity;
				if (fade_opacity < 0.0f)
					fade_opacity = 0.0f;
				if (fade_opacity > 1.0f)
					fade_opacity = 1.0f;
				for (int32 i = 0; i < total; i++)
				{
					if ((list->m_cluster_flags[i] & 0x2000) == 0)
						continue;
					casters++;
					if (draw_volumes)
					{
						int32 section_skips = 0;
						volumes += stencil_shadow_draw_object_volume_point_light(
							list->m_objects[i],
							reinterpret_cast<const real_point3d*>(light_descriptor),
							1024.0f, &section_skips,
							red_view ? 1.0f : fade_opacity);
						articulated_skipped += section_skips;
					}
				}
				if (red_view)
					stencil_shadow_debug_tint_override_set(NULL);

				// The EQUAL-128 mask around the accum layers, matching tag debug: the counts are
				// down, and writemask 0 means the accum draws only TEST, never write. The
				// disable-stencil latch silences set_stencil_mode, the bracket's only stencil writer,
				// for the duration of the layer calls. COLORWRITE is restored first, since the volume
				// draws' real branch disabled it and the accum layers need it; every state goes
				// through the dual-write wrapper so the engine's cache stays honest.
				if (tier_mask_active)
				{
					rasterizer_dx9_set_render_state(D3DRS_COLORWRITEENABLE,
						D3DCOLORWRITEENABLE_RED | D3DCOLORWRITEENABLE_GREEN
						| D3DCOLORWRITEENABLE_BLUE | D3DCOLORWRITEENABLE_ALPHA);
					*g_rasterizer_disable_stencil_get() = 1;
					rasterizer_dx9_set_render_state(D3DRS_TWOSIDEDSTENCILMODE, FALSE);
					rasterizer_dx9_set_render_state(D3DRS_STENCILENABLE, TRUE);
					rasterizer_dx9_set_render_state(D3DRS_STENCILFUNC, D3DCMP_EQUAL);
					rasterizer_dx9_set_render_state(D3DRS_STENCILREF, 128);
					rasterizer_dx9_set_render_state(D3DRS_STENCILMASK, 0xFFFFFFFF);
					rasterizer_dx9_set_render_state(D3DRS_STENCILWRITEMASK, 0);
					rasterizer_dx9_set_render_state(D3DRS_STENCILPASS, D3DSTENCILOP_KEEP);
					rasterizer_dx9_set_render_state(D3DRS_STENCILFAIL, D3DSTENCILOP_KEEP);
					rasterizer_dx9_set_render_state(D3DRS_STENCILZFAIL, D3DSTENCILOP_KEEP);
				}

				// Perf end - accumulated per tier light, reported every ~10s to match the lightmap
				// tier's own perf-line cadence.
				LARGE_INTEGER perf_end;
				LARGE_INTEGER perf_freq;
				QueryPerformanceCounter(&perf_end);
				QueryPerformanceFrequency(&perf_freq);
				const real64 light_ms = (real64)(perf_end.QuadPart - perf_start.QuadPart)
					* 1000.0 / (real64)perf_freq.QuadPart;
				static real64 perf_accum_ms = 0.0;
				static real64 perf_max_ms = 0.0;
				static uint32 perf_samples = 0;
				perf_accum_ms += light_ms;
				if (light_ms > perf_max_ms)
					perf_max_ms = light_ms;
				if (++perf_samples >= 4800)
				{
#ifdef LOG_STENCIL
					event(_event_status, "rasterizer:dx9:stencil:dynamic: perf per-light avg=%.3fms max=%.3fms over %u tier lights",
						perf_accum_ms / perf_samples, perf_max_ms, perf_samples);
#endif
					perf_accum_ms = 0.0;
					perf_max_ms = 0.0;
					perf_samples = 0;
				}
			}
#ifdef LOG_STENCIL
			// Change-log state PER ORDINAL: with up to 3 tier lights per frame, shared statics would
			// see every light as a "change" and log every frame.
			static int32 last_total[3] = { -1, -1, -1 };
			static int32 last_casters[3] = { -1, -1, -1 };
			static int32 last_volumes[3] = { -1, -1, -1 };
			const int32 log_slot = (tier_light_ordinal >= 0 && tier_light_ordinal < 3) ? tier_light_ordinal : 0;
			if (total != last_total[log_slot] || casters != last_casters[log_slot]
				|| volumes != last_volumes[log_slot])
			{
				last_total[log_slot] = total;
				last_casters[log_slot] = casters;
				last_volumes[log_slot] = volumes;
				// The staged rect and the wvp capture state ride the census line, verified live
				// before the draw depends on them.
				RECT rect = { 0, 0, 0, 0 };
				const bool rect_valid = dyn_tier_scissor_rect_from_staged_bounds(&rect);
				// The light position rides the line too: for OMNI lights the volume math uses
				// projection_point (+132), and if that reads wrong for a sphere light - against its
				// bounding-sphere centre - the facing test degenerates.
				event(_event_verbose, "rasterizer:dx9:stencil:dynamic: light %d casters=%d of visible=%d volumes=%d artic_skip=%d red=%d mask=%d rect=%s(%ld,%ld)-(%ld,%ld) wvp=%d omni=%d pos=(%.2f,%.2f,%.2f)",
					tier_light_ordinal, casters, total, volumes, articulated_skipped, red_view, tier_mask_active,
					rect_valid ? "" : "INVALID",
					rect.left, rect.top, rect.right, rect.bottom, g_dyn_wvp_captured,
					(uint16)geometry->projection_type == 0,
					light_descriptor[0], light_descriptor[1], light_descriptor[2]);
			}
#endif

			// The environment tier's feed census: consume the engine's per-light cluster and instance
			// sets (m_lists[0] = clusters with pre-set flags, m_lists[3] = instanced geometry) and
			// count sphere-vs-shadow-radius verdicts. The numbers verify the feed against the map's
			// layout before any geometry hangs off it.
			if (g_env_tier_enabled && g_env_tier_stage >= 0 && collection)
			{
				c_visibility_object_list* cluster_list = collection->m_lists[0];
				c_visibility_object_list* instance_list = collection->m_lists[3];
				int32 env_clusters = 0;
				int32 env_in_range = 0;
				int32 env_first_in_range = -1;		// the stage-1 cluster
				const int32 env_instances = instance_list ? (int32)instance_list->m_count : 0;
				UNREFERENCED_PARAMETER(env_instances);		// census only - read by the log below
				structure_bsp* bsp = global_structure_bsp_get();
				const real_point3d* light_position = reinterpret_cast<const real_point3d*>(light_descriptor);
				const real32 light_radius = *reinterpret_cast<const real32*>(
					reinterpret_cast<const int8*>(light_descriptor) - 96);	// datum+36
				if (cluster_list && cluster_list->m_objects && bsp && bsp->clusters.count > 0)
				{
					env_clusters = cluster_list->m_count;
					for (int32 ci = 0; ci < env_clusters; ci++)
					{
						const int32 cluster_index = (int32)(uint16)cluster_list->m_objects[ci];
						if (cluster_index >= bsp->clusters.count)
							continue;
						const structure_cluster* cluster = (const structure_cluster*)TAG_BLOCK_GET_ELEMENT(
							&bsp->clusters, cluster_index, structure_cluster);
						if (cluster && stencil_shadow_light_touches_bounds(
							&cluster->bounds, light_position, light_radius))
						{
							env_in_range++;
							if (env_first_in_range == -1)
								env_first_in_range = cluster_index;
						}
					}
				}

				// docs/15 section 8 stage 1: ONE cluster's volumes in the blue view - the first
				// in-range one - through the weld cache. The peek/get pair is tri-state so a cached
				// negative is never retried. g_env_reach_cull_disable is the reach-cull A/B: it flips
				// the facing reach between the light radius (cull on, so the volume must hug the
				// light) and 0 (cull off, so the map-spanning volume returns), and that verdict
				// decides the radius-scaled extrusion against tag debug's flat 1024 before stage 2.
				// Magenta tint, beside the model casters' cyan. Worldspace, so NULL matrix and world
				// facing.
				int32 env_drawn = 0;
				int32 env_facing_pop = -1;
				if (g_env_tier_stage >= 1 && red_view && env_first_in_range != -1 && bsp)
				{
					s_stencil_shadow_section* cluster_shadow = NULL;
					if (!stencil_shadow_cluster_peek(get_global_structure_bsp_index(), env_first_in_range, &cluster_shadow))
						cluster_shadow = stencil_shadow_cluster_get(bsp, get_global_structure_bsp_index(), env_first_in_range);
					if (cluster_shadow && cluster_shadow->valid)
					{
						const real32 env_reach = g_env_reach_cull_disable ? 0.f : light_radius;
						static uint32 env_facing[k_stencil_shadow_facing_bitvector_words];
						stencil_shadow_build_facing_bitvector(cluster_shadow, light_position, true,
							env_facing, env_reach);
						env_facing_pop = 0;
						for (uint32 t = 0; t < cluster_shadow->plane_count; t++)
						{
							if (BIT_VECTOR_TEST_FLAG(env_facing, t))
								env_facing_pop++;
						}
						real32 env_extrusion = light_radius * k_stencil_shadow_environment_extrusion_scale;
						if (env_extrusion < k_stencil_shadow_environment_extrusion_minimum)
							env_extrusion = k_stencil_shadow_environment_extrusion_minimum;
						static const real32 k_env_tier_debug_tint[4] = { 0.9f, 0.25f, 0.9f, 0.375f };
						stencil_shadow_debug_tint_override_set(k_env_tier_debug_tint);
						stencil_shadow_section_draw(cluster_shadow, env_facing, light_position, true,
							NULL, env_extrusion, 1.f, k_stencil_shadow_environment_self_shadow_bias);
						stencil_shadow_debug_tint_override_set(NULL);
						env_drawn = 1;
					}
				}
#ifdef LOG_STENCIL
				static int32 env_last_clusters[3] = { -1, -1, -1 };
				static int32 env_last_in_range[3] = { -1, -1, -1 };
				static int32 env_last_drawn[3] = { -1, -1, -1 };
				const int32 env_slot = (tier_light_ordinal >= 0 && tier_light_ordinal < 3) ? tier_light_ordinal : 0;
				if (env_clusters != env_last_clusters[env_slot]
					|| env_in_range != env_last_in_range[env_slot]
					|| env_drawn != env_last_drawn[env_slot])
				{
					env_last_clusters[env_slot] = env_clusters;
					env_last_in_range[env_slot] = env_in_range;
					env_last_drawn[env_slot] = env_drawn;
					event(_event_verbose, "rasterizer:dx9:stencil:environment: census %d clusters=%d in_range=%d instances=%d drawn=%d facing=%d cull=%d r=%.2f",
						tier_light_ordinal, env_clusters, env_in_range, env_instances,
						env_drawn, env_facing_pop, !g_env_reach_cull_disable, light_radius);
				}
#endif
			}
		}
	}

	*g_render_shadow_flags_active_get() = 1;	// the in-light-pass marker

	real_rgb_color* shadow_color = g_render_shadow_color_get();
	if (!cinematic_in_progress() && definition->type == _light_type_sphere)	// not cinematic + omni
	{
		shadow_color->red = 0.0f;
		shadow_color->green = 0.0f;
		shadow_color->blue = 0.0f;
	}

	// Shadow render flags, bit-exact with retail: bit0 = sphere/omni light, bit1 = non-dim shadow
	// colour, bit2 = generate shadow buffer, bit3 = soft/tap shadows. LOWORD read, retail's own
	// encoding - retail tests the low word here but branches on the full int32, so read it narrow.
	uint32 flags = (uint16)geometry->projection_type ? 0 : 1;
	if (shadow_color->red <= 0.0099999998f && shadow_color->green <= 0.0099999998f && shadow_color->blue <= 0.0099999998f)
		flags &= ~2u;
	else
		flags |= 2;
	if (!shadow_flag || (flags & 1) != 0)
	{
		flags &= 0xFFFFFFD3;
	}
	else
	{
		flags |= 0x24;
		if (*g_render_shadow_range_get() <= 0.0f || definition->shadow_tap_bias == _light_definition_shadow_1_tap)
			flags &= ~8u;
		else
			flags |= 8;
	}
	flags &= ~0x10u;
	*g_render_shadow_flags_get() = flags;

	if ((flags & 4) != 0)
	{
		rasterizer_build_light_depth_projection_constants(light_index, light_descriptor,
			reinterpret_cast<const real32*>(definition), reinterpret_cast<const real32*>(geometry));
		draw_specific_render_layer(1, _render_layer_shadow_buffer_generate);
	}
	if ((flags & 4) != 0 || (flags & 2) != 0 || definition->flags.test(_light_definition_only_render_in_first_person))
		draw_specific_render_layer(1, _render_layer_shadow_buffer_apply);
	if (cinematic_in_progress() && (flags & 4) != 0 && *g_render_cinematic_object_shadow_count_get() > 0)
		render_cinematic_object_shadows();
	draw_specific_render_layer(1, _render_layer_lightaccum_albedo);
	draw_specific_render_layer(1, _render_layer_lightaccum_diffuse);
	if ((flags & 1) != 0 && (flags & 2) != 0)
		draw_specific_render_layer(1, _render_layer_lightaccum_specular);

	// The mask teardown. The engine's restore below covers scissor-ENABLE and the render target
	// only, so the stencil restore is ours - tag debug's layer-17-end analogue. The latch releases
	// REGARDLESS of how the layer calls went, the bracket wrapping the calls rather than the draws,
	// so an empty primitive list changes nothing here.
	if (tier_mask_active)
	{
		*g_rasterizer_disable_stencil_get() = 0;
		rasterizer_dx9_set_render_state(D3DRS_STENCILENABLE, FALSE);
	}
	rasterizer_dx9_disable_scissor_restore_target();

	*g_render_shadow_flags_active_get() = 0;
	*g_render_shadow_flags_get() = 0;
	(void)ultimate_parent_1;
	(void)ultimate_parent;
	(void)visibility;
	return 0;
}
