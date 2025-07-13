#include "stdafx.h"
#include "xml_definition_block.h"

/* public code */

c_xml_definition_block::c_xml_definition_block(const tinyxml2::XMLElement* base_element, uint32 offset, uint32 size)
{
	// Checks to make sure the correct parameters were passed.
	// offset and size are default -1 and overwritten if the attribute exists
	// for the given base_element if somehow they are being passed as -1 it will most
	// likely indicate there is an issue in the plugin xml file.
	ASSERT(offset != UINT_MAX);
	ASSERT(size != UINT_MAX);

	m_element = base_element;
	m_offset = offset;
	m_size = size;
	m_tag_references = nullptr;
	m_classless_tag_references = nullptr;
	m_string_ids = nullptr;
	m_data_references = nullptr;
	m_tag_blocks = nullptr;

#if TAG_INJECTION_DEBUG
	m_tag_reference_names = nullptr;
	m_classless_tag_reference_names = nullptr;
	m_data_reference_names = nullptr;
	m_tag_block_names = nullptr;
#endif

	reset_counts();

	if (m_element->Attribute("name"))
		m_name.set(m_element->Attribute("name"));
	
	// do a first pass of the current element to get the counts we need
	// for allocating the buffers
	get_element_counts();
	allocate_buffers();

	// counts are reset to be re-used to keep track of which index
	// inside the array the current item should be placed
	reset_counts();
	populate_buffers();
	return;
}

void c_xml_definition_block::reset_counts()
{
	m_tag_reference_count = 0;
	m_classless_tag_reference_count = 0;
	m_string_id_count = 0;
	m_data_reference_count = 0;
	m_tag_block_count = 0;
}

void c_xml_definition_block::get_element_counts()
{
	const tinyxml2::XMLElement* element = m_element->FirstChildElement();
	while (element)
	{
		const char* element_name = element->Name();

		if (strcmp(element_name, "tagblock") == 0 || strcmp(element_name, "reflexive") == 0)
		{
			m_tag_block_count++;

			element = element->NextSiblingElement();
			continue;
		}
		if (strcmp(element_name, "tagRef") == 0 || strcmp(element_name, "tagref") == 0)
		{
			if (element->BoolAttribute("withClass") || element->Attribute("withClass") 
				|| element->BoolAttribute("withGroup") || element->Attribute("withGroup"))
				m_classless_tag_reference_count++;
			else
				m_tag_reference_count++;

			element = element->NextSiblingElement();
			continue;
		}
		if (strcmp(element_name, "stringId") == 0 || strcmp(element_name, "stringid") == 0)
		{
			m_string_id_count++;

			element = element->NextSiblingElement();
			continue;
		}
		if (strcmp(element_name, "dataref") == 0)
		{
			m_data_reference_count++;

			element = element->NextSiblingElement();
			continue;
		}

		element = element->NextSiblingElement();
	}
}

void c_xml_definition_block::allocate_buffers()
{
	if (m_tag_reference_count)
		m_tag_references = (uint32*)malloc(sizeof(uint32) * m_tag_reference_count);

	if (m_classless_tag_reference_count)
		m_classless_tag_references = (uint32*)malloc(sizeof(uint32) * m_classless_tag_reference_count);

	if (m_string_id_count)
		m_string_ids = (uint32*)malloc(sizeof(uint32) * m_string_id_count);

	if (m_data_reference_count)
		m_data_references = (uint32*)malloc(sizeof(uint32) * m_data_reference_count);

	if (m_tag_block_count)
		m_tag_blocks = new c_xml_definition_block[m_tag_block_count];

#if TAG_INJECTION_DEBUG
	if (m_tag_reference_count)
		m_tag_reference_names = new c_static_string<64>[m_tag_reference_count];

	if (m_classless_tag_reference_count)
		m_classless_tag_reference_names = new c_static_string<64>[m_classless_tag_reference_count];

	if (m_data_reference_count)
		m_data_reference_names = new c_static_string<64>[m_data_reference_count];

	if (m_tag_block_count)
		m_tag_block_names = new c_static_string<64>[m_tag_block_count];
#endif
}

