#include "stdafx.h"
#include "simulation_type_collection.h"

/* public code */

c_simulation_entity_definition* c_simulation_type_collection::get_entity_definition(
	e_simulation_entity_type entity_type) const
{
	ASSERT(entity_type>=0 && entity_type<m_entity_type_count);
	
	c_simulation_entity_definition* entity_definition = m_entity_definitions[entity_type];
	ASSERT(entity_definition !=NULL);
	ASSERT(entity_definition->entity_type()==entity_type);

	return entity_definition;
}

c_simulation_event_definition* c_simulation_type_collection::get_event_definition(
	e_simulation_event_type event_type) const
{
	ASSERT(event_type>=0 && event_type<m_event_type_count);

	c_simulation_event_definition* event_definition = m_event_definitions[event_type];
	ASSERT(event_definition!=NULL);
	ASSERT(event_definition->event_type()==event_type);

	return event_definition;
}

int32 c_simulation_type_collection::get_event_definition_count(
	void) const
{
	ASSERT(m_event_type_count >= 0);
	return m_event_type_count;
}

void c_simulation_type_collection::register_entity_definition(
	e_simulation_entity_type entity_type,
	c_simulation_entity_definition* definition)
{
	m_entity_definitions[entity_type] = definition;
	return;
}

void c_simulation_type_collection::register_event_definition(
	e_simulation_event_type type,
	c_simulation_event_definition* definition)
{
	m_event_definitions[type] = definition;
	return;
}
