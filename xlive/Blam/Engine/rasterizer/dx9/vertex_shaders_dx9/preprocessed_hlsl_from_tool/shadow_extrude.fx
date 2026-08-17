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
    world_pos += extrude_dir * (va_extrude.x * extrude_c.x);

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
