#pragma once

/* classes */

class c_critical_section_scope
{
public:
	c_critical_section_scope(LPCRITICAL_SECTION section);
	~c_critical_section_scope(void);

private:
	LPCRITICAL_SECTION m_critical_section;
};

/* prototypes */

LPCRITICAL_SECTION mutex_create(void);

bool mutex_valid(LPCRITICAL_SECTION section);
