#pragma once

/* constants */

enum
{
	k_system_timer_resolution_ms = 1,
};

/* globals */

extern bool display_framerate;
extern bool display_frame_deltas;
extern bool debug_disable_frame_rate_throttle;


extern bool g_main_game_time_frame_limiter_enabled;

/* prototypes */

void main_time_apply_patches(void);

void __cdecl main_time_initialize(void);

void __cdecl main_time_reset(void);

int32 main_time_maximum_ticks_per_frame(void);

bool main_time_is_throttled(void);

int32 main_time_throttle(void);

void main_time_continue(void);

void main_time_advance(int32 milliseconds);

real32 main_time_update(void);

bool main_time_halted(void);

bool __cdecl main_time_should_reset(void);

int32 __cdecl main_time_get_tickrate(void);
