#include "stdafx.h"
#include "halo_playlist.h"

#include "definitions/halo_playlist_match_property.h"

typedef int32(__cdecl* t_halo_playlist_new)(wchar_t*, s_halo_playlist_container*);
t_halo_playlist_new p_halo_playlist_new;

int32 __cdecl halo_playlist_new(wchar_t* playlist_file_path, s_halo_playlist_container* playlist_container)
{
    csmemset(&playlist_container->playlist, 0, sizeof(s_halo_playlist));

    FILE* file_handle = _wfsopen(playlist_file_path, L"rt,ccs=UNICODE", _SH_DENYWR);

    int32 result = 3;
    
    c_halo_playlist_reader reader{};
    reader.playlist = &playlist_container->playlist;
    reader.match_count = 0;
    reader.section_buffer_current_index = 0;
    reader.reader_current_char_index = 0;
    reader.reader_current_mode = new_line;
    reader.current_section_type = _halo_playlist_header_none;
    reader.playlist_header_found = false;

    if(file_handle)
    {
        wchar_t buffer[16384]{};
        do
        {
            if(fgetws(buffer, 16384, file_handle))
            {
                reader.parse_file_section(buffer);
            }
            else
            {
                fflush(file_handle);
                return 3;
            }
        }
        while (!feof(file_handle));

        reader.finalize();

        result = playlist_container->playlist.match_variant_count != 0 ? 0 : 4;

        fflush(file_handle);
    }
    else
    {
        if (!(GetLastError() - 2))
            result = 1;
        if ((GetLastError() - 2) == 11)
            result = 2;
    }

    return result;
}

void c_halo_playlist_reader::parse_file_section(const wchar_t* file_buffer)
{
    while (*file_buffer)
	{
	    const wchar_t c = *file_buffer;
        bool consumed = false;

        switch (this->reader_current_mode)
    	{
	        case new_line: 
			{
	            if (c == L'\t' || c == L' ') 
	            {
	                consumed = true;
	            }
	            else if (c == L'\n')
	            {
	                ++this->reader_current_line;
	                consumed = true;
	            }
	            else if (c == L';') 
	            {
	                this->reader_current_mode = seek_to_next_line;
	                consumed = true;
	            }
	            else if (c == L'[')
	            {
	                this->reader_current_mode = header_start;
	                consumed = true;
	            }
	            else
	            {
	                this->buffer[this->section_buffer_current_index].unk = false;
	                this->reader_current_char_index = 0;
	                this->reader_current_mode = property_name_read;
	            }
	            break;
	        } 

	        case header_start: 
	        {
	            if (c == L'\t' || c == L' ') 
	            {
	                consumed = true;
	            }
	            else if (c == L']' || c == L'\n') 
	            {
	                this->evaluate_current_header();
	                this->reader_current_mode = seek_to_next_line;
	            }
	            else 
	            {
	                this->reader_current_char_index = 0;
	                this->reader_current_mode = header_read;
	            }
	            break;
	        } 

	        case header_read: 
	        {
	            if (c == L']' || c == L'\n') 
	            {
	                this->evaluate_current_header();
	                if (c == L'\n') 
	                {
	                    this->reader_current_mode = new_line;
	                    ++this->reader_current_line;
	                }
	                else 
	                {
	                    this->reader_current_mode = seek_to_next_line;
	                }
	            }
	            else 
				{
	                uint32 char_index = this->reader_current_char_index;
	                if (char_index >= 31)
	                {
	                    this->header_buffer[char_index] = 0;
	                    this->error(_halo_playlist_error_header_name_invalid, this->reader_current_line, this->header_buffer, L"", L"");
	                    this->reader_current_mode = seek_to_next_line;
	                }
	                else 
	                {
	                    this->header_buffer[char_index] = c;
	                    ++this->reader_current_char_index;
	                }
	                consumed = true;
	            }
	            break;
	        } 

	        case property_name_read: 
	        {
	            if (c == L'\n')
	            {
	                this->trim_property_name();
	                this->reader_current_mode = property_deliminator_scan;
	            }
	            else if (c == L'=')
	            {
	                this->trim_property_name();
	                this->reader_current_mode = property_deliminator_scan;
	                consumed = true;
	            }
	            else 
	            {
	                uint32 section_index = this->section_buffer_current_index;
	                uint32 char_index = this->reader_current_char_index;
	                if (char_index >= 31) 
	                {
	                    this->buffer[section_index].name_buffer[char_index] = 0;
	                    this->error(_halo_playlist_error_property_name_invalid, this->reader_current_line, this->buffer[section_index].name_buffer, L"", L"");
	                    this->reader_current_mode = seek_to_next_line;
	                }
	                else 
	                {
	                    this->buffer[section_index].name_buffer[char_index] = c;
	                    ++this->reader_current_char_index;
	                }
	                consumed = true;
	            }
	            break;
	        } 

	        case property_deliminator_scan:
	        {
	            if (c == L'\t' || c == L' ')
	            {
	                consumed = true;
	            }
	            else 
	            {
	                this->reader_current_char_index = 0;
	                this->reader_current_mode = property_value_read;
	            }
	            break;
	        } 

	        case property_value_read: 
	        {
	            if (c == L'\n') 
	            {
	                this->process_current_property();
	                this->reader_current_mode = new_line;
	                ++this->reader_current_line;
	                consumed = true;
	            }
	            else 
	            {
	                uint32 section_index = this->section_buffer_current_index;
	                uint32 char_index = this->reader_current_char_index;
	                if (char_index >= 31) 
	                {
	                    this->buffer[section_index].value_buffer[char_index] = 0;
	                    this->error(_halo_playlist_error_property_value_invalid, this->reader_current_line, this->buffer[section_index].name_buffer, this->buffer[section_index].value_buffer, L"");
	                    this->reader_current_mode = seek_to_next_line;
	                }
	                else 
	                {
	                    this->buffer[section_index].value_buffer[char_index] = c;
	                    ++this->reader_current_char_index;
	                }
	                consumed = true;
	            }
	            break;
	        } 

	        case seek_to_next_line: 
	        {
	            if (c == L'\n')
	            {
	                this->reader_current_mode = new_line;
	                ++this->reader_current_line;
	            }
	            consumed = true;
	            break;
	        } 

			default:
			{
				consumed = true;
				break;
			}
        }

        if (consumed) 
		{
            ++file_buffer;
        }
    }
    //INVOKE_TYPE(0, 0x12E4F, void(__thiscall*)(c_halo_playlist_reader*, wchar_t*), this, file_buffer);
}

