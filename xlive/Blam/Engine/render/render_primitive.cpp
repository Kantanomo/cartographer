#include "stdafx.h"
#include "render_primitive.h"

/* globals */

c_render_primitive_list g_render_primitives_by_primitive_list_type[3];	// Only used for a assert for now (DO NOT USE this until we "own" this global)

/* public code */

void c_render_primitive_list::backup_settings(void)
{
	m_render_layer_flags_backup = m_render_layer_flags;
	m_render_layer_flags = 0;
	m_field_C = m_primitive_count;
	return;
}

void c_render_primitive_list::restore_backup(void)
{
	m_render_layer_flags = m_render_layer_flags_backup;
	m_field_C = 0;
	return;
}

bool c_render_primitive_list::test_layer(e_render_layer layer_index) const
{
	ASSERT(VALID_INDEX(layer_index, k_number_of_render_layers));
	return !TEST_BIT(m_render_layer_flags, layer_index);
}

void __cdecl create_visible_render_primitives(int32 hologram_flag)
{
	INVOKE(0x19BDBC, 0x0, create_visible_render_primitives, hologram_flag);
	return;
}
