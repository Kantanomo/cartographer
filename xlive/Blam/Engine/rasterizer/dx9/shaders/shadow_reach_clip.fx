// shadow_reach_clip.fx -- per-pixel REACH BOUND for stencil shadow volumes (it. 557, rewritten it. 566).
//
// WHY THIS EXISTS
// ---------------
// The port is stuck between two measured facts:
//   * finite 2.0 wu extrusion puts the FAR CAP inside the scene, where it grazes receiver geometry and
//     fragments the shadow (it. 525/539; td does the same -- it. 544 check 3);
//   * effectively-infinite extrusion (the "500 wu" F6 mode, which the far-plane clamp collapses onto the
//     far plane -- MEASURED it. 550) has no far cap and is visually clean, but the volume then reaches
//     forever and leaks through every wall behind the caster.
//
// Dynamic and clipped extrusion both tried to place a finite cap *somewhere better*. Any finite cap is in
// the scene somewhere, so both relocated the artefact instead of removing it. This runs the volume
// INFINITE and bounds its REACH per pixel instead.
//
// WHY IT IS SOUND (and the clip-plane attempt was not)
// ----------------------------------------------------
// it. 530-541 clipped the GEOMETRY against a plane. That cut the volume open: a clipped side quad no
// longer met its neighbours, closure broke, and the counts stopped cancelling.
//
// This clips the PIXEL. The discard reads only the RECEIVER, which is fixed for a given pixel and
// identical for every volume fragment covering it -- so a pixel's crossings are ALL counted or ALL
// discarded, never half. Closure is untouched.
//
// WHY THIS IS THE THIRD FORMULATION -- READ BEFORE "SIMPLIFYING" IT
// ----------------------------------------------------------------
// it. 557 and it. 563 both compared VIEW DEPTHS instead of reconstructing position, specifically to keep
// the unverified depth ENCODING SEMANTIC non-load-bearing. Both failed, and it. 565 measured why:
//
//     caster_depth=-0.03wu  threshold=-2.65wu  ->  "kill everything in front of the camera"
//
// A downward light with a horizontal view puts the shadow's end point at LOWER view depth than the
// caster, so the threshold goes negative while receivers sit at every depth. **"Within R of the caster"
// is a 3D distance and view depth cannot express it, at any constant.** Do not attempt a fourth
// depth-proxy variant.
//
// So this reconstructs the receiver's world position and measures the real thing. That makes the
// encoding semantic load-bearing for the first time -- the correct trade, because a wrong semantic now
// produces VISIBLY wrong shadows instead of silently absent ones.
//
// DEPTH SOURCE AND DECODE
// -----------------------
// `_rasterizer_target_z_a8b8g8r8` (target 22), the engine's depth-as-colour MRT output. The volume pass
// suppresses that MRT binding for its own duration (it. 561) -- without which this samples a live render
// target and reads undefined data (it. 560).
//
// Packing VERIFIED from the engine's own consumer, fog_atmospheric_apply.fx:20-22:
//     depth.r += depth.g * 0.00390625;   // 1/256
//     depth.r += depth.b * 0.000015;     // ~1/65536
// Reproduced exactly. That the value is LINEAR normalised depth (not NDC z) is ASSUMED -- fog cubes it
// and indexes a gradient linearly, both useless on NDC z, which sits at ~0.99 across the whole scene.

// it. 618 — CONSTANTS LIVE AT c216-c223, THE TOP OF ps_3_0's RANGE. They used to sit at c0-c7 and
// STOMPED THE ENGINE'S OWN PIXEL-SHADER CONSTANTS: fog_atmospheric_apply.fx uses c0-c2, weather_plate.fx
// uses c0-c2 (tint_colors) and c3-c5 (dot_factors), bloom_simple.fx uses c0-c2. Every reach-clip draw
// overwrote those with camera vectors, so later draws tinted themselves with our forward vector —
// user-observed as a wall decal changing colour shot to shot.
//
// Same convention the rest of this file already follows: the vertex constants sit at c254/c255 and the
// ps_2_0 tint at c31, each the top of its range. ps_3_0 allows c0-c223, so the top 8 are c216-c223.
sampler2D scene_depth : register(s0);

