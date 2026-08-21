// shadow_reach_clip.fx -- the per-pixel REACH BOUND for stencil shadow volumes.
//
// The port is stuck between two measured facts: a finite extrusion puts the FAR CAP inside the scene,
// where it grazes receiver geometry and fragments the shadow (tag debug has the same problem), while
// an effectively-infinite extrusion has no far cap and is visually clean but reaches forever and leaks
// through every wall behind the caster. Every attempt to place a finite cap somewhere better only
// relocated the artefact. This runs the volume infinite and bounds its REACH per pixel instead.
//
// WHY CLIPPING THE PIXEL IS SOUND AND CLIPPING THE GEOMETRY WAS NOT. Clipping the volume against a
// plane cuts it open: a clipped side quad no longer meets its neighbours, closure breaks and the
// counts stop cancelling. The discard below reads only the RECEIVER, which is fixed for a given pixel
// and identical for every volume fragment covering it, so a pixel's crossings are all counted or all
// discarded, never half.
//
// READ BEFORE "SIMPLIFYING" THIS INTO A DEPTH COMPARISON. Two earlier formulations compared VIEW
// DEPTHS rather than reconstructing position, to keep the unverified depth encoding non-load-bearing.
// Both failed for the same reason: a downward light with a horizontal view puts the shadow's end at
// LOWER view depth than the caster, so the threshold goes negative while receivers sit at every depth
// (measured: caster -0.03 wu against a threshold of -2.65 wu, i.e. "kill everything in front of the
// camera"). "Within R of the caster" is a 3D distance and view depth cannot express it at any
// constant. Do not attempt a fourth depth-proxy variant.
//
// DEPTH SOURCE. `_rasterizer_target_z_a8b8g8r8`, the engine's depth-as-colour MRT output. The volume
// pass suppresses that MRT binding for its own duration; without that this samples a live render
// target and reads undefined data. The packing below is reproduced from the engine's own consumer
// fog_atmospheric_apply.fx, and the value is LINEAR normalised depth rather than NDC z -- confirmed by
// two independent engine shaders, since fog cubes it and indexes a gradient linearly while
// weather_plate.fx applies an AFFINE remap, and NDC -> linear is a reciprocal an affine remap cannot
// express.

// CONSTANTS LIVE AT c216-c223, THE TOP OF ps_3_0's RANGE, and must stay clear of c0-c7: the engine's
// own pixel shaders use that range for tints (fog_atmospheric_apply, weather_plate, bloom_simple), so
// reach-clip draws there overwrote them with camera vectors and later draws tinted themselves with our
// forward vector -- observed as a wall decal changing colour shot to shot. Same convention as the rest
// of this system, where the vertex constants sit at c254/c255 and the ps_2_0 tint at c31.
sampler2D scene_depth : register(s0);

float4 reach_c    : register(c216);	// xy = 1/viewport, z = z_far
float4 cam_pos_c  : register(c217);	// xyz = camera point, w = reach (world units)
float4 cam_fwd_c  : register(c218);	// xyz = camera forward (unit)
float4 cam_rgt_c  : register(c219);	// xyz = camera right (unit), w = tan(hfov/2)
float4 cam_up_c   : register(c220);	// xyz = camera up (unit),    w = tan(vfov/2)
float4 caster_c   : register(c221);	// xyz = caster centre, world
float4 extrude_c  : register(c222);	// xyz = extrusion direction (AWAY from the light, unit)
float4 spread_c   : register(c223);	// xyz = horizontal shadow direction (unit), w = d(along)/d(lateral)

// The `half` declarations compile to _pp hints, which DX10-and-later hardware running D3D9 ignores -
// its ALUs are fp32 regardless. On hardware that honours them this shader would visibly break: it
// reconstructs world positions at magnitudes of hundreds of world units, where fp16's 11-bit mantissa
// steps 0.5-1.0 wu against a bound base of ~0.7 wu, and the kill test disintegrates into noise. If
// reach-mode shadows ever look wrong on old hardware, revert this first.
//
// The return type stays float4 because D3D9 validates that a pixel shader writes all four components
// of COLOR0 (error X4530). The constant-zero return is the legal minimum and the ROP masks it.
float4 main(float2 vpos : VPOS) : COLOR
{
    // VPOS is the pixel centre in pixels; +0.5 lands on the texel centre of a 1:1 target.
    half2 uv = (vpos + 0.5f) * reach_c.xy;

    half3 packed = tex2D(scene_depth, uv).rgb;
    half depth01 = packed.r
                 + packed.g * 0.00390625f
                 + packed.b * 0.000015f;
    half view_depth = depth01 * reach_c.z;

    // Ray through this pixel, scaled so its forward component is exactly 1. `right` and `up` are
    // perpendicular to `forward`, so dot(dir, forward) == 1 and `cam + dir * view_depth` lands at
    // precisely that view depth -- no normalize, and no division.
    half sx = (uv.x * 2.0f - 1.0f) * cam_rgt_c.w;
    half sy = (1.0f - uv.y * 2.0f) * cam_up_c.w;
    half3 dir = cam_fwd_c.xyz + cam_rgt_c.xyz * sx + cam_up_c.xyz * sy;

    half3 receiver = cam_pos_c.xyz + dir * view_depth;

    // Distance from the caster ALONG THE LIGHT. Signed on purpose: anything in front of the caster is
    // negative and must always be kept, or the caster would stop shadowing surfaces nearer than itself.
    // A height-based bound was written here and reverted before shipping - exact for horizontal floors,
    // but it clips a wall shadow away entirely, and shadows do land on walls.
    half along = dot(receiver - caster_c.xyz, extrude_c.xyz);

    // THE BOUND IS A LINE, NOT A CONSTANT. A receiving surface is not perpendicular to the light, so
    // `along` varies across it - further along the shadow means a larger `along` even on flat ground.
    // A constant bound must therefore either cut the shadow short or be padded enough to leak, and
    // both were observed. That variation is a property of the receiver, so the bound tracks it:
    //
    //     bound(lateral) = along_at_caster + slope * lateral + margin
    //
    // `slope` arrives in spread_c.w, computed on the CPU from the receiver plane the reach trace
    // already returns. Exact for any SINGLE planar receiver at any orientation - floor, ramp or wall.
    // A ledge is three planes and one line still fits one of them; that is the documented ceiling in
    // docs/05, not something this bound can be tuned out of.
    half lateral = dot(receiver - caster_c.xyz, spread_c.xyz);
    half bound = cam_pos_c.w + spread_c.w * lateral;
    clip(bound - along);

    // Colour is irrelevant: the volume pass runs with COLORWRITEENABLE = 0 and exists only to move the
    // stencil counters. The clip() above is this shader's entire purpose.
    return float4(0.f, 0.f, 0.f, 0.f);
}
