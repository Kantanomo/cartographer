#include "stdafx.h"
#include "main_time.h"

#include "console.h"
#include "main.h"
#include "main_screenshot.h"

#include "game/game.h"
#include "game/game_time.h"
#include "game/player_control.h"
#include "rasterizer/rasterizer_globals.h"
#include "shell/shell.h"
#include "shell/shell_windows.h"

#include "H2MOD/Modules/Shell/Config.h"

/* constants */

enum
{
	k_maximum_ticks_per_frame = 8,
};

#define k_use_precise_counters true

//#define MAIN_TIME_DEBUG

/* typedefs */

typedef void(__cdecl* t_main_time_reset)(void);

/* structures */

struct s_main_time_globals
{
	bool wmv_playback_in_progress;
	bool wmv_playback_throttle_framerate;
	bool temporary_throttle_control;
	bool temporary_throttle;
	int32 last_game_time;
	uint64 last_milliseconds;
	bool should_reset;
	int64 last_vblank_index;
	int64 last_initial_vblank_index;
	int64 target_display_vblank_index;
	int64 last_input_timestamp;
};
ASSERT_STRUCT_SIZE(s_main_time_globals, 56);

#ifdef MAIN_TIME_DEBUG
struct s_main_time_debug
{
	real32 dt_default;
	real32 dt_performance_counter;
};
#endif

/* prototypes */

static s_main_time_globals* main_time_globals_get(void);

static real32 main_time_get_delta_sec_precise(LARGE_INTEGER counter_now, LARGE_INTEGER freq);

static uint64 __cdecl main_time_get_absolute_milliseconds(void);

static real32 main_time_delta_calculate(LARGE_INTEGER counter_now, LARGE_INTEGER freq);

/* globals */

static real32 last_framerate_time = 0.f;

static LARGE_INTEGER g_main_game_time_counter_last_time;
#ifdef MAIN_TIME_DEBUG
static s_main_time_debug g_main_game_time_debug;
#endif

static t_main_time_reset p_main_time_reset;


bool display_framerate = false;
bool display_frame_deltas = false;
bool debug_disable_frame_rate_throttle = false;

bool g_main_game_time_frame_limiter_enabled = false;

/* public code */

void main_time_apply_patches(void)
{
	PatchCall(Memory::GetAddress(0x39BE3, 0xC03E), main_time_update);

	DETOUR_ATTACH(p_main_time_reset, Memory::GetAddress<t_main_time_reset>(0x286C5, 0x24867), main_time_reset);
	return;
}

void __cdecl main_time_initialize(void)
{
	s_main_time_globals* main_time_globals = main_time_globals_get();
	main_time_globals->last_milliseconds = system_milliseconds();
	main_time_globals->should_reset = false;
	main_time_globals->last_vblank_index = 0;
	main_time_globals->temporary_throttle_control = 0;

	// windows 10 version 2004 and above added behaviour changes to how windows timer resolution works, and we have to explicitly set the time resolution
	// and since they were added, when playing on a laptop on battery it migth add heavy stuttering when using a frame limiter based on Sleep function (or std::this_thread::sleep_for) implementation
	// the game sets them already but only during the loading screen period, then it resets to system default when the loading screen ends 
	// (tho i think in the new implementation is working on a per thread basis now instead of global frequency, since it still works even when the game resets after loading screen ends and loading screen runs in another thread)

	// More details @ https://randomascii.wordpress.com/2020/10/04/windows-timer-resolution-the-great-rule-change/

	timeBeginPeriod(k_system_timer_resolution_ms);
	return;
}

void __cdecl main_time_reset(void)
{
	s_main_time_globals* main_time_globals = main_time_globals_get();
	main_time_globals->last_milliseconds = system_milliseconds();
	main_time_globals->should_reset = true;
	g_main_game_time_counter_last_time = shell_time_counter_now(NULL);
	return;
}

int32 main_time_maximum_ticks_per_frame(void)
{
	return k_maximum_ticks_per_frame;
}

bool main_time_is_throttled(void)
{
	const s_main_time_globals* main_time_globals = main_time_globals_get();

	bool result = true;
	if (!main_time_globals->temporary_throttle_control)
	{
		const s_window_globals* window_globals = window_globals_get();
		s_rasterizer_globals* rasterizer_globals = rasterizer_globals_get();

		result =
			rasterizer_globals->clipping_parameters.parameters.field_0
			&& game_in_progress()
			&& !debug_disable_frame_rate_throttle
			&& main_time_globals->wmv_playback_in_progress
			&& main_time_globals->wmv_playback_throttle_framerate
			|| IsIconic(window_globals->hWnd) && !shell_is_dedicated_server();
	}

	return result;
}

int32 main_time_throttle(void)
{
	return INVOKE(0x287A1, 0x0, main_time_throttle);
}

void main_time_continue(void)
{
	s_main_time_globals* main_time_globals = main_time_globals_get();
	main_time_globals->should_reset = false;
	return;
}

void main_time_advance(int32 milliseconds)
{
	s_main_time_globals* main_time_globals = main_time_globals_get();
	main_time_globals->last_milliseconds += milliseconds;
	return;
}

