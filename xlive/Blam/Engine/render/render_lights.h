#pragma once
#include "math/color_math.h"

/* structures */

// max: 1
struct render_lighting
{
	real_rgb_color ambient;
	real_vector3d shadow_direction;
	real32 lighting_accuracy;
	real32 shadow_opacity;
	real_rgb_color primary_direction_color;
	real_vector3d primary_direction;
	real_rgb_color secondary_direction_color;
	real_vector3d secondary_direction;
	uint16 sh_index;
	int16 pad;
};
ASSERT_STRUCT_SIZE(render_lighting, 84);

// ================================ dynamic-light tier (docs/14 rev 3) ================================
// Written from the R38 implementation contract (td-dynamic-restock-ledger.md) with the R41
// simplifications — every named field below carries its retail derivation; everything the tier
// does not read is PADDING by design (impl2 J1). The parked tier's file-local s_dyn_light_datum
// (render_stencil_shadow_dynamic.cpp) is dead code and is not extended (PF-1).

// The runtime light/shadow render-geometry block. R41: the ONLY field our code reads is
// projection_type (+0 — the shadow_flag chain and the omni test, R18/R37); the rest passes
// opaquely to INVOKE'd engine children. Size 124 = the retail loop's own stack gap
// [esp+4Ch]->[esp+C8h] (R38).
struct s_light_render_geometry
{
	int32 projection_type;		// 0x00: 0 = omni/sphere, 1 = perspective/spot, 2 = ortho.
								// HAZARD (R37): retail tests LOWORD here but branches on the
								// full int32 elsewhere — our reads use (uint16) like the loop.
	int8 opaque[120];
};
ASSERT_STRUCT_SIZE(s_light_render_geometry, 124);

// The 272-byte runtime dynamic-light datum (stride from retail bytes: imul 110h, R36; data
// pointer at data_array+0x44). ONLY the fields the per-light loop reads are named (R41):
struct light_datum
{
	int8 pad_0[4];
	datum definition_index;			// +4: 'ligh' tag (R36 gather bytes: tag_instances resolve)
	int8 pad_8[16];
	real_point3d bounding_sphere_center;	// +24: fed to rasterizer_build_visibility (R18/R36)
	real32 bounding_sphere_radius;			// +36: same (R36: light_get_bounding_sphere bytes)
	int8 pad_40[32];
	uint16 cluster_reference;		// +72: the visibility cluster; NONE-gate in qualify (R40),
									//      cluster arg to rasterizer_build_visibility (R18)
	int8 pad_74[2];
	datum owner_object_index;		// +76: (R36 bytes: [esi+4Ch])
	int8 pad_80[4];
	uint16 attachment_marker_index;	// +84: 0xFFFF = unattached (R18: word +42)
	int8 pad_86[46];
	real_point3d projection_point;	// +132: the light frame origin / light_descriptor base (R18)
	real_point3d look_at_point;		// +144: written = projection_point + forward (R18)
	int8 pad_156[16];
	real_vector3d forward;			// +172: marker-refresh target (R18)
	real_vector3d up;				// +184: perpendicular-rebuild target (R18/R35)
	int8 pad_196[52];
	real32 shadow_fade;				// +248: > 0 gates shadow casting (R17 producer, R18 consumer)
	int8 pad_252[20];
};
ASSERT_STRUCT_SIZE(light_datum, 272);

/* prototypes */

/* prototypes */

void __cdecl render_lights(void);

void __cdecl render_light_clear_data(void);

void __cdecl render_cinematic_lightmap_shadows(int32 effect_flag);

void __cdecl render_lights_new(void);

// impl2 stage 2 (J17): the dynamic tier's F5 toggle — flips the tier between active and
// retail-exact standdown. Wired to F5 in the stencil-shadow debug view (replacing the parked
// module's refuse-stub).
void render_lights_dyn_tier_toggle(void);

// impl2 J25: Shift+F5 — force EVERY processed light to tier-shadow (overrides the tag-side
// no-shadow flag and fade gates; sanity caps still apply). A testing lever.
void render_lights_dyn_tier_force_all_toggle(void);

// cluster-impl K2: F4 = the ENVIRONMENT tier toggle (rewired from the parked module's stub);
// Shift+F4 = the reach-cull A/B lever (the it. 651 verification, docs/15 §8 stage 1).
void render_lights_env_tier_toggle(void);
void render_lights_env_reach_cull_toggle(void);
