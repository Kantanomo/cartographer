#include "stdafx.h"
#include "render_layers.h"

/* public code */

s_render_layer_globals* render_layer_globals_get(void)
{
	return Memory::GetAddress<s_render_layer_globals*>(0xA3E3E4);
}

bool __cdecl render_layer_begin(e_render_layer layer)
{
	return INVOKE(0x1A155C, 0x0, render_layer_begin, layer);
}

void __cdecl render_layer_draw(void)
{
	return INVOKE(0x1A176F, 0x0, render_layer_draw);
}

void __cdecl render_layer_end(void)
{
	return INVOKE(0x1A17D9, 0x0, render_layer_end);
}
