#include "PlaylistLoader.h"
#include "Util/Hooks/Hook.h"
#include "H2MOD.h"
#include "H2MOD/Modules/Startup/Startup.h"

namespace PlaylistLoader
{


	bool ProcessCustomSetting(playlist_entry* playlist_entry)
	{
		//Section Type must be Variant
		if (playlist_entry->current_section_type != 1)
			return false;

		wchar_t* propertyName = (wchar_t*)calloc(64, 1);
		for (auto i = 0; i < 32; i++)
		{
			propertyName[i] = playlist_entry->section_buffer[68 * playlist_entry->section_buffer_current_index + i];
			if (propertyName[i] == 0) break;
		}
		wchar_t* propertyValue = (wchar_t*)calloc(64, 1);
		for (auto i = 0; i < playlist_entry->reader_current_char_index; i++)
		{
			propertyValue[i] = playlist_entry->section_buffer[68 * playlist_entry->section_buffer_current_index + 32 + i];
			if (propertyValue[i] == 0) break;
		}
		//wchar_t* propertyValue = playlist_entry->section_buffer[68 * playlist_entry->section_buffer_current_index + 32];
		
		if (_wcsicmp(propertyName, L"teSt") == 0)
		{
			//the custom setting does not need to be stored in the section buffer, so remove one from the index so it gets overwritten by the next base setting.
			playlist_entry->section_buffer_current_index--;
			wchar_t* tempName = (wchar_t*)calloc(64, 1);
			wchar_t* variant = (wchar_t*)calloc(64, 1);
			for (auto i = 0; i < playlist_entry->section_buffer_current_index; i++)
			{
				for (auto j = 0; j < 32; j++)
					tempName[j] = playlist_entry->section_buffer[68 * i + j];
				if (_wcsicmp(tempName, L"name") == 0)
				{
					for (auto j = 0; j < 32; j++)
						variant[j] = playlist_entry->section_buffer[68 * i + 32 + j];
					break;
				}
			}
			if (_wcsicmp(variant, L"") != 0)
			{
				LOG_INFO_GAME(L"[PlaylistLoader::ProcessCustomSetting] Processing Variant: {}", variant);
				LOG_INFO_GAME(L"[PlaylistLoader::ProcessCustomSetting] Custom Setting Detected: {} = {}", propertyName, propertyValue);
			}
			return true;
		}
		//LOG_INFO_GAME(L"[PlaylistLoader::ProcessCustomSetting] Base Setting Detected {} = {}", propertyName, propertyValue);
		return false;
	}


	void ProcessPlaylistHeaderHook(DWORD* playlist_entry, int a2)
	{
		__asm
		{
			mov ecx, playlist_entry
			mov ebx, a2
			mov eax, [0x12D14]
			add eax, [H2BaseAddr]
			call eax
		}
	}

	void  CurrentItemNameTrimEnd(playlist_entry *pThis)
	{
		int i; // eax
		int v2; // edx
		wchar_t v3; // si
		wchar_t *v4; // edx

		pThis->section_buffer[pThis->reader_current_char_index + 68 * pThis->section_buffer_current_index] = 0;
		for (i = pThis->reader_current_char_index - 1; i > 0; *v4 = 0)
		{
			v2 = i + 68 * pThis->section_buffer_current_index;
			v3 = pThis->section_buffer[v2];
			v4 = &pThis->section_buffer[v2];
			if (v3 != ' ' && v3 != '\t')
				break;
			--i;
		}
	}

	typedef void(__fastcall* h_playlist_processs_setting)( playlist_entry* playlist_entry);
	h_playlist_processs_setting p_playlist_process_setting;
	void __fastcall ProcessSetting(playlist_entry* playlist_entry)
	{
		p_playlist_process_setting(playlist_entry);
	}

	typedef void(__fastcall* h_playlist_read_line)(playlist_entry* playlist_entry, DWORD a1, wchar_t* stringBuffer);
	h_playlist_read_line p_playlist_read_line;

