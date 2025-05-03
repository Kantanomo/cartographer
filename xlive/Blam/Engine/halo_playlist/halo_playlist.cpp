#include "stdafx.h"
#include "halo_playlist.h"

#include "definitions/halo_playlist_assault_property.h"
#include "definitions/halo_playlist_ctf_property.h"
#include "definitions/halo_playlist_equipment_property.h"
#include "definitions/halo_playlist_game_engine_flags.h"
#include "definitions/halo_playlist_game_type_property.h"
#include "definitions/halo_playlist_headhunter_property.h"
#include "definitions/halo_playlist_juggernaut_property.h"
#include "definitions/halo_playlist_king_property.h"
#include "definitions/halo_playlist_match_property.h"
#include "definitions/halo_playlist_match_settings_property.h"
#include "definitions/halo_playlist_max_heads_carried.h"
#include "definitions/halo_playlist_moving_hill.h"
#include "definitions/halo_playlist_oddball_property.h"
#include "definitions/halo_playlist_player_property.h"
#include "definitions/halo_playlist_player_speed.h"
#include "definitions/halo_playlist_playlist_property.h"
#include "definitions/halo_playlist_score_to_win_round.h"
#include "definitions/halo_playlist_slayer_property.h"
#include "definitions/halo_playlist_team_property.h"
#include "definitions/halo_playlist_team_scoring.h"
#include "definitions/halo_playlist_territories_property.h"
#include "definitions/halo_playlist_variant_property.h"
#include "definitions/halo_playlist_vehicle_property.h"
#include "text/unicode.h"

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
            else if(!feof(file_handle))
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
                    this->buffer[this->section_buffer_current_index].processed = false;
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
                        this->error(_halo_playlist_error_header_name_invalid, this->reader_current_line, this->header_buffer);
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
                        this->error(_halo_playlist_error_property_name_invalid, this->reader_current_line, this->buffer[section_index].name_buffer);
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
                        this->error(_halo_playlist_error_property_value_invalid, this->reader_current_line, this->buffer[section_index].name_buffer, this->buffer[section_index].value_buffer);
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
                this->error(_halo_playlist_error_playlist_header_already_defined, this->reader_current_line);
            }
            break;
        }
        case _halo_playlist_header_variant:
        {
            if(this->playlist->variant_count >= 100)
            {
                this->error(_halo_playlist_error_max_variants, this->reader_current_line);
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
                this->error(_halo_playlist_error_max_matches, this->reader_current_line);
                this->current_section_type = _halo_playlist_header_none;
            }
            break;
        }
        case _halo_playlist_header_none:
        {
            break;
        }
        default:
            this->error(_halo_playlist_error_header_name_invalid, this->reader_current_line);
    }

    //INVOKE_TYPE(0, 0x12D14, void(__thiscall*)(c_halo_playlist_reader*), this);
}

void c_halo_playlist_reader::process_current_header()
{
    switch(this->current_section_type)
    {
        case _halo_playlist_header_variant:
            this->process_variant_section();
            break;
        case _halo_playlist_header_match:
            this->process_match_section();
            break;
    }
    //INVOKE_TYPE(0, 0x12968, void(__thiscall*)(c_halo_playlist_reader*), this);
}

