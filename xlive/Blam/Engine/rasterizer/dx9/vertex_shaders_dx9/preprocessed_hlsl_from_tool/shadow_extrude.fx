// shadow_extrude.fx — Cartographer stencil shadow volume extrusion (ISQ/DSQ port).
// Not a tool-dumped shader: the Xbox volume shaders (codes 141-146) never existed on Vista.
//
// ENGINE-CONVENTION TRANSFORMS (td-vista-render-design-map.md section 2): like the tag-debug
// volume shaders, this inherits the model framework's constants instead of a private composite:
//   c0-c3   : world->clip dp4 rows — INHERITED from the engine (never set by our code; the
//             scene layers keep them current per window/pass via 0x661a0d)
//   c50-c52 : model->world dp4 rows — set per object through the engine convention
//             (row r = forward[r], left[r], up[r], position[r])
//   c254    : WORLD-space light: point -> (position.xyz, 1), directional -> (toward_light.xyz, 0)
//   c255.x  : extrusion distance
//
// Vertex layout (s_stencil_shadow_vertex): POSITION0 = float3 position (section/model space),
// TEXCOORD0 = float1 extrude flag (0 = anchor vertex, 1 = extruded vertex).

float4 wvp[4] : register(c0);
float4 node[3] : register(c50);
float4 light_c : register(c254);
float4 extrude_c : register(c255);

// it. 534 — PER-VERTEX clip plane (xyz = extrude direction, w = plane distance + margin).
//
// Zero xyz = disabled, and every existing mode leaves it zero, so the stock path is untouched.
//
// Why this exists: with ONE extrusion scalar per caster the volume must be long enough to bury the
// FURTHEST vertex's cap, so the NEAREST vertex overshoots by the caster's own extent along the light
// (~0.71 wu on a biped). Penetration was therefore `caster_size + margin` and shrinking the margin
// could not help. Giving each vertex its own distance to the receiver plane makes penetration the
// MARGIN ALONE, independent of caster size.
//
// The plane is perpendicular to the light through the clip hit point: exact for a floor under this
// light, and a good approximation on slopes. One CPU ray supplies it; no per-vertex tracing.
float4 clip_c : register(c253);

struct VS_OUTPUT
{
    float4 oPos : POSITION;
};

VS_OUTPUT main(
    float4 va_position : POSITION0,
    float4 va_extrude : TEXCOORD0)
{
    VS_OUTPUT output;

    float4 model_pos = float4(va_position.xyz, 1.0f);
    float3 world_pos;
    world_pos.x = dot(model_pos, node[0]);
    world_pos.y = dot(model_pos, node[1]);
    world_pos.z = dot(model_pos, node[2]);

    float3 extrude_dir = normalize(world_pos * light_c.w - light_c.xyz);

    // it. 534: per-vertex distance to the clip plane when one is supplied, else the flat constant.
    // `max(0, ...)` matters — a vertex already past the plane must not extrude BACKWARDS toward the
    // light, which would invert its silhouette contribution.
    // it. 539: the floor is extrude_c.y, NOT zero. A vertex already past the clip plane used to get
    // amount = 0, which places its FAR-cap triangle exactly on top of its NEAR-cap triangle. The two
    // coincide, their stencil contributions cancel, and that part of the caster loses its shadow —
    // user-observed as pieces of the shadow going missing under clipping, and absent at both 2.0 and
    // 500 where no vertex is ever clamped. Every vertex must extrude far enough to keep the two caps
    // apart; extrude_c.y carries that minimum (0 in the non-clipped modes, which never reach here).
    float amount = extrude_c.x;
    if (dot(clip_c.xyz, clip_c.xyz) > 0.5f)
    {
        amount = max(extrude_c.y, clip_c.w - dot(clip_c.xyz, world_pos));
    }
    world_pos += extrude_dir * (va_extrude.x * amount);

    float4 pos = float4(world_pos, 1.0f);
    output.oPos.x = dot(pos, wvp[0]);
    output.oPos.y = dot(pos, wvp[1]);
    output.oPos.z = dot(pos, wvp[2]);
    output.oPos.w = dot(pos, wvp[3]);

    // clamp EXTRUDED vertices onto the far plane: far-clip must never cut the volume's side
    // sheets open. Homogeneous-sign robust: far plane is z/w = 1, so pull z toward 0.9999*w
    // from the far side whichever sign w has.
    float far_z = 0.9999f * output.oPos.w;
    float z_clamped = (output.oPos.w >= 0.0f)
        ? min(output.oPos.z, far_z)
        : max(output.oPos.z, far_z);
    output.oPos.z = lerp(output.oPos.z, z_clamped, va_extrude.x);
    return output;
}
