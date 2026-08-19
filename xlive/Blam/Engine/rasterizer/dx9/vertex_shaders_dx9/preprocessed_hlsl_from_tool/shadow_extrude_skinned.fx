// shadow_extrude_skinned.fx — Cartographer stencil shadow volume extrusion, GPU-SKINNED (it. 660).
// Not a tool-dumped shader; the companion of shadow_extrude.fx for articulated sections.
//
// THE ONLY DIFFERENCE FROM shadow_extrude.fx is how world_pos is produced: instead of one node
// matrix at c50-c52, a 4-bone blend against a palette of 3-row matrices starting at c50 — the same
// blend, register base and row convention as Vista's own shadow_buffer_generation_skinned_4_bone
// (which indexes `c[a0.x + 50/51/52]` in vs_2_0). Everything from `extrude_dir` down is copied
// verbatim from shadow_extrude.fx; see the derivations there (self-shadow bias it. 615/616,
// far-plane clamp) — they are deliberately not re-explained here.
//
// Divergences from the reference blend, both baked at VB build time (it. 660):
//   * indices arrive PRE-MULTIPLIED BY 3 (the reference multiplies by c[7].w at run time; we do not
//     touch c7, which belongs to the engine). ubyte4 payload, so local slot <= 84 fits; the builder
//     guards node_map_count <= 67 so c50 + 3N + 2 stays inside the vs_2_0 constant file.
//   * all four weights are STORED (ubyte4n, summing 1.0); the reference derives w0 = 1 - (rest)
//     from c[4].w. Storing it costs nothing in a 4-byte lane we pad anyway and drops the c4 read.
//
// Constants:
//   c0-c3        : world->clip dp4 rows — INHERITED from the engine
//   c50..c50+3N  : palette — 3 rows per matrix, row r = {f[r]*s, l[r]*s, u[r]*s, pos[r]},
//                  model->world with the inverse bind already composed (the skinning pool's own
//                  packing, render_model_build_skinning_for_base_nodes 0x77DD88)
//   c254         : WORLD-space light: point -> (position.xyz, 1), directional -> (toward_light.xyz, 0)
//   c255.x       : extrusion distance;  c255.y : self-shadow bias
//
// Vertex layout (s_stencil_shadow_skinned_vertex, 24 bytes):
//   POSITION0      float3  bind-pose position (section space)
//   TEXCOORD0      float1  extrude flag (0 = anchor, 1 = extruded)
//   BLENDINDICES0  ubyte4  palette row offsets: local_slot * 3 (unused lanes 0)
//   BLENDWEIGHT0   ubyte4n weights summing 1.0 (unused lanes 0)

float4 wvp[4] : register(c0);
float4 palette[204] : register(c50);	// 68 matrices x 3 rows — the full c50..c253 window
float4 light_c : register(c254);
float4 extrude_c : register(c255);

struct VS_OUTPUT
{
    float4 oPos : POSITION;
};

VS_OUTPUT main(
    float4 va_position : POSITION0,
    float4 va_extrude : TEXCOORD0,
    float4 va_indices : BLENDINDICES0,
    float4 va_weights : BLENDWEIGHT0)
{
    VS_OUTPUT output;

    float4 model_pos = float4(va_position.xyz, 1.0f);

    // 4-bone blend of the three matrix rows — the reference shader's r4/r5/r6 accumulation, with
    // the index scale and the derived weight baked out (see the header). UBYTE4 input arrives as
    // raw 0..255 floats — exact integers, so the cast (which compiles to mova) is lossless.
    int4 a0 = (int4)va_indices;

    float4 row_x = va_weights.x * palette[a0.x + 0];
    float4 row_y = va_weights.x * palette[a0.x + 1];
    float4 row_z = va_weights.x * palette[a0.x + 2];

    row_x += va_weights.y * palette[a0.y + 0];
    row_y += va_weights.y * palette[a0.y + 1];
    row_z += va_weights.y * palette[a0.y + 2];

    row_x += va_weights.z * palette[a0.z + 0];
    row_y += va_weights.z * palette[a0.z + 1];
    row_z += va_weights.z * palette[a0.z + 2];

    row_x += va_weights.w * palette[a0.w + 0];
    row_y += va_weights.w * palette[a0.w + 1];
    row_z += va_weights.w * palette[a0.w + 2];

    float3 world_pos;
    world_pos.x = dot(model_pos, row_x);
    world_pos.y = dot(model_pos, row_y);
    world_pos.z = dot(model_pos, row_z);

    // ---- identical to shadow_extrude.fx from here down ------------------------------------
    float3 extrude_dir = normalize(world_pos * light_c.w - light_c.xyz);

    world_pos += extrude_dir * extrude_c.y;
    world_pos += extrude_dir * (va_extrude.x * extrude_c.x);

    float4 pos = float4(world_pos, 1.0f);
    output.oPos.x = dot(pos, wvp[0]);
    output.oPos.y = dot(pos, wvp[1]);
    output.oPos.z = dot(pos, wvp[2]);
    output.oPos.w = dot(pos, wvp[3]);

    float far_z = 0.9999f * output.oPos.w;
    float z_clamped = (output.oPos.w >= 0.0f)
        ? min(output.oPos.z, far_z)
        : max(output.oPos.z, far_z);
    output.oPos.z = lerp(output.oPos.z, z_clamped, va_extrude.x);
    return output;
}