void c_halo_playlist_reader::process_variant_section()
{
    bool invalid = false;
    if(this->section_buffer_current_index == 0)
    {
        this->error(_halo_playlist_error_variant_name_not_found, this->m_current_header_file_line);
        invalid = true;
    }
    else
    {
        bool variant_name_found = false;
        wchar_t variant_name[32] {};
        uint32 variant_file_line = 0;

        bool game_type_found = false;
        e_game_variant_description_index game_type = k_game_variant_description_invalid;
        uint32 game_type_line = 0;

        bool base_variant_found = false;
        wchar_t base_variant_name[32]{};
        s_game_variant* base_game_variant {};
        uint32 base_variant_file_line = 0;

        bool valid_variant_setup = false;

        for(uint32 i = 0; i < this->section_buffer_current_index; ++i)
        {
            s_halo_playlist_section_line* file_section = &this->buffer[i];

            e_halo_playlist_variant_property_type variant_property_type = halo_playlist_item_collection_get_variant_property_type(file_section->name_buffer);

            switch(variant_property_type)
            {
                case _halo_playlist_variant_property_name:
                {
                    variant_name_found = true;
                    variant_file_line = file_section->file_line;
                    if (!wcscmp(file_section->value_buffer, L"") ||
                        wcsncpy_s(variant_name, NUMBEROF(variant_name), file_section->value_buffer, NONE))
                    {
                        this->error(_halo_playlist_error_property_value_invalid, variant_file_line, file_section->name_buffer, file_section->value_buffer);
                    }

                    file_section->processed = true;
                    break;
                }
                case _halo_playlist_variant_property_base_variant:
                {
                    base_variant_found = true;
                    base_variant_file_line = file_section->file_line;
                    if(!wcscmp(file_section->value_buffer, L""))
                    {
                        this->error(_halo_playlist_error_property_value_invalid, base_variant_file_line, file_section->name_buffer, file_section->value_buffer);
                        valid_variant_setup = game_type_found == false;

                        if (!valid_variant_setup)
                            this->error(_halo_playlist_error_variant_base_variant_and_game_type_both_set, file_section->file_line);
                    }
                    else
                    {
                        wcsncpy_s(base_variant_name, 32, file_section->value_buffer, -1);
                        base_game_variant = get_default_game_variant_by_name(base_variant_name);

                        if (!base_game_variant)
                            this->error(_halo_playlist_error_variant_invalid, base_variant_file_line, file_section->name_buffer);

                        valid_variant_setup = game_type_found == false;

                        if (!valid_variant_setup)
                            this->error(_halo_playlist_error_variant_base_variant_and_game_type_both_set, file_section->file_line);
                    }

                    file_section->processed = true;
                    break;
                }
                case _halo_playlist_variant_property_game_type:
                {
                    game_type_found = true;
                    game_type_line = file_section->file_line;
                    game_type = halo_playlist_item_collection_game_type_get_value(file_section->value_buffer);

                    if (game_type < _game_variant_description_slayer)
                        this->error(_halo_playlist_error_property_value_invalid, game_type_line, file_section->name_buffer, file_section->value_buffer);

                    valid_variant_setup = base_variant_found == false;

                    if (!valid_variant_setup)
                        this->error(_halo_playlist_error_variant_base_variant_and_game_type_both_set, file_section->file_line);

                    file_section->processed = true;
                    break;
                }
            }
        }


        if(!variant_name_found)
        {
            this->error(_halo_playlist_error_variant_name_not_found, this->m_current_header_file_line);
            invalid = true;
        }

        if(!base_variant_found && !game_type_found)
        {
            this->error(_halo_playlist_error_variant_base_or_game_type_not_found, this->m_current_header_file_line);
            invalid = true;
        }

        if(!validate_wchar_characters(variant_name) || uniswcntrl(variant_name))
        {
            this->error(_halo_playlist_error_variant_name_invalid, variant_file_line);
            invalid = true;
        }

        if(wcsstr(variant_name, L"|"))
        {
            this->error(_halo_playlist_error_variant_name_illegal_character, variant_file_line);
            invalid = true;
        }

        if(!invalid
            && wcscmp(variant_name, L"")
            && (!base_variant_found || base_game_variant)
            && (!game_type_found || game_type >= _game_variant_description_slayer)
            )
        {

            s_game_variant new_variant{};

            if(this->playlist->variant_count == 0)
            {
                if (base_variant_found)
                    csmemcpy(&new_variant, base_game_variant, sizeof(s_game_variant));
                else
                    game_variant_create_default_new(&new_variant, game_type);
            }
            else
            {
                bool variant_name_exists = false;
                for(int32 i = 0; i < this->playlist->variant_count; ++i)
                {
                    if(_wcsicmp(this->playlist->variants[i].variant_name, variant_name) == 0)
                    {
                        variant_name_exists = true;
                        break;
                    }
                }
                if(variant_name_exists)
                {
                    this->error(_halo_playlist_error_duplicate_variant_found, variant_file_line, variant_name);
                    return;
                }

                if (base_variant_found)
                    csmemcpy(&new_variant, base_game_variant, sizeof(s_game_variant));
                else
                    game_variant_create_default_new(&new_variant, game_type);
            }

            new_variant.flags &= ~1u;
            wcsncpy_s(new_variant.variant_name, 32, variant_name, -1);

            e_game_variant_description_index variant_description_index = _game_variant_description_slayer;
            switch(new_variant.variant_game_engine_index)
            {
                case _game_engine_type_ctf:
                    variant_description_index = _game_variant_description_ctf;
                    break;
                case _game_engine_type_slayer:
                    variant_description_index = _game_variant_description_slayer;
                    break;
                case _game_engine_type_oddball:
                    variant_description_index = _game_variant_description_oddball;
                    break;
                case _game_engine_type_koth:
                    variant_description_index = _game_variant_description_king;
                    break;
                case _game_engine_type_race:
                    variant_description_index = k_game_variant_description_invalid;
                    break;
                case _game_engine_type_headhunter:
                    variant_description_index = _game_variant_description_headhunter;
                    break;
                case _game_engine_type_juggernaut:
                    variant_description_index = _game_variant_description_juggernaut;
                    break;
                case _game_engine_type_territories:
                    variant_description_index = _game_variant_description_territories;
                    break;
                case _game_engine_type_assault:
                    variant_description_index = _game_variant_description_invasion;
                    break;
                default:
                {
                    this->error(_halo_playlist_error_variant_invalid, this->reader_current_line, variant_name, base_variant_name);
                    break;
                }
            }

            for(uint32 i = 0; i < this->section_buffer_current_index; ++i)
            {
                s_halo_playlist_section_line* file_section = &this->buffer[i];

                if (file_section->processed)
                    continue;

                file_section->processed = true;

                bool base_settings_check =
                    this->process_variant_match_setting(file_section, &new_variant)   ||
                    this->process_variant_player_setting(file_section, &new_variant)  ||
                    this->process_variant_team_setting(file_section, &new_variant)	  ||
                    this->process_variant_vehicle_setting(file_section, &new_variant) ||
                    this->process_variant_equipment_setting(file_section, &new_variant);


                if(!base_settings_check)
                {
                    bool section_processed = false;

                    switch(variant_description_index)
                    {
                        case _game_variant_description_slayer:
                            section_processed = this->process_variant_slayer_setting(file_section, &new_variant);
                            break;
                        case _game_variant_description_oddball:
                            section_processed = this->process_variant_oddball_setting(file_section, &new_variant);
                            break;
                        case _game_variant_description_juggernaut:
                            section_processed = this->process_variant_juggernaut_setting(file_section, &new_variant);
                            break;
                        case _game_variant_description_king:
                            section_processed = this->process_variant_king_setting(file_section, &new_variant);
                            break;
                        case _game_variant_description_ctf:
                            section_processed = this->process_variant_ctf_setting(file_section, &new_variant);
                            break;
                        case _game_variant_description_invasion:
                            section_processed = this->process_variant_assault_setting(file_section, &new_variant);
                            break;
                        case _game_variant_description_territories:
                            section_processed = this->process_variant_territories_setting(file_section, &new_variant);
                            break;
                        case _game_variant_description_headhunter:
                            section_processed = this->process_variant_headhunter_setting(file_section, &new_variant);
                            break;
                    }

                    if(!section_processed)
                    {
                        wchar_t* variant_type_name = halo_playlist_item_collection_game_type_get_name(variant_description_index);
                        this->error(_halo_playlist_error_variant_setting_invalid, file_section->file_line, file_section->name_buffer, variant_type_name);
                    }
                }
            }

            csmemcpy(&this->playlist->variants[this->playlist->variant_count], &new_variant, sizeof(s_game_variant));
            ++this->playlist->variant_count;
        }
    }

    //INVOKE_TYPE(0, 0x12284, void(__thiscall*)(c_halo_playlist_reader*), this);
}

