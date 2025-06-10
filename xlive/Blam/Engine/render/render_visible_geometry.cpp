#include "stdafx.h"
#include "render_visible_geometry.h"

/* prototypes */

static s_render_visible_globals* render_visible_globals_get(void);

/* public code */

uint32 render_section_visibility_get_model_group_count(void)
{
	return render_visible_globals_get()->model_group_count;
}

s_render_visible_section* render_visible_section_get(int16 array_index)
{
	s_render_visible_globals* g_render_visible_globals = render_visible_globals_get();
	ASSERT(VALID_INDEX(array_index, g_render_visible_globals->render_visible_section_count));
	return &g_render_visible_globals->render_visible_sections[array_index];
}

int8 render_visible_section_extern_shader_override_get(int16 array_index)
{
	s_render_visible_globals* g_render_visible_globals = render_visible_globals_get();
	ASSERT(VALID_INDEX(array_index, g_render_visible_globals->render_visible_section_count));
	return g_render_visible_globals->render_visible_section_extern_shader_override[array_index];
}

datum render_visible_section_shader_index_get(int16 array_index)
{
	s_render_visible_globals* g_render_visible_globals = render_visible_globals_get();
	ASSERT(VALID_INDEX(array_index, g_render_visible_globals->render_visible_section_count));
	const int8 shader_index_index = g_render_visible_globals->render_visible_section_shader_index_indices[array_index];

	datum result = NONE;

	if (shader_index_index != NONE)
	{
		ASSERT(VALID_INDEX(shader_index_index, g_render_visible_globals->section_extern_shader_count));
		result = g_render_visible_globals->render_visible_section_shader_index[shader_index_index];
	}

	return result;
}

/* private code */

static s_render_visible_globals* render_visible_globals_get(void)
{
	return Memory::GetAddress<s_render_visible_globals*>(0x4ECAB0);
}
