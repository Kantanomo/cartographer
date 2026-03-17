#include "stdafx.h"
#include "replication_event_manager.h"

/* public code */

void c_replication_event_manager::initialize(
	c_replication_entity_manager* entity_manager)
{
	m_entity_manager = entity_manager;
	m_client = NULL;
	m_view_mask = 0;
	csmemset(m_views, 0, sizeof(m_views));
	m_outgoing_event_count = 0;
	m_outgoing_event_list = NULL;

	return;
}

void c_replication_event_manager::destroy(
	void)
{
	ASSERT(m_view_mask==0);

	for (int32 view_index= 0; view_index <NUMBEROF(m_views); ++view_index)
	{
		ASSERT(m_views[view_index]==NULL);
	}

	ASSERT(m_outgoing_event_count==0);
	ASSERT(m_outgoing_event_list==NULL);

	return;
}

void c_replication_event_manager::reset(void)
{
	INVOKE_TYPE(0x1D676D, 0x1D9A8E, void(__thiscall*)(c_replication_event_manager*), this);
	return;
}

void c_replication_event_manager::register_client(
	c_simulation_event_handler* client)
{
	ASSERT(client!=NULL);
	ASSERT(m_client==NULL);

	m_client = client;

	return;
}
