#include "stdafx.h"
#include "object_lists.h"

#include "memory/data.h"

/* prototypes */

static data_array* object_list_header_data_get(void);

static data_array* object_list_data_get(void);

/* public code */

int32 __cdecl object_list_new(void)
{
	return INVOKE(0x7AA29, 0x84481, object_list_new);
}

void __cdecl object_list_add(int32 object_list_index, int32 object_index)
{
	INVOKE(0x7A8A3, 0x842FB, object_list_add, object_list_index, object_index);
	return;
}

void object_lists_dispose_from_old_map(void)
{
	data_make_invalid(object_list_header_data_get());
	data_make_invalid(object_list_data_get());
	return;
}

void __cdecl object_list_gc(void)
{
	INVOKE(0x7A993, 0x843EB, object_list_gc);
	return;
}

int32 __cdecl object_list_get_first(int32 object_list_index, int32* reference_index)
{
	return INVOKE(0x7AA7A, 0x0, object_list_get_next, object_list_index, reference_index);
}

int32 __cdecl object_list_get_next(int32 object_list_index, int32* reference_index)
{
	return INVOKE(0x7A93C, 0x0, object_list_get_next, object_list_index, reference_index);
}

void __cdecl object_list_add_reference(int32 object_list_index)
{
	INVOKE(0x7A94B, 0x0, object_list_add_reference, object_list_index);
	return;
}

void __cdecl object_list_remove_reference(int32 object_list_index)
{
	INVOKE(0x7A96F, 0x0, object_list_remove_reference, object_list_index);
	return;
}

/* private code */

static data_array* object_list_header_data_get(void)
{
	return *Memory::GetAddress<data_array**>(0x4C06E0, 0x4F3CE0);
}

static data_array* object_list_data_get(void)
{
	return *Memory::GetAddress<data_array**>(0x4C06DC, 0x4F3CDC);
}
