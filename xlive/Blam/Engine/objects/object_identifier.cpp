#include "stdafx.h"
#include "object_identifier.h"
#include "object_globals.h"

/* public code */

void c_object_identifier::clear(
	void)
{
	m_object_type = _object_type_none;
	m_source = (e_object_source)NONE;
	m_origin_bsp_index = NONE;
	m_unique_id = NONE;
	
	return;
}

void c_object_identifier::clear_for_deletion(
	void)
{
	m_source = (e_object_source)NONE;
	m_origin_bsp_index = NONE;
	m_unique_id = NONE;
	
	return;
}

void c_object_identifier::create_dynamic(
	e_object_type type)
{
	s_object_globals* object_globals = object_globals_get();

	m_object_type = (int8)type;
	m_source = _object_source_dynamic;
	m_origin_bsp_index = NONE;
	m_unique_id = ++object_globals->unique_id;
	return;
}

int16 c_object_identifier::get_origin_bsp(
	void) const
{
	return m_origin_bsp_index;
}

e_object_source c_object_identifier::get_source(
	void) const
{
	return m_source;
}

e_object_type c_object_identifier::get_type(
	void) const
{
	return (e_object_type)m_object_type;
}

int32 c_object_identifier::get_unique_id(
	void) const
{
	return m_unique_id;
}

bool c_object_identifier::is_scenario_object(
	void) const
{
	return TEST_BIT(get_source(), _object_source_legacy);
}

datum c_object_identifier::find_object_index(
	void) const
{
	return INVOKE_TYPE(0x134CFC, 0x123BCC, datum(__thiscall*)(const c_object_identifier*), this);
}