float4 reach_c    : register(c216);	// xy = 1/viewport, z = proj A, w = proj B  (see decode below)
float4 cam_pos_c  : register(c217);	// xyz = camera point, w = reach (world units)
float4 cam_fwd_c  : register(c218);	// xyz = camera forward (unit)
float4 cam_rgt_c  : register(c219);	// xyz = camera right (unit), w = tan(hfov/2)
float4 cam_up_c   : register(c220);	// xyz = camera up (unit),    w = tan(vfov/2)
float4 caster_c   : register(c221);	// xyz = caster centre, world
float4 extrude_c  : register(c222);	// xyz = extrusion direction (AWAY from the light, unit)
float4 spread_c   : register(c223);	// xyz = horizontal shadow direction (unit), w = d(along)/d(lateral)

float4 main(float2 vpos : VPOS) : COLOR
{
    // VPOS is the pixel centre in pixels; +0.5 lands on the texel centre of a 1:1 target.
    float2 uv = (vpos + 0.5f) * reach_c.xy;

    float3 packed = tex2D(scene_depth, uv).rgb;
    float depth01 = packed.r
                  + packed.g * 0.00390625f
                  + packed.b * 0.000015f;

    // it. 568 — LINEAR, confirmed by a SECOND independent engine consumer.
    //
    // A brief NDC-z theory (the "no shadows at all" signature would fit reconstructing everything at
    // ~z_far) is REFUTED: `weather_plate.fx:29-37` decodes this same packing and then applies an
    // AFFINE remap, `dot(float3(decoded,1,1), per_vertex_coeffs)`. NDC -> linear is a RECIPROCAL, which
    // an affine remap cannot express. Together with fog_atmospheric_apply.fx cubing the value and
    // indexing a gradient linearly, two independent engine shaders treat it as linear.
    //
    // So the encoding is NOT the reason it. 566 drew nothing. reach_c.z carries z_far again.
    float view_depth = depth01 * reach_c.z;

    // Ray through this pixel, scaled so its forward component is exactly 1. `right` and `up` are
    // perpendicular to `forward`, so dot(dir, forward) == 1 and `cam + dir * view_depth` lands at
    // precisely that view depth -- no normalize, and no division.
    float sx = (uv.x * 2.0f - 1.0f) * cam_rgt_c.w;
    float sy = (1.0f - uv.y * 2.0f) * cam_up_c.w;
    float3 dir = cam_fwd_c.xyz + cam_rgt_c.xyz * sx + cam_up_c.xyz * sy;

    float3 receiver = cam_pos_c.xyz + dir * view_depth;

    // Distance from the caster ALONG THE LIGHT. Signed on purpose: anything in front of the caster
    // (negative) must always be kept, or the caster would stop shadowing surfaces nearer than itself.
    //
    // it. 589: a height-based bound was written here and REVERTED before shipping. It is exact for
    // horizontal floors, but the user observed shadows landing on WALLS ("if i approach a wall the
    // shadow starts coming back"), and a height bound would clip a wall shadow away entirely — a
    // strictly worse trade. `along` handles any receiver orientation.
    float along = dot(receiver - caster_c.xyz, extrude_c.xyz);

    // it. 590 — THE BOUND IS A LINE, NOT A CONSTANT.
    //
    // A receiving surface is not perpendicular to the light, so `along` VARIES across it: further along
    // the shadow means a larger `along` even on perfectly flat ground. A constant bound must therefore
    // either cut the shadow short (it. 587) or be padded enough to leak (it. 589) — a single per-caster
    // scalar cannot do both, and both were observed.
    //
    // But that variation is a property of the RECEIVER, and it is measurable: two rays give `along` at
    // two points, and the slope between them is d(along)/d(lateral). The bound then TRACKS the surface:
    //
    //     bound(lateral) = along_at_caster + slope * lateral + margin
    //
    // Exact for any planar receiver at any orientation — floor, ramp or wall — which is what makes the
    // padding unnecessary. Leak returns to the MARGIN ALONE while the shadow keeps its full extent.
    float lateral = dot(receiver - caster_c.xyz, spread_c.xyz);
    float bound = cam_pos_c.w + spread_c.w * lateral;
    clip(bound - along);

    // Colour is irrelevant: the volume pass runs with COLORWRITEENABLE = 0 and exists only to move the
    // stencil counters. The clip() above is this shader's entire purpose.
    return float4(0.f, 0.f, 0.f, 0.f);
}
