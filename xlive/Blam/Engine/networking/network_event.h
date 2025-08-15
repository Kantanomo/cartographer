#pragma once

#ifdef EVENTS_ENABLED

/* enums */

enum e_event_level : int32
{
	_event_verbose,
	_event_status,
	_event_message,
	_event_warning,
	_event_error,
	_event_fatal,
	k_network_event_level_count,
	_event_invalid = NONE
};

/* classes */

class c_event
{
public:
	c_event(void) : m_event_level(_event_invalid), m_category_set(false), m_category(NONE) {};

	bool query(e_event_level event_level);
	void generate(const char* format, ...);

private:
	e_event_level m_event_level;
	bool m_category_set;
	int32 m_category;
};

/* macros */

#define GENERATE_EVENT_NAME() J(x_event_, __LINE__)

#define event(severity, format, ...)							\
do																\
{																\
	static c_event GENERATE_EVENT_NAME();						\
	if (GENERATE_EVENT_NAME().query(severity))					\
	{															\
		GENERATE_EVENT_NAME().generate(format, __VA_ARGS__);	\
	}															\
}																\
while(0)

/* prototypes */

void network_event_initialize(void);

void network_event_dispose(void);

void network_event_dump_categories(const char* event_name);

void network_event_display_category(const char* category_string, e_event_level level);

void network_event_log_category(const char* category_string, e_event_level level);

#else
#define event(severity, format, ...) (void)(0)

#endif