bool c_halo_playlist_reader::process_variant_match_setting(s_halo_playlist_section_line* section_item, s_game_variant* variant)
{
    return INVOKE_TYPE(0, 0x1159C, bool(__thiscall*)(c_halo_playlist_reader*, wchar_t*, wchar_t*, uint32, s_game_variant*),
        this, section_item->name_buffer, section_item->value_buffer, section_item->file_line, variant);
}

bool c_halo_playlist_reader::process_variant_player_setting(s_halo_playlist_section_line* section_item,	s_game_variant* variant)
{
    return INVOKE_TYPE(0, 0x11658, bool(__thiscall*)(c_halo_playlist_reader*, wchar_t*, wchar_t*, uint32, s_game_variant*),
        this, section_item->name_buffer, section_item->value_buffer, section_item->file_line, variant);
}

bool c_halo_playlist_reader::process_variant_team_setting(s_halo_playlist_section_line* section_item, s_game_variant* variant)
{
    return INVOKE_TYPE(0, 0x11768, bool(__thiscall*)(c_halo_playlist_reader*, wchar_t*, wchar_t*, uint32, s_game_variant*),
        this, section_item->name_buffer, section_item->value_buffer, section_item->file_line, variant);
}

bool c_halo_playlist_reader::process_variant_vehicle_setting(s_halo_playlist_section_line* section_item, s_game_variant* variant)
{
    return INVOKE_TYPE(0, 0x11816, bool(__thiscall*)(c_halo_playlist_reader*, wchar_t*, wchar_t*, uint32, s_game_variant*),
        this, section_item->name_buffer, section_item->value_buffer, section_item->file_line, variant);
}

