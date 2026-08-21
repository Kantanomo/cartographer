// shadow_extrude.fx - Cartographer stencil shadow volume extrusion (ISQ/DSQ port). Not a tool-dumped
// shader: the Xbox volume shaders never existed on Vista.
//
// ENGINE-CONVENTION TRANSFORMS. Like the tag-debug volume shaders, this inherits the model
// framework's constants instead of building a private composite:
//   c0-c3   : world->clip dp4 rows - INHERITED from the engine, never set by our code; the scene
//             layers keep them current per window and pass
//   c50-c52 : model->world dp4 rows - set per object through the engine's own upload
//   c254    : WORLD-space light: point -> (position.xyz, 1), directional -> (toward_light.xyz, 0)
//   c255    : .x extrusion distance, .y self-shadow bias
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

    // SELF-SHADOW BIAS, extrude_c.y, AND IT GOES AWAY FROM THE LIGHT.
    //
    // The near cap is the caster's light-facing triangles at their ORIGINAL positions, so it is exactly
    // coplanar with the object's own rendered surface. With ZFUNC = LESS an equal-depth fragment fails,
    // and z-fail counts on failure, so whether the near cap contributes at a given pixel is decided by
    // depth quantisation: as the object moves, pixels flip between "slightly nearer" (passes, no count)
    // and "equal" (fails, counts), the near and far contributions stop cancelling, and the caster
    // partially shadows itself in a flickering pattern.
    //
    // The sign follows from the counting. For a pixel on the caster's lit surface at depth D, z-fail
    // counts fragments behind it: the extruded far cap is far behind, fails, and counts, and the two
    // caps have opposite winding, so the net cancels ONLY IF THE NEAR CAP ALSO COUNTS - which requires
    // it to be BEHIND the surface. Biasing toward the light puts it in front instead, where it passes
    // and stops counting, leaving the far cap uncancelled and every lit pixel of the caster shadowed.
    //
    // Applied to EVERY vertex, not just anchors, so the volume translates rigidly rather than
    // stretching - the side quads keep meeting the caps exactly, which the counting depends on.
    world_pos += extrude_dir * extrude_c.y;
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
