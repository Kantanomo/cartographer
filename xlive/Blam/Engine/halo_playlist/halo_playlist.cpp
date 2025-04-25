#include "stdafx.h"
#include "halo_playlist.h"

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
	                this->process_current_header();
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
	                this->process_current_header();
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
	                    this->error(3, this->reader_current_line, this->header_buffer, L"", L"");
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
	                    this->error(2, this->reader_current_line, this->buffer[section_index].name_buffer, L"", L"");
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
	                    this->error(4, this->reader_current_line, this->buffer[section_index].name_buffer, this->buffer[section_index].value_buffer, L"");
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

void c_halo_playlist_reader::process_current_header()
{
    INVOKE_TYPE(0, 0x12D14, void(__thiscall*)(c_halo_playlist_reader*), this);
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
