#include "PlaylistLoader.h"
#include "Util/Hooks/Hook.h"
#include "H2MOD.h"
#include "H2MOD/Modules/Startup/Startup.h"
#include "H2MOD/Modules/Utils/Utils.h"
#include "H2MOD/Modules/CustomVariantSettings/CustomVariantSettings.h"

namespace PlaylistLoader
{
	static wchar_t EmptyChar = '\0';
	enum e_custom_setting
	{
		None = -1,
		Gravity,
		InfiniteAmmo,
		PlayerSpeed
	};
	std::map<std::wstring, e_custom_setting> Custom_Settings;
	e_custom_setting GetCustomSettingIndex(wchar_t* Name)
	{
		for (const auto custom_setting : Custom_Settings)
			if (_wcsicmp(custom_setting.first.c_str(), Name) == 0)
				return custom_setting.second;
		return None;
	}
	void PlaylistInvalidItemHook(playlist_entry* playlist_entry, int a2, int a3, int a4, wchar_t *a5, wchar_t *a6, wchar_t *a7)
	{
		__asm
		{
			push a7
			push a6
			push a5
			push a4
			push a3
			mov ebx, a2
			mov ecx, playlist_entry
			mov eax, [0xED2E]
			add eax, [H2BaseAddr]
			call eax
			add esp, 20
		}
	}

	bool CustomSettingBooleanCheck(playlist_entry* playlist_entry, wchar_t* value)
	{
		if (_wcsicmp(value, L"on") == 0 || _wcsicmp(value, L"true") == 0)
			return true;
		if (_wcsicmp(value, L"off") == 0 || _wcsicmp(value, L"false") == 0)
			return false;
		PlaylistInvalidItemHook(
			playlist_entry,
			0,
			4,
			playlist_entry->reader_current_line,
			&playlist_entry->section_buffer[68 * playlist_entry->section_buffer_current_index],
			&playlist_entry->section_buffer[68 * playlist_entry->section_buffer_current_index + 32],
			&EmptyChar);
		return false;
	}

	bool ProcessCustomSetting(playlist_entry* playlist_entry)
	{
		//Section Type must be Variant
		if (playlist_entry->current_section_type != 1)
			return false;

		bool result = false;
		wchar_t* propertyName = (wchar_t*)calloc(64, 1);
		wchar_t* propertyValue = (wchar_t*)calloc(64, 1);
		wchar_t* tempName = (wchar_t*)calloc(64, 1);
		wchar_t* variant = (wchar_t*)calloc(64, 1);

		//Pull the current property name from the buffer.
		for (auto i = 0; i < 32; i++)
		{
			propertyName[i] = playlist_entry->section_buffer[68 * playlist_entry->section_buffer_current_index + i];
			if (propertyName[i] == 0) break;
		}
		//Pull the current property value from  the buffer.
		for (auto i = 0; i < playlist_entry->reader_current_char_index; i++)
		{
			propertyValue[i] = playlist_entry->section_buffer[68 * playlist_entry->section_buffer_current_index + 32 + i];
			if (propertyValue[i] == 0) break;
		}

		//Trim the end of the value incase there is extra whitespace
		for(auto i = playlist_entry->reader_current_char_index; i > 0; i--)
		{
			if (propertyValue[i] != ' ' || propertyValue[i] != '\n')
				break;
			propertyValue[i] = 0;
		}

		//Scan the section buffer for the current variant name to associate it with the custom 
		//setting as the property can be anywhere in the section buffer
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
		const auto custom_setting_type = GetCustomSettingIndex(propertyName);

		if (_wcsicmp(variant, L"") != 0 && custom_setting_type != -1) {
			
			//Grab or create the Custom Settings for the current variant.
			CustomVariantSettings::s_variantSettings* settings;
			const auto variant_string = std::wstring(variant);
			if (CustomVariantSettingsMap.count(variant_string) > 0)
				settings = &CustomVariantSettingsMap.at(variant_string);
			else
			{
				CustomVariantSettingsMap[variant_string] = CustomVariantSettings::s_variantSettings();
				settings = &CustomVariantSettingsMap.at(variant_string);
			}

			LOG_INFO_GAME(L"[PlaylistLoader::ProcessCustomSetting] Variant: {} Custom Setting Detected: {} = {}", variant, propertyName, propertyValue);
			switch (custom_setting_type)
			{
				case Gravity:
					if (isFloat(propertyValue)) {
						settings->Gravity = std::stod(propertyValue);
					} 
					else
					{
						PlaylistInvalidItemHook(
							playlist_entry,
							0,
							4,
							playlist_entry->reader_current_line,
							&playlist_entry->section_buffer[68 * playlist_entry->section_buffer_current_index],
							&playlist_entry->section_buffer[68 * playlist_entry->section_buffer_current_index + 32],
							&EmptyChar);
					}
					break;
				case InfiniteAmmo:
					settings->InfiniteAmmo = CustomSettingBooleanCheck(playlist_entry, propertyValue);
					break;
				case None:
				default: break;
			}
			result = true;
		}
		free(propertyName);
		free(propertyValue);
		free(tempName);
		free(variant);
		return result;
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
							PlaylistInvalidItemHook(
								playlist_entry,
								0,
								3,
								playlist_entry->reader_current_line,
								playlist_entry->header_buffer,
								&EmptyChar,
								&EmptyChar);
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
								PlaylistInvalidItemHook(
									playlist_entry,
									0,
									2,
									playlist_entry->reader_current_line,
									&playlist_entry->section_buffer[68 * playlist_entry->section_buffer_current_index],
									&EmptyChar,
									&EmptyChar);
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
						PlaylistInvalidItemHook(
							playlist_entry,
							0,
							4,
							playlist_entry->reader_current_line,
							&playlist_entry->section_buffer[68 * playlist_entry->section_buffer_current_index],
							&playlist_entry->section_buffer[68 * playlist_entry->section_buffer_current_index + 32],
							&EmptyChar);
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
		Custom_Settings.emplace(L"Gravity", e_custom_setting::Gravity);
		Custom_Settings.emplace(L"Infinite Ammo", e_custom_setting::InfiniteAmmo);
		Custom_Settings.emplace(L"Player Speed", e_custom_setting::PlayerSpeed);
	}
}
