#pragma once

/* constants */

//#define MAIN_GAME_TIME_DEBUG

#define SYSTEM_TIMER_RESOLUTION_MS 1

/* structures */

struct s_main_time_debug
{
	real32 dt_default;
	real32 dt_performance_counter;
};

#pragma pack(push, 1)
struct s_main_time_globals
{
	// NOTE this is not the entire structure
	// this is somewhere in the middle of the base structure
	bool field_0;
	char pad_1[1];
	int last_game_time;

	union
	{
		struct
		{
			uint32 low_time;
			uint32 hi_time;
		} last_milliseconds_parts;
		uint64 last_milliseconds;
	};

	bool should_reset;
	char pad_2[7];
	__int64 field_16[2];

	static inline s_main_time_globals* get()
	{
		return Memory::GetAddress<s_main_time_globals*>(0x479E92, 0x4A2982);
	}
};
#pragma pack(pop)
ASSERT_STRUCT_SIZE(s_main_time_globals, 38);

/* globals */

extern bool g_main_game_time_frame_limiter_enabled;

#ifdef MAIN_GAME_TIME_DEBUG
extern s_main_time_debug g_main_game_time_debug;
#endif

/* prototypes */

void main_game_time_apply_patches(void);

void __cdecl main_time_reset(void);

real32 __cdecl main_time_update(bool fixed_time_step, real32 fixed_time_delta);

bool should_limit_framerate();

