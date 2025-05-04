#pragma once
#include "object_type_list.h"

enum e_object_source : int8
{
	_object_source_structure = 0,
	_object_source_editor = 1,
	_object_source_dynamic = 2,
	_object_source_legacy = 3,
};

class c_object_identifier
{
private:
	int32 m_unique_id;
	int16 m_origin_bsp_index;
	int8/*e_object_type*/ m_object_type;
	e_object_source m_source;

public:
	c_object_identifier(void) = default;
	~c_object_identifier(void) = default;
	void clear(void);
	void clear_for_deletion(void);
	void create_dynamic(e_object_type type);

	int16 get_origin_bsp(void) const;
	e_object_source get_source(void) const;
	e_object_type get_type(void) const;
	int32 get_unique_id(void) const;

	bool is_scenario_object(void) const;
	datum find_object_index(void) const;
};
ASSERT_STRUCT_SIZE(c_object_identifier, 8);