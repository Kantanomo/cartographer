#pragma once

/* constants */

// 1 hour offset
#define k_process_system_time_startup_offset_sec (1 * 60 * 60)

#define k_shell_time_sec_denominator 1
#define k_shell_time_msec_denominator 1000
#define k_shell_time_usec_denominator 1000000

/* prototypes */

HWND* shell_windows_get_hwnd(void);

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
