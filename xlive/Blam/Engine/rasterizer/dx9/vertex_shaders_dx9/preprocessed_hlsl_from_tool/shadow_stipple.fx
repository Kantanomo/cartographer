// shadow_stipple.fx - tag-debug stipple-density emulation for stencil volume draws.
// td stippled the volume rasterization at min(model fade alpha, global density) so fading
// objects cast dithered lighter shadows (shadows_model_section_draw, renderstate 0x53 +
// rasterizer_submit_stipple). DX9 has no stipple state; this ps_3_0 screen-door clip()
// kills the same fraction of volume fragments before they touch stencil.
//
//   c30.x = opacity (0..1); fragments below the quantised level are discarded.
//
// Color writes are disabled during volume draws - the shader exists only for the clip().
//
// The pattern is tag debug's exactly, and it is NOT the textbook Bayer matrix:
//   rasterizer_stipple_initialize (td 0xB01B0) builds 17 x 32x32 masks by replicating a 4x4
//   tile 8x8 times; for level L it sets the first L of 16 tile positions taken in order from
//   the (dx,dy) table at td 0x456780:
//     0:(0,0) 1:(2,2) 2:(0,2) 3:(2,0)  4:(1,1) 5:(3,3) 6:(3,1) 7:(1,3)
//     8:(0,1) 9:(2,3) 10:(3,0) 11:(1,2) 12:(1,0) 13:(3,2) 14:(0,3) 15:(2,1)
//   Inverting that to a threshold matrix gives the table below: T[y][x] is the level at which
//   that pixel turns on. Against standard Bayer, T[0][1]/T[1][0] and T[0][2]/T[2][0] are swapped
//   and T[0][3] is 10 where Bayer's transpose gives 15 - so do not substitute a stock matrix.
//
// rasterizer_get_stipple_pattern (td 0xB03C0) quantises to 17 levels:
//   level = clamp((int)(opacity * 16), 0, 16), and a pixel is drawn iff T[y][x] < level.
// The endpoints fall out of that rule without special-casing: opacity 1.0 -> level 16 > every
// T (all drawn, matching td disabling the stipple outright at alpha 0xFF), and opacity 0 ->
// level 0 <= every T (nothing drawn).

float4 stipple_c : register(c30);

float4 main(float2 vpos : VPOS) : COLOR
{
    static const float threshold[16] =
    {
         0.0, 12.0,  3.0, 10.0,
         8.0,  4.0, 15.0,  6.0,
         2.0, 11.0,  1.0, 13.0,
        14.0,  7.0,  9.0,  5.0
    };
    int2 cell = (int2)fmod(vpos, 4.0);
    float level = clamp(floor(stipple_c.x * 16.0), 0.0, 16.0);
    // keep when threshold < level; the -0.5 puts the cut between adjacent integers
    clip(level - threshold[cell.y * 4 + cell.x] - 0.5);
    return float4(0.0, 0.0, 0.0, 0.0);
}
