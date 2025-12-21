#include "stdafx.h"
#include "game_state.h"

#include "game/game.h"
#include "game_state_procs.h"

#include "main/console.h"
#include "tag_files/files.h"

/* prototypes */

static bool string_is_absolute_path(const char* string);

/* public code */

void __cdecl game_state_initialize(void)
{
	INVOKE(0x30AA6, 0x53EAD, game_state_initialize);
	return;
}

void* __cdecl game_state_malloc(const char* name, const char* description, uint32 size)
{
	return INVOKE(0x2FF1D, 0x53327, game_state_malloc, name, description, size);
}

s_game_state_globals* game_state_globals_get()
{
	return Memory::GetAddress<s_game_state_globals*>(0x47A728, 0x4D2590);
}

bool game_state_header_valid(game_state_header* header)
{
	bool result;

	void* fn_ptr = Memory::GetAddress<void*>(0x301C1, 0x0);
	__asm
	{
		mov eax, header
		call fn_ptr
		mov result, al
	}

	return result;
}

bool game_state_get_game_options_from_core(const char* core_name, s_game_options* options)
{
	ASSERT(core_name);
	ASSERT(options);

	bool success = false;

	game_state_header state_header;
	
	if (game_state_read_core(core_name, &state_header, sizeof(state_header)) && game_state_header_valid(&state_header))
	{
		csmemcpy(options, &state_header.options, sizeof(*options));
		success = true;
	}

	return success;
}

void game_state_save_core(const char* name)
{
	s_game_state_globals* game_state_globals = game_state_globals_get();

	uint32 game_state_proc_flags = 0;

	if (!game_is_server() && !game_is_distributed())
	{
		game_state_proc_flags = FLAG(3) | FLAG(6) | FLAG(7);
	}

	game_state_call_before_save_procs(game_state_proc_flags);
	if (game_state_write_core(name, game_state_globals->base_address, k_game_state_size))
	{
#ifdef TERMINAL_ENABLED
		console_printf("saved \'%s\'", name);
#endif
		// game_state_upload_to_debug_server
	}
	else
	{
#ifdef TERMINAL_ENABLED
		console_printf("error writing \'%s\'", name);
#endif
	}
	game_state_call_after_save_procs(game_state_proc_flags);
	return;
}

void game_state_get_core_file_reference(const char* core_name, s_file_reference* file)
{
	ASSERT(core_name);
	ASSERT(file);

	c_static_string<1024> filename;

	if (string_is_absolute_path(core_name))
	{
		filename.print("%s.bin", core_name);
	}
	else
	{
		filename.print("core\\%s.bin", core_name);
	}

	file_reference_create_from_path(file, filename.get_string(), false);
}

bool game_state_read_core(const char* name, void* buffer, uint32 buffer_size)
{
	bool success = false;

	s_file_reference core_file;
	e_file_open_error core_file_open_error;
	game_state_get_core_file_reference(name, &core_file);

	if (file_open(&core_file, _permission_read_bit, &core_file_open_error))
	{
		success = file_read(&core_file, buffer_size, false, buffer);
		file_close(&core_file);
	}

	return success;
}

bool game_state_write_core(const char* name, void* buffer, uint32 buffer_size)
{
	bool success = false;

	s_file_reference core_file;
	e_file_open_error core_file_open_error;

	game_state_get_core_file_reference(name, &core_file);
	file_create_parent_directories_if_not_present(&core_file);

	if (!file_exists(&core_file))
	{
		file_create(&core_file);
	}

	if (file_open(&core_file, _permission_write_bit, &core_file_open_error))
	{
		success = file_write(&core_file, buffer_size, buffer);
		file_close(&core_file);
	}

	return success;
}

/* private code */

static bool string_is_absolute_path(const char* string)
{
	return strstr(string, ":\\") > string;
}
