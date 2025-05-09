#include "stdafx.h"
#include "main_game_time.h"

#include "game/game.h"
#include "game/game_time.h"
#include "game/player_control.h"
#include "math/math.h"
#include "shell/shell.h"
#include "shell/shell_windows.h"

/* constants */

#define USE_PRECISE_COUNTERS 1


/* typedefs */

typedef void(__cdecl* t_main_time_reset)();

/* globals */

t_main_time_reset p_main_time_reset;

bool g_main_game_time_frame_limiter_enabled = false;
LARGE_INTEGER g_main_game_time_counter_last_time;

#ifdef MAIN_GAME_TIME_DEBUG
s_main_time_debug g_main_game_time_debug;
#endif

/* prototypes */

static real32 main_time_get_max_frame_time(void);

static real32 main_time_get_delta_sec_precise(LARGE_INTEGER counter_now, LARGE_INTEGER freq);

static real32 main_time_delta_calculate(LARGE_INTEGER counter_now, LARGE_INTEGER freq);

static uint64 __cdecl main_time_get_absolute_milliseconds(void);

static void __cdecl main_game_time_initialize_hook(void);

/* public code */

void main_game_time_apply_patches(void)
{
	PatchCall(Memory::GetAddress(0x39BE3, 0xC03E), main_time_update);
	PatchCall(Memory::GetAddress(0x39E3D, 0xBA40), main_game_time_initialize_hook);

	DETOUR_ATTACH(p_main_time_reset, Memory::GetAddress<t_main_time_reset>(0x286C5, 0x24867), main_time_reset);

	if (!shell_is_dedicated_server())
	{
		PatchCall(Memory::GetAddress(0x39C0D), main_time_update);
	}
	return;
}

void __cdecl main_time_reset(void)
{
	s_main_time_globals* main_time_globals = s_main_time_globals::get();
	main_time_globals->last_milliseconds = system_milliseconds();
	g_main_game_time_counter_last_time = shell_time_counter_now(NULL);
	// main_time_globals->should_reset = true;
	return;
}

real32 __cdecl main_time_update(bool fixed_time_step, real32 fixed_time_delta)
{
	//return INVOKE(0x28814, 0x0, fixed_time_step, fixed_time_delta);
	
	real32 dt_sec = 0.0f;
	LARGE_INTEGER freq = shell_time_counter_freq();
	int32 game_time = game_in_progress() ? game_time_get() : 0;

	// TranslateMessage()
	shell_update();

	if (fixed_time_step)
	{
		dt_sec = fixed_time_delta;
		if (time_globals::available())
			time_globals::get()->game_ticks_leftover = 0.0f;
	}
	else
	{
		dt_sec = main_time_delta_calculate(shell_time_counter_now(NULL), freq);

		// don't run the frame limiter when time step is fixed, because the code doesn't support it
		// in case of fixed time step, frame limiter should be handled by the other frame limiter
		if (g_main_game_time_frame_limiter_enabled || game_is_minimized())
		{
			if (time_globals::available())
			{
				// if there's game tick leftover time (i.e the actual game tick update executed faster than the actual engine's fixed time step)
				// FIXED by interpolation: 
				// limit the framerate to get back in sync with the renderer to prevent ghosting and jagged movement
				const real32 desired_frame_time = main_time_get_max_frame_time();
				while (dt_sec < desired_frame_time)
				{
					uint32 yield_time_msec = 0;
					real32 fMsSleep = (real32)(desired_frame_time - dt_sec) * 1000.f;

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
	}

	s_main_time_globals* main_time_globals = s_main_time_globals::get();

#ifdef MAIN_GAME_TIME_DEBUG
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

	dt_sec = PIN(dt_sec, 0.0f, 10.f);
	main_time_globals->last_milliseconds = main_time_get_absolute_milliseconds();
	main_time_globals->last_game_time = game_time;
	main_time_globals->field_16[0] = *Memory::GetAddress<__int64*>(0xA3E440);
	main_time_globals->field_16[1] = *Memory::GetAddress<__int64*>(0xA3E440);

	player_control_update_dt(dt_sec);

	//LOG_TRACE_GAME("main_time_update() - timeDeltaSeconds: {}", timeDeltaSeconds);
	return dt_sec;
}

/* private code */

static real32 main_time_get_max_frame_time(void)
{
	time_globals* time_globals = time_globals::get();
	real32 result = time_globals->tick_length - (real32)(time_globals->game_ticks_leftover / (real32)time_globals->ticks_per_second);
	return MAX(result, 0.0f);
}

static real32 main_time_get_delta_sec_precise(LARGE_INTEGER counter_now, LARGE_INTEGER freq)
{
	real32 result = (real32)(((real64)shell_time_counter_diff(counter_now, g_main_game_time_counter_last_time).QuadPart) / (real64)freq.QuadPart);
	return result;
}

static real32 main_time_delta_calculate(LARGE_INTEGER counter_now, LARGE_INTEGER freq)
{
	const s_main_time_globals* main_time_globals = s_main_time_globals::get();

	real32 dt;
	// We want the return value to be configurable
	if (USE_PRECISE_COUNTERS)
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

static uint64 __cdecl main_time_get_absolute_milliseconds(void)
{
	uint64 milliseconds = system_milliseconds();
	s_main_time_globals* main_time_globals = s_main_time_globals::get();

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

static void __cdecl main_game_time_initialize_hook(void)
{
	// windows 10 version 2004 and above added behaviour changes to how windows timer resolution works, and we have to explicitly set the time resolution
	// and since they were added, when playing on a laptop on battery it migth add heavy stuttering when using a frame limiter based on Sleep function (or std::this_thread::sleep_for) implementation
	// the game sets them already but only during the loading screen period, then it resets to system default when the loading screen ends 
	// (tho i think in the new implementation is working on a per thread basis now instead of global frequency, since it still works even when the game resets after loading screen ends and loading screen runs in another thread)

	// More code in Tweaks.cpp in InitH2Tweaks

	// More details @ https://randomascii.wordpress.com/2020/10/04/windows-timer-resolution-the-great-rule-change/

	timeBeginPeriod(SYSTEM_TIMER_RESOLUTION_MS);
	g_main_game_time_counter_last_time = shell_time_counter_now(NULL);
	INVOKE(0x2869F, 0x24841, main_game_time_initialize_hook);
	return;
}
