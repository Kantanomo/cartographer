#include "stdafx.h"
#include "saved_game_files.h"

#include "saved_game_files_async_windows.h"
#include "cache/physical_memory_map.h"
#include "cseries/async.h"
#include "filesys/pc_file_system.h"
#include "filesys/pc_saved_game.h"
#include "tag_files/files_windows.h"
#include "text/unicode.h"

/* globals */

const wchar_t* k_saved_game_file_type_strings[k_number_of_saved_game_file_types]
{
	L"profile",
	L"slayer",
	L"koth",
	L"race",
	L"oddball",
	L"juggernaut",
	L"headhunter",
	L"ctf",
	L"assault",
	L"territories",
};

/* typedef */

typedef void(__cdecl* t_saved_games_load_save_file_information_from_disk)(c_static_array<s_saved_game_main_menu_globals_save_file_info, k_maximum_enumerated_saved_game_files_any_type_per_memory_unit>* save_files_storage);
t_saved_games_load_save_file_information_from_disk p_saved_games_load_save_file_information_from_disk;

/* prototypes */

void saved_game_main_menu_globals_set(s_saved_game_main_menu_globals* new_globals);
uint32 __cdecl saved_game_loading_allocate_storage(int32 a1, s_saved_game_file_loading_information* loading_information);
void saved_game_load_save_file_information_from_disk(c_static_array<s_saved_game_main_menu_globals_save_file_info, k_maximum_enumerated_saved_game_files_any_type_per_memory_unit>* save_files_storage);
void saved_game_file_globals_wait_for_io_to_complete();
void saved_game_main_menu_globals_initialize(void);
void saved_game_files_memory_initialize(int32 unk);

/* public code */

void saved_game_files_apply_hooks(void)
{
	//WritePointer(Memory::GetAddress(0x39BD90), saved_game_files_memory_initialize);
	//DETOUR_ATTACH(p_saved_games_load_save_file_information_from_disk, Memory::GetAddress<t_saved_games_load_save_file_information_from_disk>(0x46596), saved_game_load_save_file_information_from_disk);
	return;
}

s_saved_game_main_menu_globals* saved_game_main_menu_globals_get()
{
	return *Memory::GetAddress<s_saved_game_main_menu_globals**>(0x482300);
}

s_saved_game_files_globals* saved_game_files_globals_get()
{
	return Memory::GetAddress<s_saved_game_files_globals*>(0x482424);
}

e_global_string_id saved_game_get_type_string_id(e_saved_game_file_type type)
{
	const static e_global_string_id saved_game_file_type_string_ids[k_number_of_saved_game_file_types]
	{
		_string_id_player_profile_display_name,
		_string_id_slayer_display_name,
		_string_id_koth_display_name,
		_string_id_race_display_name,
		_string_id_oddball_display_name,
		_string_id_juggernaut_display_name,
		_string_id_headhunter_display_name,
		_string_id_ctf_display_name,
		_string_id_assault_display_name,
		_string_id_territories_display_name
	};

	if (type <= k_number_of_saved_game_file_types)
		return saved_game_file_type_string_ids[type];

	return saved_game_file_type_string_ids[0];
}

bool saved_game_get_file_info(s_saved_game_main_menu_globals_save_file_info* out_info, uint32 enumerated_index)
{
	s_saved_game_main_menu_globals* saved_game_main_menu_globals = saved_game_main_menu_globals_get();
	s_saved_game_files_globals* saved_game_files_globals = saved_game_files_globals_get();

	if (saved_game_main_menu_globals)
	{
		// file is not a default save
		if (!ENUMERATED_INDEX_IS_DEFAULT_SAVE(enumerated_index))
		{
			auto abs_index = (enumerated_index >> 8) & 0x1FFF;
			auto last_index = saved_game_main_menu_globals->save_files.get_count() - 1;
			if ((abs_index <= last_index || abs_index == last_index))
			{
				csmemcpy(out_info, saved_game_main_menu_globals->save_files[abs_index], saved_game_main_menu_globals->save_files.get_type_size());
				return true;
			}
		}
	}
	else if (saved_game_files_globals->cache_files_exist)
	{
		for (uint32 i = 0; i < saved_game_files_globals->cached_save_files.get_count(); ++i)
		{
			if (enumerated_index == saved_game_files_globals->cached_save_files[i]->enumerated_index)
			{
				csmemcpy(out_info, &saved_game_files_globals->cached_save_files[i]->file_info, sizeof(s_saved_game_main_menu_globals_save_file_info));
				return true;
			}
		}
	}

	return false;
}

