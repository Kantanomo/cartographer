#pragma once
#include "render_layers.h"

/* classes */

class c_render_primitive
{
private:
	void* m_build_func;
	void* m_render_func;
	datum m_shader_index;
	datum m_particle_system_index;
	int32 field_10;
	bool m_field_14;
	int8 m_render_primitive_list_index;
	int8 m_pad[2];
};
ASSERT_STRUCT_SIZE(c_render_primitive, 24);

class c_render_primitive_list
{	
public:
	int32 m_max_primitive_count;
	int32 m_primitive_count;
	uint32 m_render_layer_flags;
	int32 m_field_C;
	uint32 m_render_layer_flags_backup;
	c_render_primitive* m_primitives;

	void backup_settings(void);
	void restore_backup(void);

	bool test_layer(e_render_layer layer_index) const;
};
ASSERT_STRUCT_SIZE(c_render_primitive_list, 24);

/* globals */

extern c_render_primitive_list g_render_primitives_by_primitive_list_type[3];

/* prototypes */

void __cdecl create_visible_render_primitives(int32 hologram_flag);

/* public code */

inline c_render_primitive_list* render_primitive_get_by_primitive_list_type(uint8 primitive_list_type)
{
	ASSERT(VALID_INDEX(primitive_list_type, NUMBEROF(g_render_primitives_by_primitive_list_type)));
	c_render_primitive_list* lists = Memory::GetAddress<c_render_primitive_list*>(0x4F4EC0);
	return &lists[primitive_list_type];
}