	void __fastcall ReadLine( playlist_entry* playlist_entry, DWORD a1, wchar_t* stringBuffer)
	{
		//p_playlist_read_line(pThis, playlist_entry, stringBuffer);
		while (*stringBuffer)
		{
			switch (playlist_entry->reader_current_mode)
			{
				case newLine:
					switch (*stringBuffer)
					{
						case '\t':
						case ' ':
							goto Go_to_next_char;
						case '\n':
							goto Go_to_next_line;
						case ';':
							playlist_entry->reader_current_mode = seekToNextLine;
							goto Go_to_next_char;
						case '[':
							playlist_entry->reader_current_mode = headerStart;
							goto Go_to_next_char;
						default:
							playlist_entry->section_buffer[68 * playlist_entry->section_buffer_current_index + 66 + 2] = 0;
							playlist_entry->reader_current_mode = propertyNameRead;
							playlist_entry->reader_current_char_index = 0;
							break;
					}
					break;
				case headerStart:
					switch (*stringBuffer)
					{
						case '\t':
						case ' ':
							goto Go_to_next_char;
						case '\n':
						case ']':
							playlist_entry->reader_current_char_index = 0;
							goto process_header;
						default:
							playlist_entry->reader_current_char_index = 0;
							playlist_entry->reader_current_mode = headerRead;
							break;
					}
					break;
				case headerRead:
					if (*stringBuffer != '\n' && *stringBuffer != ']')
					{
						if (playlist_entry->reader_current_char_index >= 31)
						{
							playlist_entry->header_buffer[playlist_entry->reader_current_char_index] = 0;
							/*playlist_item_invalid_unk(
								playlist_entry,
								0,
								3,
								playlist_entry->reader_current_line,
								playlist_entry->header_buffer,
								byte_752CA8,
								byte_752CA8);*/
							playlist_entry->reader_current_mode = seekToNextLine;
						}
						else
						{
							playlist_entry->header_buffer[playlist_entry->reader_current_char_index] = *stringBuffer;
							++playlist_entry->reader_current_char_index;
						}
						goto Go_to_next_char;
					}
				process_header:
					ProcessPlaylistHeaderHook((DWORD*)playlist_entry, 0);
					playlist_entry->reader_current_mode = seekToNextLine;
					break;
				case propertyNameRead:
					if (*stringBuffer != '\n')
					{
						if (*stringBuffer == '=')
						{
							CurrentItemNameTrimEnd(playlist_entry);
							playlist_entry->reader_current_mode = propertyDeliminatorScan;
						}
						else
						{
							if (playlist_entry->reader_current_char_index >= 31)
							{
								playlist_entry->section_buffer[playlist_entry->reader_current_char_index + 68 * playlist_entry->section_buffer_current_index] = 0;
								/*playlist_item_invalid_unk(
									playlist_entry,
									0,
									2,
									playlist_entry->reader_current_line,
									&playlist_entry->section_buffer[68 * playlist_entry->section_buffer_current_index],
									byte_752CA8,
									byte_752CA8);*/
								playlist_entry->reader_current_mode = seekToNextLine;
							}
							else
							{
								playlist_entry->section_buffer[playlist_entry->reader_current_char_index + 68 * playlist_entry->section_buffer_current_index] = *stringBuffer;
								++playlist_entry->reader_current_char_index;
							}
						}
						goto Go_to_next_char;
					}
					CurrentItemNameTrimEnd(playlist_entry);
					playlist_entry->reader_current_mode = propertyDeliminatorScan;
					break;
				case propertyDeliminatorScan:
					if (*stringBuffer == '\t' || *stringBuffer == ' ')
						goto Go_to_next_char;
					playlist_entry->reader_current_char_index = 0;
					playlist_entry->reader_current_mode = propertyValueRead;
					break;
				case propertyValueRead:
					if (*stringBuffer == '\n')
					{
						if (!ProcessCustomSetting(playlist_entry)) {
							ProcessSetting(playlist_entry);
						}
						goto go_to_next_item;
					}
					if (playlist_entry->reader_current_char_index >= 31)
					{
						playlist_entry->section_buffer[playlist_entry->reader_current_char_index + 68 * playlist_entry->section_buffer_current_index + 32] = 0;
					/*	playlist_item_invalid_unk(
							playlist_entry,
							0,
							4,
							playlist_entry->reader_current_line,
							&playlist_entry->section_buffer[68 * playlist_entry->section_buffer_current_index],
							&playlist_entry->section_buffer[68 * playlist_entry->section_buffer_current_index + 32],
							byte_752CA8);*/
						playlist_entry->reader_current_mode = seekToNextLine;
					}
					else
					{
						playlist_entry->section_buffer[playlist_entry->reader_current_char_index + 68 * playlist_entry->section_buffer_current_index + 32] = *stringBuffer;
						++playlist_entry->reader_current_char_index;
					}
					goto Go_to_next_char;
				case seekToNextLine:
					if (*stringBuffer == '\n')
					{
					go_to_next_item:
						playlist_entry->reader_current_mode = newLine;
					Go_to_next_line:
						++playlist_entry->reader_current_line;
					}
				Go_to_next_char:
					++stringBuffer;
					break;
				default:
					break;
			}
		}
	}

	void ApplyHooks()
	{
		p_playlist_read_line = (h_playlist_read_line)DetourFunc(h2mod->GetAddress<BYTE*>(0, 0x12E4F), (BYTE*)ReadLine, 12);
		p_playlist_process_setting = (h_playlist_processs_setting)DetourFunc(h2mod->GetAddress<BYTE*>(0, 0x10FBE), (BYTE*)ProcessSetting, 5);
	}

	void Initialize()
	{
		ApplyHooks();
	}
}
