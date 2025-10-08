#pragma once

/* prototypes */

// Checks if a debugger is present
bool is_debugger_present(void);

void display_debug_string(const char* format);

void system_get_date_and_time(char* string, int16 size, bool exclude_milliseconds);

void handle_fatal_error(int32 a1, const char* str);

// Center window from hwnd
void center_window(HWND hwnd);