uint32 saved_game_get_file_size_kb_for_type(e_saved_game_file_type type)
{
	if (type == _saved_game_file_type_profile)
		return (sizeof(s_saved_game_player_profile) + 1023) / k_kilo;

	if (type > _saved_game_file_type_profile && type <= _saved_game_file_type_game_variant_territories)
		return (sizeof(s_game_variant) + 1023) / k_kilo;

	return 0;
}

const wchar_t* saved_game_get_file_type_as_string(e_saved_game_file_type file_type)
{
	return file_type < k_number_of_saved_game_file_types ? k_saved_game_file_type_strings[file_type] : L"unknown";
}

bool saved_game_new_main_menu_globals_save_file(s_saved_game_main_menu_globals_save_file_info* new_save, e_saved_game_file_type file_type, wchar_t* out_path)
{
	wcsncpy(out_path, new_save->file_path, 256);
	wchar_t* cat_path = ustrnzcat(out_path, saved_game_get_file_type_as_string(file_type), 256);
	cat_path[255] = '\0';
	wcsncpy(out_path, cat_path, 256);
	return true;
}

bool __cdecl saved_game_add_save_to_cache(s_saved_game_main_menu_globals_save_file_info* new_save, uint32* out_save_index)
{
	s_saved_game_main_menu_globals* main_menu_globals = saved_game_main_menu_globals_get();

	ASSERT(new_save);
	ASSERT(main_menu_globals);

	uint32 last_save_index = main_menu_globals->save_files.get_count();

	if(last_save_index != k_maximum_enumerated_saved_game_files_any_type_per_memory_unit)
	{
		s_saved_game_main_menu_globals_save_file_info* save = main_menu_globals->save_files.next();

		memcpy(save, new_save, sizeof(s_saved_game_main_menu_globals_save_file_info));

		*out_save_index = last_save_index;

		return true;
	}

	return false;
	//return INVOKE(0x427A7, 0, saved_game_add_save_to_cache, new_save, out_save_index);
}

void __fastcall saved_game_remove_save_from_cache(uint32 enumerated_file_index)
{
	INVOKE(0x3EEC3, 0, saved_game_remove_save_from_cache, enumerated_file_index);
}

void saved_game_get_display_name(uint32 enumerated_index, wchar_t* display_name)
{
	ASSERT(display_name);

	display_name[0] = '\0';
	s_saved_game_main_menu_globals* saved_game_main_menu_globals = saved_game_main_menu_globals_get();

	if (saved_game_main_menu_globals && ENUMERATED_INDEX_IS_DEFAULT_SAVE(enumerated_index))
	{
		uint32 absolute_index = (enumerated_index >> 8) & 0x1FFF;
		uint32 last_index = saved_game_main_menu_globals->default_save_files.get_count() - 1;
		if (absolute_index <= last_index || absolute_index == last_index)
		{
			s_saved_game_main_menu_globals_default_save_file* default_save = saved_game_main_menu_globals->default_save_files[absolute_index];
			s_saved_game_player_profile* default_profile = (s_saved_game_player_profile*)default_save->buffer;
			if ((enumerated_index & 0xF) != 0)
			{
				if ((enumerated_index & 0xF) <= 9)
				{
					csmemcpy(display_name, &default_save->buffer[0xC], 128);
				}
			}
			else
			{
				csmemcpy(display_name, &default_save->buffer[0x8], 128);
			}
		}
	}
	else
	{
		s_saved_game_main_menu_globals_save_file_info file_info = {};
		if (saved_game_get_file_info(&file_info, enumerated_index))
		{
			ustrncpy(display_name, file_info.display_name, NUMBEROF(file_info.display_name));
		}
	}
	return;
}

