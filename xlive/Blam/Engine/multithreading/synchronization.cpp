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
