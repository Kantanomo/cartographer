#pragma once

/* prototypes */

void cseries_windows_tool_apply_patches(void);

// Checks if a debugger is present
bool is_debugger_present(void);

void display_debug_string(const char* format);

void system_get_date_and_time(char* string, int16 size, bool exclude_milliseconds);

void handle_fatal_error(int32 code, const char* str);

void __cdecl system_get_user_name(char* name, int16 size);

// Center window from hwnd
void center_window(HWND hwnd);
