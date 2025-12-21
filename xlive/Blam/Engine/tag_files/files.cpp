#include "stdafx.h"
#include "files.h"

#include "files_windows.h"

/* public code */

s_file_reference_info* file_reference_get_info(s_file_reference* info)
{
	ASSERT(info);
	ASSERT(info->signature == FILE_REFERENCE_SIGNATURE);
	ASSERT(VALID_BITS(info->flags, NUMBER_OF_REFERENCE_INFO_FLAGS));
	ASSERT(info->location >= NONE && info->location < NUMBER_OF_FILE_REFERENCE_LOCATIONS);
	return info;
}

void file_read_into_memory(s_file_reference* reference, size_t size)
{
	bool opened_file = false;

	void* buffer = CSERIES_MALLOC(size);
	if (buffer)
	{
		e_file_open_error error;
		e_file_open_flags flags = (e_file_open_flags)(_permission_read_bit | _permission_write_bit);
		opened_file = file_open(reference, flags, &error);
		if (opened_file)
		{
			const size_t eof_val = file_get_eof(reference);
			if (eof_val > size)
			{
				if (file_set_position(reference, eof_val - size, 0))
				{
					if (file_read(reference, size, 0, buffer))
					{
						if (file_set_position(reference, 0, 0))
						{
							if (file_write(reference, size, buffer))
							{
								file_set_eof(reference, size);
							}
						}
					}
				}
			}
		}
	}

	if (buffer)
	{
		CSERIES_FREE(buffer);
	}

	if (reference)
	{
		if (opened_file)
		{
			file_close(reference);
		}
	}
	return;
}
