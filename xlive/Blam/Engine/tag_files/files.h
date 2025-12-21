#pragma once
#include "files_windows.h"

/* constants */

enum
{
	FILE_REFERENCE_SIGNATURE = 'filo'
};

enum
{
	_has_filename_bit = 0,
	NUMBER_OF_REFERENCE_INFO_FLAGS
};

enum
{
	_file_reference_application_relative = 0,
	_file_reference_cd_relative,
	NUMBER_OF_FILE_REFERENCE_LOCATIONS,
};

/* structures */

struct s_file_reference_info
{
	uint32 signature;
	uint16 flags;
	int16 location;
	char path[k_maximum_filename_length];
	HANDLE file_handle;
	HRESULT api_result;
};

struct s_file_reference : s_file_reference_info
{
};
ASSERT_STRUCT_SIZE(s_file_reference, 272);

/* prototypes */

s_file_reference_info* file_reference_get_info(s_file_reference* info);

void file_read_into_memory(s_file_reference* reference, size_t size);
