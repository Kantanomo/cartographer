#pragma once

/* constants */

enum
{
	k_process_system_time_startup_offset_sec = (1 * 60 * 60),	// 1 hour offset
	k_shell_time_sec_denominator = 1,
	k_shell_time_msec_denominator = 1000,
	k_shell_time_usec_denominator = 1000000
};

/* globals */

extern uint32 g_instance_number;

extern int32 g_cmd_show;

extern WNDPROC g_wndproc_procedure;

extern wchar_t g_window_classname[64];

extern wchar_t g_window_name[64];

extern wchar_t g_shell_windows_instance_name[13];

/* prototypes */

HWND* shell_windows_get_hwnd(void);

bool shell_platform_initialize(void);

bool* should_initilize_xlive_get(void);

bool* xlive_initilized_get(void);

int32* fatal_error_id_get(void);

void shell_windows_apply_patches();

void shell_windows_initialize();

bool __cdecl game_is_minimized(void);

uint32 __cdecl system_milliseconds(void);

LARGE_INTEGER shell_time_counter_freq();

LARGE_INTEGER shell_time_counter_now(LARGE_INTEGER* freq);

LARGE_INTEGER shell_time_counter_diff(LARGE_INTEGER c1, LARGE_INTEGER c2);

unsigned long long shell_time_now_sec();

unsigned long long shell_time_now_msec();

unsigned long long shell_time_now(unsigned long long denominator);

void shell_windows_throttle_framerate(int desired_framerate);

bool __cdecl gfwl_gamestore_initialize(void);

uint32 shell_windows_get_monitor_index(void);
