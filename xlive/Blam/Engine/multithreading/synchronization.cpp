#include "stdafx.h"
#include "synchronization.h"

/* public code */

c_critical_section_scope::c_critical_section_scope(LPCRITICAL_SECTION section)
{
	m_critical_section = section;
	EnterCriticalSection(section);
	return;
}

c_critical_section_scope::~c_critical_section_scope(void)
{
	LeaveCriticalSection(m_critical_section);
	return;
}

LPCRITICAL_SECTION mutex_create(void)
{
	LPCRITICAL_SECTION mutex = new CRITICAL_SECTION;
	InitializeCriticalSection(mutex);
	return mutex;
}

bool mutex_valid(LPCRITICAL_SECTION section)
{
	return section != NULL;
}