void c_xml_definition_block::populate_buffers()
{
	const tinyxml2::XMLElement* element = m_element->FirstChildElement();
	while (element)
	{
		const char* element_name = element->Name();
		uint32 offset = UINT_MAX;
		if (element->Attribute("offset") != nullptr)
			offset = strtoul(element->Attribute("offset"), nullptr, 16);
		if (strcmp(element_name, "tagblock") == 0 || strcmp(element_name, "reflexive") == 0)
		{
			uint32 size = UINT_MAX;
			if (element->Attribute("entrySize") != nullptr)
				size = strtoul(element->Attribute("entrySize"), nullptr, 16);
			else if (element->Attribute("elementSize") != nullptr)
				size = strtoul(element->Attribute("elementSize"), nullptr, 16);

			new (&m_tag_blocks[m_tag_block_count]) c_xml_definition_block(element, offset, size);

#if TAG_INJECTION_DEBUG
			m_tag_block_names[m_tag_block_count].set(element->Attribute("name"));
#endif

			m_tag_block_count++;

			element = element->NextSiblingElement();
			continue;
		}
		if (strcmp(element_name, "tagRef") == 0 || strcmp(element_name, "tagref") == 0)
		{
			if (element->BoolAttribute("withClass") || element->Attribute("withClass")
				|| element->BoolAttribute("withGroup") || element->Attribute("withGroup"))
			{
				m_classless_tag_references[m_classless_tag_reference_count] = offset;

#if TAG_INJECTION_DEBUG
				m_classless_tag_reference_names[m_classless_tag_reference_count].set(element->Attribute("name"));
#endif

				m_classless_tag_reference_count++;
			}
			else
			{
				m_tag_references[m_tag_reference_count] = offset;

#if TAG_INJECTION_DEBUG
				m_tag_reference_names[m_tag_reference_count].set(element->Attribute("name"));
#endif

				m_tag_reference_count++;
			}

			element = element->NextSiblingElement();
			continue;
		}
		if (strcmp(element_name, "stringId") == 0 || strcmp(element_name, "stringid") == 0)
		{
			m_string_ids[m_string_id_count] = offset;
			m_string_id_count++;

			element = element->NextSiblingElement();
			continue;
		}
		if (strcmp(element_name, "dataref") == 0)
		{
			m_data_references[m_data_reference_count] = offset;

#if TAG_INJECTION_DEBUG
			m_data_reference_names[m_data_reference_count].set(element->Attribute("name"));
#endif

			m_data_reference_count++;

			element = element->NextSiblingElement();
			continue;
		}
		element = element->NextSiblingElement();
	}
}

void c_xml_definition_block::clear()
{
	if (m_tag_reference_count)
		free(m_tag_references);

	if (m_classless_tag_reference_count)
		free(m_classless_tag_references);

	if (m_string_id_count)
		free(m_string_ids);

	if (m_tag_reference_count)
		free(m_data_references);

#if TAG_INJECTION_DEBUG
	if (m_tag_reference_count)
		delete[] m_tag_reference_names;

	if (m_classless_tag_reference_count)
		delete[] m_classless_tag_reference_names;

	if (m_data_reference_count)
		delete[] m_data_reference_names;

	if (m_tag_block_count)
		delete[] m_tag_block_names;
#endif

	if (m_tag_block_count)
	{
		for (uint32 i = 0; i < m_tag_block_count; i++)
		{
			m_tag_blocks[i].clear();
		}
		free(m_tag_blocks);
	}

	reset_counts();
}

uint32 c_xml_definition_block::get_size() const
{
	return m_size;
}

uint32 c_xml_definition_block::get_offset() const
{
	return m_offset;
}

const char* c_xml_definition_block::get_name() const
{
	return m_name.get_string();
}

uint32 c_xml_definition_block::get_tag_references_count() const
{
	return m_tag_reference_count;
}

uint32 c_xml_definition_block::get_tag_reference_offset(uint32 index) const
{
	return m_tag_references[index];
}

uint32 c_xml_definition_block::get_classless_tag_references_count() const
{
	return m_classless_tag_reference_count;
}

uint32 c_xml_definition_block::get_classless_tag_reference_offset(uint32 index) const
{
	return m_classless_tag_references[index];
}

uint32 c_xml_definition_block::get_data_references_count() const
{
	return m_data_reference_count;
}

uint32 c_xml_definition_block::get_data_reference_offset(uint32 index) const
{
	return m_data_references[index];
}

uint32 c_xml_definition_block::get_string_id_count() const
{
	return m_string_id_count;
}

uint32 c_xml_definition_block::get_string_id_offset(uint32 index) const
{
	return m_string_ids[index];
}

uint32 c_xml_definition_block::get_tag_block_count() const
{
	return m_tag_block_count;
}

c_xml_definition_block* c_xml_definition_block::get_tag_block(uint32 index) const
{
	return &m_tag_blocks[index];
}

#if TAG_INJECTION_DEBUG

c_static_string<64>* c_xml_definition_block::get_tag_reference_name(uint32 index) const
{
	return &m_tag_reference_names[index];
}

c_static_string<64>* c_xml_definition_block::get_classless_tag_reference_name(uint32 index) const
{
	return &m_classless_tag_reference_names[index];
}

c_static_string<64>* c_xml_definition_block::get_data_reference_name(uint32 index) const
{
	return &m_data_reference_names[index];
}

c_static_string<64>* c_xml_definition_block::get_tag_block_name(uint32 index) const
{
	return &m_tag_block_names[index];
}

#endif
