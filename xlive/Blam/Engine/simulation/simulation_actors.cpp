#include "stdafx.h"
#include "simulation_actors.h"

/* public code */

bool c_simulation_actor::exists(void) const
{
	return m_actor_index != NONE;
}

void c_simulation_actor::destroy(void)
{
	ASSERT(exists());

	m_actor_index = NONE;
	m_unit_index = NONE;
	m_simulation_world = NULL;
	return;
}