bool c_halo_playlist_reader::process_variant_equipment_setting(s_halo_playlist_section_line* section_item, s_game_variant* variant)
{
    return INVOKE_TYPE(0, 0x1190c, bool(__thiscall*)(c_halo_playlist_reader*, wchar_t*, wchar_t*, uint32, s_game_variant*),
        this, section_item->name_buffer, section_item->value_buffer, section_item->file_line, variant);
}

bool c_halo_playlist_reader::process_variant_slayer_setting(s_halo_playlist_section_line* section_item, s_game_variant* variant)
{
    return INVOKE_TYPE(0, 0x11A08, bool(__thiscall*)(c_halo_playlist_reader*, wchar_t*, wchar_t*, uint32, s_game_variant*),
        this, section_item->name_buffer, section_item->value_buffer, section_item->file_line, variant);
}

bool c_halo_playlist_reader::process_variant_oddball_setting(s_halo_playlist_section_line* section_item, s_game_variant* variant)
{
    return INVOKE_TYPE(0, 0x11C08, bool(__thiscall*)(c_halo_playlist_reader*, wchar_t*, wchar_t*, uint32, s_game_variant*),
        this, section_item->name_buffer, section_item->value_buffer, section_item->file_line, variant);
}

bool c_halo_playlist_reader::process_variant_juggernaut_setting(s_halo_playlist_section_line* section_item, s_game_variant* variant)
{
    return INVOKE_TYPE(0, 0x11D80, bool(__thiscall*)(c_halo_playlist_reader*, wchar_t*, wchar_t*, uint32, s_game_variant*),
        this, section_item->name_buffer, section_item->value_buffer, section_item->file_line, variant);
}

bool c_halo_playlist_reader::process_variant_king_setting(s_halo_playlist_section_line* section_item, s_game_variant* variant)
{
    return INVOKE_TYPE(0, 0x11AF0, bool(__thiscall*)(c_halo_playlist_reader*, wchar_t*, wchar_t*, uint32, s_game_variant*),
        this, section_item->name_buffer, section_item->value_buffer, section_item->file_line, variant);
}

bool c_halo_playlist_reader::process_variant_ctf_setting(s_halo_playlist_section_line* section_item, s_game_variant* variant)
{
    return INVOKE_TYPE(0, 0x11E54, bool(__thiscall*)(c_halo_playlist_reader*, wchar_t*, wchar_t*, uint32, s_game_variant*),
        this, section_item->name_buffer, section_item->value_buffer, section_item->file_line, variant);
}

bool c_halo_playlist_reader::process_variant_assault_setting(s_halo_playlist_section_line* section_item, s_game_variant* variant)
{
    return INVOKE_TYPE(0, 0x12008, bool(__thiscall*)(c_halo_playlist_reader*, wchar_t*, wchar_t*, uint32, s_game_variant*),
        this, section_item->name_buffer, section_item->value_buffer, section_item->file_line, variant);
}

bool c_halo_playlist_reader::process_variant_territories_setting(s_halo_playlist_section_line* section_item, s_game_variant* variant)
{
    return INVOKE_TYPE(0, 0x121A8, bool(__thiscall*)(c_halo_playlist_reader*, wchar_t*, wchar_t*, uint32, s_game_variant*),
        this, section_item->name_buffer, section_item->value_buffer, section_item->file_line, variant);
}

