#pragma once

#ifdef PROFILE_ENABLED

/* enums */

enum e_profile_attribution_subsystem
{
	// TODO: names
	_profile_attribution_subsystem_0,
	_profile_attribution_subsystem_1,
	_profile_attribution_subsystem_2,
	_profile_attribution_subsystem_3,
	_profile_attribution_subsystem_4,
	_profile_attribution_subsystem_5,
	_profile_attribution_subsystem_6,
	_profile_attribution_subsystem_7,
	_profile_attribution_subsystem_8,
	_profile_attribution_subsystem_9,
	_profile_attribution_subsystem_10,
	_profile_attribution_subsystem_11,
	_profile_attribution_subsystem_12,
	_profile_attribution_subsystem_13,
	_profile_attribution_subsystem_14,
	_profile_attribution_subsystem_15,
	_profile_attribution_subsystem_16,
	_profile_attribution_subsystem_17,
	_profile_attribution_subsystem_18,
	_profile_attribution_subsystem_19,
	_profile_attribution_subsystem_20,
	_profile_attribution_subsystem_21,
	_profile_attribution_subsystem_22,
	_profile_attribution_subsystem_23,
	_profile_attribution_subsystem_24,
	_profile_attribution_subsystem_25,
	_profile_attribution_subsystem_26,
	_profile_attribution_subsystem_27,
	_profile_attribution_subsystem_28,
	_profile_attribution_subsystem_29,
	_profile_attribution_subsystem_30,
	_profile_attribution_subsystem_31,
	k_profile_attribution_maximum_subsystem_count
};

/* structures */

struct s_profile_section
{
	const char* name;
	int32 section_index;
	bool profile_active;
	int8 pad;
	int16 stack_depth;
	int16 recursion_count;
	int16 pad_1;
	int8 gap[1544];
};

/* classes */

class c_stop_watch
{
public:
	c_stop_watch(void);
	c_stop_watch(bool start_paused);
	~c_stop_watch(void) = default;

	int64 start(void);

	int64 hold(void);

	int64 reset(void);

	int64 stop(void);

	real32 cycles_to_seconds(int64 cycles) const;
	
	int64 total_elapsed_time_in_cycles(void) const;

	real32 total_elapsed_time_in_seconds(void) const;

	int64 get_current_delta(void) const;

private:
	int64 m_total_time;
	int64 m_start_time;
	bool m_paused;
};

/* prototypes */

bool profile_active(void);

void profile_frame_start(void);

void profile_frame_end(void);

void profile_attribute_enter(int32 a1, e_profile_attribution_subsystem subsystem);

void profile_attribute_exit(int32 a1, e_profile_attribution_subsystem subsystem);

int64 get_performance_counter(void);

int64 get_performance_frequency(void);

#else

#define profile_attribute_enter(...)

#define profile_attribute_exit(...)

#endif
