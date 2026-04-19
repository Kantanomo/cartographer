#pragma once

/* constants */

enum
{
	k_max_strings_per_language = 0x8000
};

/* structures */

struct s_string_reference
{
	string_id string_id;
	uint32 buffer_offset;
};

/* classes */

// TODO add member functions
class c_language_pack
{
public:
	bool unload_data(void);
	
	bool try_find_string_exists(string_id id, int32 starting_index, int32 max_count);
	utf8* get_string_utf8(string_id id, int32 starting_index, int32 max_count);
	
	void string_list_get_normal_string(string_id id, c_maximum_interface_text* out_string, int32 strings_start_index, int32 strings_count);
	void get_string_ids(string_id* array, int32 array_size, int32 starting_index, int32 max_count);


private:
	s_string_reference* m_string_references;
	utf8* m_string_data;
	int32 m_num_of_strings;
	int32 m_string_data_size;
	int32 m_string_reference_cache_offset;
	int32 m_string_data_cache_offset;
	bool m_data_loaded;
};
ASSERT_STRUCT_SIZE(c_language_pack, 28);

/* prototypes */

void __cdecl string_list_get_normal_string(datum unic_datum, string_id id, c_maximum_interface_text* out_string);
void __cdecl string_list_get_string_id_list(datum unic_datum, string_id* out_ids, int32 out_count);