bool __cdecl saved_game_get_display_name_for_type(wchar_t* display_name, e_saved_game_file_type saved_game_type,
	e_language language, wchar_t* full_display_name)
{
	//todo: rewrite and fix broken implementation to return the game engine mode string?

	return INVOKE(0x427F6, 0, saved_game_get_display_name_for_type, display_name, saved_game_type, language, full_display_name);
}

bool __cdecl saved_game_create_save_game_directory(e_saved_game_file_type type, wchar_t* out_string)
{
	ASSERT(out_string);

	s_saved_game_files_globals* saved_game_files_globals = saved_game_files_globals_get();


	int8 test_result;

	if(input_windows_drive_letter_test(0, &test_result))
	{
		wchar_t full_display_name[save_game_max_name] {};
		char flat_path[MAX_PATH]{};
		wchar_t wide_path[MAX_PATH]{};

		pc_file_system_get_saved_games_location(flat_path, NUMBEROF(flat_path));


		int16 current_folder_index = 0;
		do
		{
			usnzprintf(out_string, save_game_max_name, L"Halo%04d", current_folder_index + 1);

			saved_game_get_display_name_for_type(out_string, type, saved_game_files_globals->language_id, full_display_name);

			if (pc_saved_game_get_next_available_save_location(flat_path, full_display_name, 3, 0, wide_path, MAX_PATH))
				break;

			++current_folder_index;

		} while (current_folder_index + 1 < k_maximum_enumerated_saved_game_files_any_type_per_memory_unit);

		if(current_folder_index == 4096)
		{
			saved_game_files_globals->saved_file_creation_result = _saved_game_disk_result_no_free_slots;

			memset(out_string, 0, sizeof(wchar_t) * save_game_max_name);
		}
	}

	return *out_string != 0;

	return INVOKE(0x4333A, 0, saved_game_create_save_game_directory, type, out_string);
}

int32 saved_game_create_file(e_saved_game_file_type type, e_controller_index originating_controller_index,
	wchar_t* new_file_name)
{
	s_saved_game_files_globals* saved_game_files_globals = saved_game_files_globals_get();
	s_saved_game_main_menu_globals* saved_game_main_menu_globals = saved_game_main_menu_globals_get();

	ASSERT(new_file_name);
	ASSERT(saved_game_files_globals);
	ASSERT(saved_game_main_menu_globals);

	saved_game_file_globals_wait_for_io_to_complete();

	if(saved_game_main_menu_globals->save_files.get_count() >= k_maximum_enumerated_saved_game_files_any_type_per_memory_unit)
	{
		saved_game_files_globals->saved_file_creation_result = _saved_game_disk_result_no_free_slots;

		return NONE;
	}

	wchar_t full_display_name[save_game_max_name] {};

	if(saved_game_get_display_name_for_type(new_file_name, type, saved_game_files_globals->language_id, full_display_name))
	{
		char flat_games_location[MAX_PATH] {};
		wchar_t wide_save_location[MAX_PATH] {};
		
		pc_file_system_get_saved_games_location(flat_games_location, MAX_PATH);

		if(!pc_saved_game_get_next_available_save_location(flat_games_location, full_display_name, 1, 0, wide_save_location, MAX_PATH))
		{
			CHAR multi_byte_location[256] {};
			WCHAR saved_game_full_path[256] {};

			s_saved_game_main_menu_globals_save_file_info new_main_menu_save {};

			uint32 new_saved_game_index = 0;

			s_file_reference filo;

			new_main_menu_save.type = type;
			new_main_menu_save.language_id = saved_game_files_globals->language_id;

			wcsncpy(new_main_menu_save.display_name, new_file_name, 17u);
			wcsncpy(new_main_menu_save.file_path, wide_save_location, MAX_PATH);


			saved_game_new_main_menu_globals_save_file(&new_main_menu_save, type, saved_game_full_path);

			WideCharToMultiByte(CP_UTF8, 0, saved_game_full_path, NONE, multi_byte_location, 256, 0, 0);

			if(file_reference_create_from_path(&filo, multi_byte_location, 0) 
				&& file_create(&filo)
				&& saved_game_add_save_to_cache(&new_main_menu_save, &new_saved_game_index))
			{
				const int32 new_file_enumerated_file_index = type & 0xF | ((new_saved_game_index & 0x1FFF | ((saved_game_main_menu_globals->saved_game_file_index_salt & 0x1FF) << 14)) << 8);

				if(new_file_enumerated_file_index != NONE)
				{
					saved_game_files_globals->saved_file_creation_result = _saved_gave_disk_result_success;

					return new_file_enumerated_file_index;
				}
			}
			else
			{
				pc_file_system_delete_save_directory(flat_games_location, full_display_name);
			}
		}

		uint32 save_file_type_size = saved_game_get_file_size_kb_for_type(type);

		saved_game_files_globals->saved_file_creation_result = pc_file_system_check_disk_free_space(save_file_type_size)
			? _saved_games_disk_result_memory_unit_error
			: _saved_game_disk_result_no_free_space;
	}

	return NONE;
}