void c_halo_playlist_reader::evaluate_current_header()
{
	this->header_buffer[this->reader_current_char_index] = L'\0';

	if (this->current_section_type != _halo_playlist_header_none)
		this->process_current_header();

	this->m_current_header_file_line = this->reader_current_line;

	e_halo_playlist_header_type header_type = halo_playlist_item_collection_get_header_type(this->header_buffer);
	this->current_section_type = header_type;
	this->section_buffer_current_index = 0;

	switch(header_type)
	{
		case _halo_playlist_header_playlist:
		{
			if(!this->playlist_header_found)
			{
				this->playlist_header_found = true;
			}
			else
			{
				this->error(_halo_playlist_error_playlist_header_already_defined, this->reader_current_line, L"", L"", L"");
			}
			break;
		}
		case _halo_playlist_header_variant:
		{
			if(this->playlist->variant_count >= 100)
			{
				this->error(16, this->reader_current_line, L"", L"", L"");
				this->current_section_type = _halo_playlist_header_none;
			}
			break;
		}
		case _halo_playlist_header_match:
		{
			if(this->match_count < 100)
			{
				csmemset(&this->matches[this->match_count], 0, sizeof(s_halo_playlist_match));
				this->matches[this->match_count].map_line_in_file = this->reader_current_line;
			}
			else
			{
				this->error(17, this->reader_current_line, L"", L"", L"");
				this->current_section_type = _halo_playlist_header_none;
			}
			break;
		}
		case _halo_playlist_header_none:
		{
			break;
		}
		default:
			this->error(_halo_playlist_error_header_name_invalid, this->reader_current_line, L"", L"", L"");
	}

    //INVOKE_TYPE(0, 0x12D14, void(__thiscall*)(c_halo_playlist_reader*), this);
}

void c_halo_playlist_reader::process_current_header()
{
	switch(this->current_section_type)
	{
		case _halo_playlist_header_variant:
			this->process_variant_header();
			break;
		case _halo_playlist_header_match:
			this->process_match_header();
			break;
	}
	//INVOKE_TYPE(0, 0x12968, void(__thiscall*)(c_halo_playlist_reader*), this);
}

void c_halo_playlist_reader::process_variant_header()
{
	INVOKE_TYPE(0, 0x12284, void(__thiscall*)(c_halo_playlist_reader*), this);
}

void c_halo_playlist_reader::process_match_header()
{
	s_halo_playlist_match* match = &this->matches[this->match_count];

	bool valid = true;

	if(!wcscmp(match->map, L""))
	{
		wchar_t* map_string = halo_playlist_item_collection_match_property_get_name(_halo_playlist_match_property_type_map);
		this->error(_halo_playlist_error_match_property_invalid, match->map_line_in_file, map_string, L"", L"");

		valid = false;
	}

	if(!wcscmp(match->variant, L""))
	{
		wchar_t* variant_string = halo_playlist_item_collection_match_property_get_name(_halo_playlist_match_property_type_variant);
		this->error(_halo_playlist_error_match_property_invalid, match->variant_line_in_file, variant_string, L"", L"");

		valid = false;
	}

	if(valid)
	{
		if (!match->weight)
			match->weight = 100;

		if (!match->minimum_players)
			match->minimum_players = 0;
		if (!match->maximum_players)
			match->maximum_players = 16;

		++this->match_count;
	}
}

void c_halo_playlist_reader::process_current_property()
{
    INVOKE_TYPE(0, 0x10FBE, void(__thiscall*)(c_halo_playlist_reader*), this);
}

void c_halo_playlist_reader::trim_property_name()
{
    INVOKE_TYPE(0, 0xEBE2, void(__thiscall*)(c_halo_playlist_reader*), this);
}

void c_halo_playlist_reader::error(int32 error_type, uint32 file_line, wchar_t* property_name, wchar_t* property_value,	wchar_t* extra)
{
    INVOKE_TYPE(0, 0xED2E, void(__thiscall*)(c_halo_playlist_reader*, int32, uint32, wchar_t*, wchar_t*, wchar_t*),
        this, error_type, file_line, property_name, property_value, extra);
}

void c_halo_playlist_reader::finalize()
{
    INVOKE_TYPE(0, 0x12AC7, void(__thiscall*)(c_halo_playlist_reader*), this);
}


void halo_playlist_apply_patches()
{
    DETOUR_ATTACH(p_halo_playlist_new, Memory::GetAddress<t_halo_playlist_new>(0, 0x1321D), halo_playlist_new);
}
