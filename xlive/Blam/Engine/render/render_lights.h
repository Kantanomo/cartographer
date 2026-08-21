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

/* dynamic-light tier (docs/14) - only the fields the per-light loop reads are named; everything
   else is padding by design */

// The runtime light/shadow render-geometry block. The only field our code reads is projection_type;
// the rest passes opaquely to INVOKE'd engine children. Size 124 = the retail loop's own stack gap
// [esp+4Ch]->[esp+C8h].
struct s_light_render_geometry
{
	int32 projection_type;		// 0x00: 0 = omni/sphere, 1 = perspective/spot, 2 = ortho.
								// HAZARD: retail tests LOWORD here but branches on the full int32
								// elsewhere - our reads use (uint16) like the loop.
	int8 opaque[120];
};
ASSERT_STRUCT_SIZE(s_light_render_geometry, 124);

// The 272-byte runtime dynamic-light datum (stride from retail's imul 110h; data pointer at
// data_array+0x44).
struct light_datum
{
	int8 pad_0[4];
	datum definition_index;			// +4: 'ligh' tag
	int8 pad_8[16];
	real_point3d bounding_sphere_center;	// +24: fed to rasterizer_build_visibility
	real32 bounding_sphere_radius;			// +36: same
	int8 pad_40[32];
	uint16 cluster_reference;		// +72: the visibility cluster; NONE-gated in qualify, and the
									//      cluster arg to rasterizer_build_visibility
	int8 pad_74[2];
	datum owner_object_index;		// +76
	int8 pad_80[4];
	uint16 attachment_marker_index;	// +84: 0xFFFF = unattached
	int8 pad_86[46];
	real_point3d projection_point;	// +132: the light frame origin / light_descriptor base
	real_point3d look_at_point;		// +144: written = projection_point + forward
	int8 pad_156[16];
	real_vector3d forward;			// +172: marker-refresh target
	real_vector3d up;				// +184: perpendicular-rebuild target
	int8 pad_196[52];
	real32 shadow_fade;				// +248: > 0 gates shadow casting
	int8 pad_252[20];
};
ASSERT_STRUCT_SIZE(light_datum, 272);

/* prototypes */

void __cdecl render_lights(void);

void __cdecl render_light_clear_data(void);

void __cdecl render_cinematic_lightmap_shadows(int32 effect_flag);

void __cdecl render_lights_new(void);

// The dynamic tier's F5 toggle: flips the tier between active and retail-exact standdown.
void render_lights_dyn_tier_toggle(void);

// Shift+F5 - force EVERY processed light to tier-shadow (overrides the tag-side
// no-shadow flag and fade gates; sanity caps still apply). A testing lever.
void render_lights_dyn_tier_force_all_toggle(void);

// F4 = the ENVIRONMENT tier toggle; Shift+F4 = the reach-cull A/B lever (docs/15 section 8 stage 1).
void render_lights_env_tier_toggle(void);
void render_lights_env_reach_cull_toggle(void);

// Reset this module's per-map diagnostic latches. Called by stencil_shadow_cache_clear, alongside
// the other tiers' resets - a latch that reports once belongs here, or it describes only the first
// map of a session.
void render_lights_reset_diagnostics(void);
