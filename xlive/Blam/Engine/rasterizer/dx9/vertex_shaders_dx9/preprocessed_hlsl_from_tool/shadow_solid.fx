// shadow_solid.fx - constant-color pixel shader for the stencil shadow debug tint.
// c31 (highest ps_2_0 constant) so engine pixel shader constants are never clobbered.

float4 solid_color : register(c31);

float4 main() : COLOR
{
    return solid_color;
}
