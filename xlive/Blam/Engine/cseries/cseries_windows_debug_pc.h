#pragma once

void cseries_windows_debug_initialize(void);

// Get nt headers from pe_module
PIMAGE_NT_HEADERS get_nt_headers(void* pe_module);

uintptr_t devirtualize_address(void* pe_module, uintptr_t virtual_address, uint32 base_address);

