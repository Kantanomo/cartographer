#include "stdafx.h"

#include "network_statistics.h"

/* constants */

/* declarations */

/* globals */

/* public code */

real32 c_network_window_statistics::average_values_in_window(void) const
{
	return (real32)field_108 / (real32)m_window_size;
}

void c_network_time_statistics::initialize(int32 period_duration_msec)
{
	INVOKE_TYPE(0x1F5B43, 0x1E051F, void(__thiscall*)(c_network_time_statistics*, int32), this, period_duration_msec);
	return;
}
