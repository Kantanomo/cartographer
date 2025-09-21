#include "stdafx.h"
#include "game_time.h"

#include "main/main_time.h"
#include "H2MOD.h"
#include "cutscene/cinematics.h"
#include "math/math.h"
#include "main/interpolator.h"
#include "main/main_game_time.h"
#include "shell/shell_windows.h"

/* structures */

struct s_time_globals
{
	bool initialized;
	bool paused;
	int16 tick_rate;
	real32 tick_length;
	uint32 passed_ticks_count;
	real32 game_speed;
	real32 game_ticks_leftover;
	real32 field_14;
	real32 field_18;
	real32 field_1C;
	real32 field_20;
};
ASSERT_STRUCT_SIZE(s_time_globals, 36);

/* prototypes */

static s_time_globals* time_globals_get(void);

static bool __cdecl cinematic_is_running_hook(void);

/* public code */

void game_time_apply_patches(void)
{
	// apply framerate throttle patches for when the game is minimized
	PatchCall(Memory::GetAddress(0x39A2A), cinematic_is_running_hook);
	return;
}

bool game_time_initialized(void)
{
	const s_time_globals* game_time_globals = time_globals_get();
	return game_time_globals && game_time_globals->initialized;
}

uint32 game_time_get(void)
{
	const s_time_globals* game_time_globals = time_globals_get();
	ASSERT(game_time_globals && game_time_globals->initialized);

	return game_time_globals->passed_ticks_count;
}

void game_time_discard(int32 desired_ticks, int32 actual_ticks, real32* elapsed_game_dt)
{
	s_time_globals* game_time_globals = time_globals_get();
	ASSERT(game_time_globals);
	ASSERT(game_time_globals->initialized);
	ASSERT(desired_ticks > 0);
	ASSERT(actual_ticks < desired_ticks);
	ASSERT(elapsed_game_dt);

	if (actual_ticks > 0)
	{
		const real32 difference = (real32)(desired_ticks - actual_ticks);
		const real32 lost_dt = game_ticks_to_seconds(difference);
		ASSERT(*elapsed_game_dt - lost_dt > -(_real_epsilon + game_tick_length()));

		const real32 result = *elapsed_game_dt - lost_dt;
		*elapsed_game_dt = result > 0.f ? result : 0.f;
	}
	else
	{
		*elapsed_game_dt = 0.f;
	}

	return;
}

void game_time_advance(void)
{
	s_time_globals* game_time_globals = time_globals_get();
	ASSERT(game_time_globals);
	ASSERT(game_time_globals->initialized);
	++game_time_globals->passed_ticks_count;
	return;
}

bool game_time_get_paused(void)
{
	s_time_globals* game_time_globals = time_globals_get();
	ASSERT(game_time_globals);
	return game_time_globals->initialized && game_time_globals->paused;
}

void game_time_set_paused(bool pause)
{
	s_time_globals* game_time_globals = time_globals_get();
	ASSERT(game_time_globals);
	ASSERT(game_time_globals->initialized);

	game_time_globals->paused = pause;
	return;
}

real32 game_time_get_speed(void)
{
	const s_time_globals* game_time_globals = time_globals_get();
	ASSERT(game_time_globals);
	ASSERT(game_time_globals->initialized);
	return game_time_globals->game_speed;
}

void game_time_set_speed(real32 speed)
{
	s_time_globals* game_time_globals = time_globals_get();
	ASSERT(game_time_globals);
	ASSERT(game_time_globals->initialized);
	game_time_globals->game_speed = speed;
	return;
}

int32 game_seconds_integer_to_ticks(int32 seconds)
{
	const s_time_globals* game_time_globals = time_globals_get();

	ASSERT(game_time_globals);
	ASSERT(game_time_globals->initialized);

	return seconds * game_time_globals->tick_rate;
}

bool __cdecl game_time_update(real32 world_seconds_elapsed, real32* game_seconds_elapsed, int32* game_ticks_elapsed)
{
	return INVOKE(0x7C1BF, 0x4BE6F, game_time_update, world_seconds_elapsed, game_seconds_elapsed, game_ticks_elapsed);
}

int32 game_tick_rate(void)
{
	const s_time_globals* game_time_globals = time_globals_get();
	ASSERT(game_time_globals);
	ASSERT(game_time_globals->initialized);

	return game_time_globals->tick_rate;
}

real32 game_tick_length(void)
{
	const s_time_globals* game_time_globals = time_globals_get();
	ASSERT(game_time_globals);
	ASSERT(game_time_globals->initialized);

	return game_time_globals->tick_length;
}

real32 game_ticks_to_seconds(real32 ticks)
{
	const s_time_globals* game_time_globals = time_globals_get();
	ASSERT(game_time_globals);
	ASSERT(game_time_globals->initialized);

	return game_time_globals->tick_length * ticks;
}

real32 game_seconds_to_ticks_real(real32 seconds)
{
	const s_time_globals* game_time_globals = time_globals_get();

	ASSERT(game_time_globals);
	ASSERT(game_time_globals->initialized);

	return ((real32)game_time_globals->tick_rate * seconds);
}

real32 game_tick_rate_legacy_ratio_real()
{
	const s_time_globals* game_time_globals = time_globals_get();

	ASSERT(game_time_globals);
	ASSERT(game_time_globals->initialized);

	return (real32)game_time_globals->tick_rate / 30.f;
}

int32 game_seconds_to_ticks_round(real32 seconds)
{
	const s_time_globals* game_time_globals = time_globals_get();

	ASSERT(game_time_globals);
	ASSERT(game_time_globals->initialized);

	return (int32)((real32)game_time_globals->tick_rate * seconds);
}

real32 game_time_get_max_frame_time(void)
{
	s_time_globals* game_time_globals = time_globals_get();
	ASSERT(game_time_globals);
	ASSERT(game_time_globals->initialized);

	const real32 result = game_time_globals->tick_length - (real32)(game_time_globals->game_ticks_leftover / (real32)game_time_globals->tick_rate);
	return MAX(result, 0.f);
}

real32 game_time_get_leftover(void)
{
	s_time_globals* game_time_globals = time_globals_get();
	return PIN(game_time_globals->game_ticks_leftover, 0.f, 1.f);
}


/* private code */

static s_time_globals* time_globals_get(void)
{
	return *Memory::GetAddress<s_time_globals**>(0x4C06E4, 0x4CF0EC);
}

// Disables broken/experimental main loop patches in the vanilla game 
// that are also disabled when playing cinematics
// This also fixes the built in frame limiter while the game is minimized, as well as the speeding up issue
static bool __cdecl cinematic_is_running_hook(void)
{
	bool result;
	if (halo_frame_interpolator_enabled())
	{
		result = true; // these two options disable the hacks that hired gun added to the main loop
	}
	else
	{
		result = cinematic_in_progress_not_main_menu() || xbox_tickrate_is_enabled() || main_time_is_throttled() || g_main_game_time_frame_limiter_enabled;
	}

	return result;
}