#pragma once

/* prototypes */

int32 __cdecl object_list_new(void);

void __cdecl object_list_add(int32 object_list_index, int32 object_index);

void object_lists_dispose_from_old_map(void);

void __cdecl object_list_gc(void);

int32 __cdecl object_list_get_first(int32 object_list_index, int32* reference_index);

int32 __cdecl object_list_get_next(int32 object_list_index, int32* reference_index);

void __cdecl object_list_add_reference(int32 object_list_index);

void __cdecl object_list_remove_reference(int32 object_list_index);