int32 saved_game_create_new_game_variant(e_controller_index origin_controller, e_saved_game_file_type type,
                                           wchar_t* name)
{
	const int32 enumerated_file_index = saved_game_create_file(type, origin_controller, name);

	s_game_variant new_variant {};

	if (enumerated_file_index == NONE)
		return NONE;

	switch(type)
	{
		case _saved_game_file_type_game_variant_slayer:
			game_variant_create_default_new(&new_variant, _game_variant_description_slayer);
			break;
		case _saved_game_file_type_game_variant_koth:
			game_variant_create_default_new(&new_variant, _game_variant_description_king);
			break;
		case _saved_game_file_type_game_variant_race:
			//game_variant_create_default_new(&new_variant, _game_variant_description_race);
			break;
		case _saved_game_file_type_game_variant_oddball:
			game_variant_create_default_new(&new_variant, _game_variant_description_oddball);
			break;
		case _saved_game_file_type_game_variant_juggernaut:
			game_variant_create_default_new(&new_variant, _game_variant_description_juggernaut);
			break;
		case _saved_game_file_type_game_variant_headhunter:
			game_variant_create_default_new(&new_variant, _game_variant_description_headhunter);
			break;
		case _saved_game_file_type_game_variant_ctf:
			game_variant_create_default_new(&new_variant, _game_variant_description_ctf);
			break;
		case _saved_game_file_type_game_variant_assault:
			game_variant_create_default_new(&new_variant, _game_variant_description_invasion);
			break;
		case _saved_game_file_type_game_variant_territories:
			game_variant_create_default_new(&new_variant, _game_variant_description_territories);
			break;
	}

	new_variant.flags &= ~1u;

	ASSERT(game_variant_validate(&new_variant));
	
	wcsncpy(new_variant.variant_name, name, NUMBEROF(new_variant.variant_name));

	if(saved_games_async_helper_write_variant(enumerated_file_index, &new_variant))
	{
		return enumerated_file_index;
	}

	saved_game_new_failure_cleanup(type, enumerated_file_index);
	return NONE;

	//return INVOKE(0x5AB63, 0, saved_game_create_new_game_variant, origin_controller, type, name);
}

bool saved_game_load_game_variant(uint32 enumerated_index, s_game_variant* out_variant)
{
	return INVOKE(0x5A96B, 0, saved_game_load_game_variant, enumerated_index, out_variant);
}

