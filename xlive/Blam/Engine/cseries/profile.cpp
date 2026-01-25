#include "stdafx.h"
#include "profile.h"

#ifdef PROFILE_ENABLED

/* constants */

enum
{
	MAXIMUM_PROFILE_SECTIONS = 2048,
	CPU_PROFILE_STACK_SIZE = 128,
	ATTRIBUTION_STACK_SIZE = 16,
};

/* enums */

enum e_profile_interval
{
	_profile_interval_0,
	_profile_interval_1,
	_profile_interval_2,
	k_profile_interval_count
};

enum e_profile_mode
{
	_profile_mode_none = NONE,
	_profile_mode_0 = 0,
	_profile_mode_1,
	_profile_mode_2,
	_profile_mode_3,
	_profile_mode_4,
	_profile_mode_5,
	_profile_mode_6,
	_profile_mode_7,
	_profile_mode_8,
	_profile_mode_9,
	_profile_mode_10,
	_profile_mode_11,
	_profile_mode_12,
	_profile_mode_13,
	_profile_mode_14,
	_profile_mode_15,
	_profile_mode_16,
	k_profile_mode_count
};

enum e_profile_sort_mode
{
	_profile_sort_mode_0 = 0,
	_profile_sort_mode_1,
	_profile_sort_mode_2,
	k_profile_sort_mode_count
};

enum e_profile_summary_mode
{
	_profile_summary_mode_all = 0,
	_profile_summary_mode_objects,
	_profile_summary_mode_graphics,
	_profile_summary_mode_ai,
	_profile_summary_mode_game_state,
	k_profile_summary_mode_count
};

/* structures */

struct s_profile_timesection
{
	int64 start_time;
	real32 total_time;
	int32 samples;
};

#pragma pack(push,4)
struct s_profile_globals
{
	int64 frequency;
	bool calibrated;
	e_profile_mode requested_profile_mode;
	bool reset;
	bool update_profile_mode;
	bool active;
	bool update_freeze_toggle;
	const char* substring;
	int16 section_count;
	s_profile_section* sections[MAXIMUM_PROFILE_SECTIONS];
	
	bool enabled;
	e_profile_mode current_profile_mode;
	e_profile_interval profile_interval;
	bool profile_interval_active;
	uint32 sample_count;
	bool unfrozen;
	int8 pad[7];
	int64 start_time;
	int64 end_time;
	
	bool cpu_profile;
	bool cpu_profile_active;
	int16 cpu_profile_stack_depth;
	s_profile_section* cpu_profile_stack[CPU_PROFILE_STACK_SIZE];
	bool render_legend;
	bool field_224D;	// Seems to be part of unfinished/removed code?
	
	real32 interval[k_profile_interval_count];

	int32 attribution;
	bool attribution_active;
	int32 attribution_stack_depth;
	int32 attribution_stack[ATTRIBUTION_STACK_SIZE];
	int32 attribution_subsystem_count;

	int32 field_22AC;
	s_profile_timesection attribution_stack_timesection[k_profile_attribution_maximum_subsystem_count];
	s_profile_timesection attribution_timesection[k_profile_attribution_maximum_subsystem_count];
	
	e_profile_sort_mode sort_mode;
	e_profile_summary_mode summary_mode;
	bool increment_summary_mode;
	bool profiling_enabled;

	uint32 profile_file_index;
};
#pragma pack(pop)

/* globals */

static s_profile_globals profile_globals;

/* public code */

c_stop_watch::c_stop_watch(void)
{
	m_total_time = 0;
	m_start_time = get_performance_counter();
	m_paused = false;
	return;
}

c_stop_watch::c_stop_watch(bool start_paused)
{
	m_total_time = 0;
	m_start_time = get_performance_counter();
	m_paused = start_paused;
	return;
}

int64 c_stop_watch::start(void)
{
	const int64 result = total_elapsed_time_in_cycles();
	if (m_paused)
	{
		m_paused = false;
		m_start_time = get_performance_counter();
	}
	return result;
}

int64 c_stop_watch::hold(void)
{
	if (!m_paused)
	{
		m_total_time += get_current_delta();
		m_paused = true;
	}
	return m_total_time;
}

int64 c_stop_watch::reset(void)
{
	const int64 result = stop();
	start();
	return result;
}

int64 c_stop_watch::stop(void)
{
	const int64 result = hold();
	m_total_time = 0;
	return result;
}

real32 c_stop_watch::cycles_to_seconds(int64 cycles) const
{
	const int64 counter = get_performance_frequency();
	const real32 remainder_fraction = (real32)((real64)(cycles % counter) / (real64)counter);
	return (cycles / counter) + remainder_fraction;
}

int64 c_stop_watch::total_elapsed_time_in_cycles(void) const
{
	int64 result = m_total_time;
	if (!m_paused)
	{
		result += get_current_delta();
	}
	return result;
}

real32 c_stop_watch::total_elapsed_time_in_seconds(void) const
{
	const int64 cycles = total_elapsed_time_in_cycles();
	return cycles_to_seconds(cycles);
}

int64 c_stop_watch::get_current_delta(void) const
{
	ASSERT(!m_paused);
	const int64 delta = get_performance_counter() - m_start_time;
	return MAX(delta, 0);
}

bool profile_active(void)
{
	return profile_globals.enabled && profile_globals.current_profile_mode != _profile_mode_none;
}

void profile_frame_start(void)
{
	// TODO: implement
	return;
}

void profile_frame_end(void)
{
	// TODO: implement
	return;
}

void profile_attribute_enter(int32 a1, e_profile_attribution_subsystem subsystem)
{
	// TODO: implement
	return;
}

void profile_attribute_exit(int32 a1, e_profile_attribution_subsystem subsystem)
{
	// TODO: implement
	return;
}



int64 get_performance_counter(void)
{
	LARGE_INTEGER result;
	const BOOL success = QueryPerformanceCounter(&result);
	ASSERT(success);

	if (!success)
	{
		result.QuadPart = GetTickCount();
	}
	return result.QuadPart;
}

int64 get_performance_frequency(void)
{
	LARGE_INTEGER result;
	const BOOL success = QueryPerformanceFrequency(&result);
	ASSERT(success);

	if (!success)
	{
		result.QuadPart = GetTickCount();
	}
	return result.QuadPart;
}


#endif
