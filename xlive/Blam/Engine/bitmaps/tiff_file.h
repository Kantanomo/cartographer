#pragma once

/* prototypes */

const char* __cdecl tiff_export_begin_tiled(struct s_file_reference* file_reference, int32 a2, int32 a3, int32 a4, int32 a5, void** a6);

int32 __cdecl tiff_export_raw(struct s_file_reference* file, struct bitmap_data* bitmap);

int32 __cdecl tiff_export_add_tile_from_surface(void* handle, const struct bitmap_data* bitmap, int32 left, int32 top);

int32 __cdecl tiff_export_end_tiled(void* data);