real32 __cdecl main_time_update(void)
{
	//return INVOKE(0x28814, 0x0, main_time_update, fixed_time_step, fixed_time_delta);
	
	real32 dt_sec = 0.f;
	const int32 game_time = game_in_progress() ? game_time_get() : 0;
	const LARGE_INTEGER freq = shell_time_counter_freq();

	shell_update();
	
	dt_sec = main_time_delta_calculate(shell_time_counter_now(NULL), freq);

	// don't run the frame limiter when time step is fixed, because the code doesn't support it
	// in case of fixed time step, frame limiter should be handled by the other frame limiter
	if (main_time_is_throttled())
	{
		if (game_time_initialized())
		{
			// if there's game tick leftover time (i.e the actual game tick update executed faster than the actual engine's fixed time step)
			// FIXED by interpolation: 
			// limit the framerate to get back in sync with the renderer to prevent ghosting and jagged movement
			while (dt_sec < game_time_get_max_frame_time())
			{
				uint32 yield_time_msec = 0;
				real32 fMsSleep = (real32)(game_time_get_max_frame_time() - dt_sec) * 1000.f;

				if (fMsSleep >= 2.0f + k_real_epsilon)
				{
					yield_time_msec = (int32)fMsSleep;

					// TODO FIXME to reduce stuttering, spend some of the time to sleep by CPU spinning,
					// Sleep is not precise since Windows is not a RTOS
					Sleep(yield_time_msec);
				}

				dt_sec = main_time_delta_calculate(shell_time_counter_now(NULL), freq);
			}
		}
		else
		{
			Sleep(15u);
			dt_sec = main_time_delta_calculate(shell_time_counter_now(NULL), freq);
		}
	}
	else
	{
		shell_windows_throttle_framerate(H2Config_fps_limit);
		dt_sec = main_time_delta_calculate(shell_time_counter_now(NULL), freq);
	}
	

	s_main_time_globals* main_time_globals = main_time_globals_get();

#ifdef MAIN_TIME_DEBUG
	if (k_use_precise_counters)
	{
		const uint64 time_now_msec = main_time_get_absolute_milliseconds();
		g_main_game_time_debug.dt_default = (real32)((real32)(time_now_msec - main_time_globals->last_milliseconds) / 1000.f);
		g_main_game_time_debug.dt_performance_counter = dt_sec;
	}
	else
	{
		g_main_game_time_debug.dt_default = dt_sec;
		g_main_game_time_debug.dt_performance_counter = main_time_get_delta_sec_precise(shell_time_counter_now(NULL), freq);
	}
#endif

	g_main_game_time_counter_last_time = shell_time_counter_now(NULL);

	dt_sec = movie_recording() ? movie_recording_timestep() : dt_sec;

	dt_sec = PIN(dt_sec, 0.f, 10.f);
	
	last_framerate_time = dt_sec;
	main_time_globals->last_milliseconds = main_time_get_absolute_milliseconds();
	main_time_globals->last_game_time = game_time;
	main_time_globals->last_vblank_index = *Memory::GetAddress<int64*>(0xA3E440, 0x4905C8);
	main_time_globals->last_initial_vblank_index = *Memory::GetAddress<int64*>(0xA3E440, 0x4905C8);
	return dt_sec;
}

bool main_time_halted(void)
{
	bool result = shell_application_is_paused();
	/*
#if TERMINAL_ENABLED
	if (debug_console_pauses_game
		&& console_is_active()
		&& (!game_in_progress() || !game_is_networked() || game_is_playback()))
	{
		result = true;
	}
#endif*/
	return result;
}

bool __cdecl main_time_should_reset(void)
{
	return INVOKE(0x286E1, 0x0, main_time_should_reset);
}

int32 __cdecl main_time_get_tickrate(void)
{
	return INVOKE(0x28707, 0x2489B, main_time_get_tickrate);
}

/* private code */

static s_main_time_globals* main_time_globals_get(void)
{
	return Memory::GetAddress<s_main_time_globals*>(0x479E90, 0x4A2980);
}

static real32 main_time_get_delta_sec_precise(LARGE_INTEGER counter_now, LARGE_INTEGER freq)
{
	real32 result = (real32)(((real64)shell_time_counter_diff(counter_now, g_main_game_time_counter_last_time).QuadPart) / (real64)freq.QuadPart);
	return result;
}

static uint64 __cdecl main_time_get_absolute_milliseconds(void)
{
	uint64 milliseconds = system_milliseconds();
	s_main_time_globals* main_time_globals = main_time_globals_get();

	// copy the high part
	milliseconds |= (uint64)(main_time_globals->last_milliseconds >> 32) << 32;

	if (milliseconds < main_time_globals->last_milliseconds)
	{
		// adjust the time, a complete cycle of system_milliseconds has passed
		milliseconds += UINT_MAX + 1ULL;
	}

	ASSERT(milliseconds >= main_time_globals->last_milliseconds);
	return milliseconds;
}

static real32 main_time_delta_calculate(LARGE_INTEGER counter_now, LARGE_INTEGER freq)
{
	const s_main_time_globals* main_time_globals = main_time_globals_get();

	real32 dt;
	// We want the return value to be configurable
	if (k_use_precise_counters)
	{
		// Precise dt
		dt = main_time_get_delta_sec_precise(shell_time_counter_now(NULL), freq);
	}
	else
	{
		// Imprecise dt (original logic)
		uint64 time_now_msec = main_time_get_absolute_milliseconds();
		dt = (real32)((real32)(time_now_msec - main_time_globals->last_milliseconds) / 1000.f);
	}

	return dt;
}
