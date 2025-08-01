#include "stdafx.h"
#include "text_group.h"
#include "cache/cache_files.h"
#include "main/game_preferences.h"
#include "game/game_globals.h"
#include "tag_files/global_string_ids.h"


/* structures */
struct s_unicode_string_list_reference
{
	uint16 strings_index;
	uint16 strings_count;
};


/* public code */

bool c_language_pack::try_find_string_exists(string_id id, int32 starting_index, int32 max_count)
{
	return INVOKE_TYPE(0x3DF4C, 0x0, bool(__thiscall*)(c_language_pack*, string_id, int32, int32), this, id, starting_index, max_count);
}

utf8* c_language_pack::get_string_utf8(string_id id, int32 starting_index, int32 max_count)
{
	return INVOKE_TYPE(0x3DEFD, 0x0, utf8*(__thiscall*)(c_language_pack*, string_id, int32, int32), this, id, starting_index, max_count);
}

const int32 c_language_pack::get_num_of_strings() const
{
	ASSERT(m_strings_count < k_max_strings_per_language);

	return m_strings_count;
}

void c_language_pack::string_list_get_normal_string(string_id id, c_maximum_interface_text* out_string, int32 strings_start_index, int32 strings_count)
{
	INVOKE_TYPE(0x3E332, 0x0, void(__thiscall*)(c_language_pack*, string_id, c_maximum_interface_text*, int32, int32), this, id, out_string, strings_start_index, strings_count);
}

void c_language_pack::get_string_ids(string_id* out_ids, int32 out_count, int32 starting_index, int32 max_count)
{
	csmemset(out_ids, _string_id_invalid, sizeof(*out_ids) * out_count);
	if (data_loaded)
	{
		for (int32 itr = starting_index; itr < max_count + starting_index; itr++)
		{
			int32 index = itr - starting_index;
			if (index < out_count)//prevent buffer overrun
			{
				out_ids[index] = this->m_string_references[itr].string_id;
			}
		}
	}
}

void __cdecl string_list_get_normal_string(datum unic_datum, string_id id, c_maximum_interface_text* out_string)
{
	INVOKE(0x3E3AC, 0x0, string_list_get_normal_string, unic_datum, id, out_string);
}

void __cdecl string_list_get_string_id_list(datum unic_datum, string_id* out_ids, int32 out_count)
{
	ASSERT(out_ids);
	ASSERT(out_count > 0);

	if (unic_datum != NONE)
	{
		e_language current_language = get_current_language();
		int8* unic_tag = (int8*)tag_get_fast(unic_datum);
		s_unicode_string_list_reference* list_reference = &((s_unicode_string_list_reference*)(unic_tag + 0x10))[current_language];

		s_game_globals* game_globals = scenario_get_game_globals();
		game_globals->language_pack[current_language].get_string_ids(out_ids, out_count, list_reference->strings_index, list_reference->strings_count);
	}
}
