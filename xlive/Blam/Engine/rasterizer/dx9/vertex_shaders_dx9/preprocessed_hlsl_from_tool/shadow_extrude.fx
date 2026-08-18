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

    // it. 605: the per-vertex CLIP PLANE path (it. 534/539, c253) is REMOVED with the CLIPPED mode.
    //
    // It read `clip_c` and, when non-zero, gave each vertex its own extrusion distance. Nothing writes
    // that constant any more, and leaving the branch would be actively dangerous: c253 is a shared
    // register, so a value left there by any other engine shader would silently re-enable clipping with
    // garbage. The branch and the upload had to go together, and the branch first.
    //
    // Reach-clip supersedes it — it bounds the shadow per PIXEL in shadow_reach_clip.fx instead of
    // shortening the extrusion per vertex, so the volume stays infinite and no far cap enters the scene.
    // See td-do-not-fix.md entry 15.
    // it. 615 — SELF-SHADOW BIAS, extrude_c.y.
    //
    // The NEAR CAP is the caster's light-facing triangles at their ORIGINAL positions, so it is exactly
    // coplanar with the object's own rendered surface. With ZFUNC = LESS an equal-depth fragment FAILS,
    // and z-fail counts on failure — so whether the near cap contributes at a given pixel is decided by
    // depth quantisation. As the object moves, pixels flip between "slightly nearer" (passes, no count)
    // and "equal" (fails, counts), the near and far contributions stop cancelling, and the caster
    // partially shadows itself in a pattern that flickers. User-reported at the top of objects, where the
    // extrusion begins — exactly where the cap meets the lit surface.
    //
    // it. 616 — THE BIAS GOES AWAY FROM THE LIGHT. it. 615 pushed it TOWARD the light and was WRONG;
    // the user saw the flicker replaced by a constant shell of self-shadow. Deriving it properly:
    //
    // For a pixel on the caster's own LIT surface at depth D, z-fail counts fragments BEHIND it.
    //   * far cap  (extruded, back-facing)  : depth >> D  -> fails -> COUNTS
    //   * near cap (un-extruded, front-facing) : depth ~ D
    // The two have opposite winding, so the net cancels ONLY IF THE NEAR CAP ALSO COUNTS — which
    // requires it to be BEHIND the surface. Coplanar is the ambiguous case, and that ambiguity is the
    // flicker.
    //
    // it. 615 moved the near cap IN FRONT, so it passed the depth test and stopped counting, leaving the
    // far cap's count uncancelled -> every lit pixel of the caster shadowed, constantly. The observation
    // matched the error exactly.
    //
    // So push the volume AWAY from the light (`+`, extrude_dir's own direction): the near cap is then
    // unambiguously behind the surface, always counts, and always cancels.
    //
    // Applied to EVERY vertex, not just anchors, so the volume translates rigidly rather than stretching
    // — the side quads keep meeting the caps exactly, which the stencil counting depends on.
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