bool c_halo_playlist_reader::process_variant_headhunter_setting(s_halo_playlist_section_line* section_item,	s_game_variant* variant)
{
    const e_halo_playlist_headhunter_property headhunter_property = halo_playlist_item_collection_headhunter_property_get_value(section_item->name_buffer);

    if (headhunter_property == k_halo_playlist_headhunter_property_invalid)
        return false;

    bool property_result = false;

    switch(headhunter_property)
    {
        case _halo_playlist_headhunter_property_score_to_win:
            property_result = halo_playlist_item_collection_score_to_win_round_write_to_variant(section_item->value_buffer, variant);
            break;
        case _halo_playlist_headhunter_property_team_play:
            property_result = halo_playlist_item_collection_team_play_write_to_variant(section_item->value_buffer, variant);
            break;
        case _halo_playlist_headhunter_property_team_scoring:
            property_result = halo_playlist_item_collection_team_scoring_write_to_variant(section_item->value_buffer, variant);
            break;
        case _halo_playlist_headhunter_property_team_changing:
            property_result = halo_playlist_item_collection_team_changing_write_to_variant(section_item->value_buffer, variant);
            break;
        case _halo_playlist_headhunter_property_force_even_teams:
            property_result = halo_playlist_item_collection_force_even_teams_write_to_variant(section_item->value_buffer, variant);
            break;
        case _halo_playlist_headhunter_property_uncontested_hill:
            property_result = halo_playlist_item_collection_headhunter_uncontested_hill_write_to_variant(section_item->value_buffer, variant);
            break;
        case _halo_playlist_headhunter_property_moving_hill:
            property_result = halo_playlist_item_collection_headhunter_hill_move_time_write_to_variant(section_item->value_buffer, variant);
            break;
        case _halo_playlist_headhunter_property_max_heads_carried:
            property_result = halo_playlist_item_collection_max_heads_carried_write_to_variant(section_item->value_buffer, variant);
            break;
        case _halo_playlist_headhunter_property_speed_with_heads:
            property_result = halo_playlist_item_collection_headhunter_speed_with_heads_write_to_variant(section_item->value_buffer, variant);
            break;
    }

    if (!property_result)
        this->error(_halo_playlist_error_property_value_invalid, section_item->file_line, section_item->name_buffer, section_item->value_buffer);

    return property_result;
}