/* private code */

void saved_game_main_menu_globals_set(s_saved_game_main_menu_globals* new_globals)
{
	*Memory::GetAddress<s_saved_game_main_menu_globals**>(0x482300) = new_globals;
}

uint32 __cdecl saved_game_loading_allocate_storage(int32 a1, s_saved_game_file_loading_information* loading_information)
{
	return INVOKE(0x9CC67, 0, saved_game_loading_allocate_storage, a1, loading_information);
}


void saved_game_load_save_file_information_from_disk(c_static_array<s_saved_game_main_menu_globals_save_file_info, k_maximum_enumerated_saved_game_files_any_type_per_memory_unit>* save_files_storage)
{
	return p_saved_games_load_save_file_information_from_disk(save_files_storage);
}

void saved_game_file_globals_wait_for_io_to_complete()
{
	s_saved_game_files_globals* saved_game_file_globals = saved_game_files_globals_get();

	async_yield_until_done((s_async_completion*)saved_game_file_globals, true);
}

void saved_game_main_menu_globals_initialize(void)
{
	s_saved_game_files_globals* saved_game_files_globals = saved_game_files_globals_get();

	s_saved_game_main_menu_globals* saved_game_main_menu_globals = (s_saved_game_main_menu_globals*)c_physical_memory::allocate(sizeof(s_saved_game_main_menu_globals));

	ASSERT(saved_game_main_menu_globals);

	if(saved_game_main_menu_globals)
	{
		saved_game_main_menu_globals->default_save_files.clear();
		saved_game_main_menu_globals->save_files.clear();
	}
	else
	{
		saved_game_main_menu_globals = nullptr;
	}

	saved_game_main_menu_globals->saved_game_file_index_salt = saved_game_files_globals->unk_6;

	saved_game_files_globals->unk_6 = (saved_game_files_globals->unk_6 + 1) % 200;
	saved_game_files_globals->unk_3 = 3;

	saved_game_main_menu_globals_set(saved_game_main_menu_globals);
	return;
}

void saved_game_new_failure_cleanup(e_saved_game_file_type type, uint32 enumerated_file_index)
{
	if(enumerated_file_index != NONE)
	{
		s_saved_game_files_globals* file_globals = saved_game_files_globals_get();

		s_saved_game_main_menu_globals_save_file_info save_file_info{};

		ASSERT(file_globals);

		saved_game_file_globals_wait_for_io_to_complete();

		if(saved_game_get_file_info(&save_file_info, enumerated_file_index))
		{
			if((enumerated_file_index & 0x200000) == 0)
			{
				char flat_directory_path[MAX_PATH] {};

				int8 drive_letter;

				if(input_windows_drive_letter_test((enumerated_file_index >> 4) & 0xF, &drive_letter))
				{
					wchar_t full_display_name[save_game_max_name]{};

					saved_game_get_display_name_for_type(
						save_file_info.display_name, 
						(e_saved_game_file_type)(enumerated_file_index & 0xF), 
						(e_language)save_file_info.language_id, 
						full_display_name);

					pc_file_system_get_saved_games_location(flat_directory_path, MAX_PATH);

					pc_file_system_delete_save_directory(flat_directory_path, full_display_name);
				}
			}

			saved_game_remove_save_from_cache(enumerated_file_index);
		}
	}
}


void saved_game_files_memory_initialize(int32 unk)
{
	s_saved_game_files_globals* saved_game_files_globals = saved_game_files_globals_get();
	s_saved_game_main_menu_globals* saved_game_main_menu_globals = saved_game_main_menu_globals_get();

	ASSERT(saved_game_main_menu_globals);
	ASSERT(saved_game_files_globals->memory_initialized_for_game);
	
	if (unk == 1)
	{
		saved_game_main_menu_globals_initialize();
	}

	saved_game_files_globals->memory_initialized_for_game = true;
	return;
}