void c_halo_playlist_reader::process_match_section()
{
    s_halo_playlist_match* match = &this->matches[this->match_count];

    bool valid = true;

    if(!wcscmp(match->map, L""))
    {
        wchar_t* map_string = halo_playlist_item_collection_match_property_get_name(_halo_playlist_match_property_type_map);
        this->error(_halo_playlist_error_match_property_invalid, match->map_line_in_file, map_string);

        valid = false;
    }

    if(!wcscmp(match->variant, L""))
    {
        wchar_t* variant_string = halo_playlist_item_collection_match_property_get_name(_halo_playlist_match_property_type_variant);
        this->error(_halo_playlist_error_match_property_invalid, match->variant_line_in_file, variant_string);

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
    s_halo_playlist_section_line* section = &this->buffer[this->section_buffer_current_index];

    section->file_line = this->reader_current_line;
    for(uint32 i = this->reader_current_char_index; i > 0; --i)
    {
        if (section->value_buffer[i] == L' ' || section->value_buffer[i] == L'\t')
            section->value_buffer[i] = L'\0';
        else
            break;
    }

    switch(this->current_section_type)
    {
        case _halo_playlist_header_playlist:
        {
            e_halo_playlist_playlist_property_type property_type = halo_playlist_item_collection_playlist_property_get_value(section->name_buffer);

            if(property_type == k_halo_playlist_playlist_property_invalid)
            {
                this->error(_halo_playlist_error_property_name_invalid, this->reader_current_line, section->name_buffer);
                return;
            }

            switch(property_type)
            {
                case _halo_playlist_playlist_property_shuffle:
                {
                    if (!halo_playlist_item_collection_get_boolean_value(section->value_buffer, &this->playlist->shuffle))
                        this->error(_halo_playlist_error_property_value_invalid, this->reader_current_line, section->name_buffer, section->value_buffer);

                    return;
                }
                case _halo_playlist_playlist_property_postgame_delay:
                {
                    int32 duration = halo_playlist_item_collection_get_int_time_value(section->value_buffer);

                    if (duration < 0)
                    {
                        this->error(_halo_playlist_error_property_value_invalid, this->reader_current_line, section->name_buffer, section->value_buffer);
                        return;
                    }

                    this->playlist->postgame_delay = duration;
                    return;
                }
                case _halo_playlist_playlist_property_pregame_delay:
                {
                    int32 duration = halo_playlist_item_collection_get_int_time_value(section->value_buffer);

                    if (duration < 0)
                    {
                        this->error(_halo_playlist_error_property_value_invalid, this->reader_current_line, section->name_buffer, section->value_buffer);
                        return;
                    }

                    this->playlist->pregame_delay = duration;
                    return;
                }
                case _halo_playlist_playlist_property_pregame_team_selection_delay:
                {
                    int32 duration = halo_playlist_item_collection_get_int_time_value(section->value_buffer);

                    if (duration < 0)
                    {
                        this->error(_halo_playlist_error_property_value_invalid, this->reader_current_line, section->name_buffer, section->value_buffer);
                        return;
                    }

                    this->playlist->pregame_team_selection_delay = duration;
                    return;
                }
            }
            break;
        }
        case _halo_playlist_header_variant:
        {
            bool section_name_exists = false;
            for(uint32 i = 0; i < this->section_buffer_current_index; ++i)
            {
                if(!wcscmp(section->name_buffer, L""))
                {
                    this->error(_halo_playlist_error_property_already_defined, this->reader_current_line, section->name_buffer);
                    section_name_exists = true;
                    break;
                }
            }

            if(!section_name_exists)
            {
	            if(this->property_name_is_valid(section->name_buffer))
	            {
                    ++this->section_buffer_current_index;
	            }
                else
                {
                    this->error(_halo_playlist_error_property_name_invalid, this->reader_current_line, section->name_buffer);
                }
            }
            break;
        }
        case _halo_playlist_header_match:

            e_halo_playlist_match_property_type property = halo_playlist_item_collection_get_match_property_get_value(section->name_buffer);

			if(property == k_halo_playlist_match_property_invalid)
			{
                this->error(_halo_playlist_error_property_name_invalid, this->reader_current_line, section->name_buffer);
                return;
			}

            s_halo_playlist_match* match = &this->matches[this->match_count];

			switch(property)
			{
	            case _halo_playlist_match_property_type_variant:
	            {
                    if(match->variant[0] == L'\0')
                    {
	                    if(section->value_buffer[0] != L'\0' && wcsncpy_s(match->variant, NUMBEROF(match->variant), section->value_buffer, NONE))
	                    {
                            match->variant_line_in_file = this->reader_current_line;
	                    }
                        else
                        {
                            this->error(_halo_playlist_error_property_value_invalid, this->reader_current_line, section->name_buffer, section->value_buffer);
                        }
                    }
                    else
                    {
                        this->error(_halo_playlist_error_property_already_defined, this->reader_current_line, section->name_buffer);
                        this->error(_halo_playlist_error_property_value_invalid, this->reader_current_line, section->name_buffer, section->value_buffer);
                    }
	                break;
	            }
				case _halo_playlist_match_property_type_map:
                {
                    if(match->map[0] == L'\0')
                    {
	                    if(section->value_buffer[0] != L'\0' && wcsncpy_s(match->map, NUMBEROF(match->map), section->value_buffer, NONE))
	                    {
                            match->map_line_in_file = this->reader_current_line;
	                    }
                        else
                        {
                            this->error(_halo_playlist_error_property_value_invalid, this->reader_current_line, section->name_buffer, section->value_buffer);
                        }
                    }
                    else
                    {
                        this->error(_halo_playlist_error_property_already_defined, this->reader_current_line, section->name_buffer);
                        this->error(_halo_playlist_error_property_value_invalid, this->reader_current_line, section->name_buffer, section->value_buffer);
                    }
                    break;
                }
				case _halo_playlist_match_property_type_weight:
                {
                    if(match->weight == 0)
                    {
                        int32 value = halo_playlist_item_collection_get_int_value(section->value_buffer);
                        if(value != NONE)
                        {
                            match->weight = value;
                        }
                        else
                        {
                            this->error(_halo_playlist_error_property_value_invalid, this->reader_current_line, section->name_buffer, section->value_buffer);
                        }
                    }
                    else
                    {
                        this->error(_halo_playlist_error_property_already_defined, this->reader_current_line, section->name_buffer);
                        this->error(_halo_playlist_error_property_value_invalid, this->reader_current_line, section->name_buffer, section->value_buffer);
                    }
                    break;
                }
				case _halo_playlist_match_property_type_minimum_players:
                {
                    if (match->minimum_players == 0)
                    {
                        int16 value = halo_playlist_item_collection_player_count_get_value(section->value_buffer);
                        if (value != NONE)
                        {
                            match->minimum_players = value;
                        }
                        else
                        {
                            this->error(_halo_playlist_error_property_value_invalid, this->reader_current_line, section->name_buffer, section->value_buffer);
                        }
                    }
                    else
                    {
                        this->error(_halo_playlist_error_property_already_defined, this->reader_current_line, section->name_buffer);
                        this->error(_halo_playlist_error_property_value_invalid, this->reader_current_line, section->name_buffer, section->value_buffer);
                    }
                    break;
                }
				case _halo_playlist_match_property_type_maximum_players:
                {
                    if (match->maximum_players == 0)
                    {
                        int16 value = halo_playlist_item_collection_player_count_get_value(section->value_buffer);
                        if (value != NONE)
                        {
                            match->maximum_players = value;
                        }
                        else
                        {
                            this->error(_halo_playlist_error_property_value_invalid, this->reader_current_line, section->name_buffer, section->value_buffer);
                        }
                    }
                    else
                    {
                        this->error(_halo_playlist_error_property_already_defined, this->reader_current_line, section->name_buffer);
                        this->error(_halo_playlist_error_property_value_invalid, this->reader_current_line, section->name_buffer, section->value_buffer);
                    }
                    break;
                }
			}

            break;
    }

    //INVOKE_TYPE(0, 0x10FBE, void(__thiscall*)(c_halo_playlist_reader*), this);
}

bool c_halo_playlist_reader::property_name_is_valid(wchar_t* property_name)
{
    if (halo_playlist_item_collection_get_variant_property_type(property_name) != k_halo_playlist_variant_property_invalid)
        return true;

    if (halo_playlist_item_collection_match_settings_property_get_value(property_name) != k_halo_playlist_match_property_invalid)
        return true;

    if (halo_playlist_item_collection_player_property_get_value(property_name) != k_halo_playlist_player_property_invalid)
        return true;

    if (halo_playlist_item_collection_team_property_get_value(property_name) != k_halo_playlist_team_property_invalid)
        return true;

    if (halo_playlist_item_collection_vehicle_property_get_value(property_name) != k_halo_playlist_vehicle_property_invalid)
        return true;

    if (halo_playlist_item_collection_equipment_property_get_value(property_name) != k_halo_playlist_equipment_property_invalid)
        return true;

    if (halo_playlist_item_collection_slayer_property_get_value(property_name) != k_halo_playlist_slayer_property_invalid)
        return true;

    if (halo_playlist_item_collection_king_property_get_value(property_name) != k_halo_playlist_king_property_invalid)
        return true;

    if (halo_playlist_item_collection_oddball_property_get_value(property_name) != k_halo_playlist_oddball_property_invalid)
        return true;

    if (halo_playlist_item_collection_juggernaut_property_get_value(property_name) != k_halo_playlist_juggernaut_property_invalid)
        return true;

    if (halo_playlist_item_collection_ctf_property_get_value(property_name) != k_halo_playlist_ctf_property_invalid)
        return true;

    if (halo_playlist_item_collection_assault_property_get_value(property_name) != k_halo_playlist_assault_property_invalid)
        return true;

    if (halo_playlist_item_collection_territories_property_get_value(property_name) != k_halo_playlist_territories_property_invalid)
        return true;

    if (halo_playlist_item_collection_headhunter_property_get_value(property_name) != k_halo_playlist_headhunter_property_invalid)
        return true;

    return false;
}

void c_halo_playlist_reader::trim_property_name()
{
    INVOKE_TYPE(0, 0xEBE2, void(__thiscall*)(c_halo_playlist_reader*), this);
}

void c_halo_playlist_reader::error(e_halo_playlist_error error_type, uint32 file_line, wchar_t* property_name, wchar_t* property_value,	wchar_t* extra)
{
    INVOKE_TYPE(0, 0xED2E, void(__thiscall*)(c_halo_playlist_reader*, e_halo_playlist_error, uint32, wchar_t*, wchar_t*, wchar_t*),
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
