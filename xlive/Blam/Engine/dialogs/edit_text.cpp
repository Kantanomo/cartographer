#include "stdafx.h"
#include "edit_text.h"

#include "input/input_windows.h"

/* prototypes */

static void edit_text_fix_selection(edit_text* edit);

static bool edit_text_get_selection_indices(edit_text* edit, int16* start_index, int16* end_index);

/* public code */

void edit_text_new(edit_text* edit)
{
	ASSERT(valid_edit_text(edit));
	edit_text_selection_reset(edit);
	return;
}

void edit_text_handle_key(edit_text* edit, const key_stroke* key)
{
	ASSERT(valid_edit_text(edit));
	edit_text_fix_selection(edit);

	if (key->key_code == _key_backspace || key->key_code == _key_delete)
	{
		int16 start_index;
		int16 end_index;
		if (edit_text_get_selection_indices(edit, &start_index, &end_index))
		{
			const size_t length = cstrlen(&edit->buffer[start_index]);
			memmove_guarded(&edit->buffer[start_index], &edit->buffer[end_index], length, edit->buffer, edit->maximum_length);
			edit->insertion_point_index = start_index;
			edit->selection_start_index = NONE;
		}
		else if (key->key_code == _key_backspace && edit->insertion_point_index > 0)
		{
			const int16 index = edit->insertion_point_index--;
			const size_t length = cstrlen(&edit->buffer[index]);
			memmove_guarded(&edit->buffer[edit->insertion_point_index], &edit->buffer[index], length + 1, edit->buffer, edit->maximum_length);
		}
		else if (key->key_code == _key_delete)
		{
			if (edit->insertion_point_index < (int16)cstrlen(edit->buffer))
			{
				const int16 index = edit->insertion_point_index + 1;
				const size_t length = cstrlen(&edit->buffer[index]) + 1;
				memmove_guarded(&edit->buffer[edit->insertion_point_index], &edit->buffer[index], length, edit->buffer, edit->maximum_length);
			}
		}
	}
	else if (key->key_code == _key_left_arrow || key->key_code == _key_right_arrow)
	{
		int16 start_index;
		int16 end_index;
		if (TEST_BIT(key->modifier_flags, _key_modifier_shift_bit) || !edit_text_get_selection_indices(edit, &start_index, &end_index))
		{
			if (TEST_BIT(key->modifier_flags, _key_modifier_shift_bit) && edit->selection_start_index == NONE)
			{
				edit->selection_start_index = edit->insertion_point_index;
			}

			if (key->key_code == _key_left_arrow && edit->insertion_point_index > 0)
			{
				--edit->insertion_point_index;
			}
			else if (key->key_code == _key_right_arrow)
			{
				if (edit->insertion_point_index < (int16)cstrlen(edit->buffer))
				{
					++edit->insertion_point_index;
				}
			}

			if (edit->selection_start_index == edit->insertion_point_index)
			{
				edit->selection_start_index = NONE;
			}
		}
		else
		{
			edit->insertion_point_index = key->key_code == _key_left_arrow ? start_index : end_index;
			edit->selection_start_index = NONE;
		}
	}
	else if (key->ascii_code && key->ascii_code != NONE)
	{
		int16 start_index;
		int16 end_index;
		if (edit_text_get_selection_indices(edit, &start_index, &end_index))
		{
			const int16 max_length = edit->maximum_length;
			char* bounds_lower = edit->buffer;
			const size_t length = cstrlen(&edit->buffer[start_index]);

			memmove_guarded(&edit->buffer[start_index + 1], &edit->buffer[end_index], length + 1, bounds_lower, max_length);
			edit->insertion_point_index = start_index;
			edit->selection_start_index = NONE;
			edit->buffer[edit->insertion_point_index++] = key->ascii_code;
		}
		else if ((int16)cstrlen(edit->buffer) < edit->maximum_length)
		{
			char* dst = &edit->buffer[edit->insertion_point_index + 1];
			const char* src = &edit->buffer[edit->insertion_point_index];
			const size_t length = cstrlen(src);

			memmove_guarded(dst, src, length, edit->buffer, edit->maximum_length);
			edit->buffer[edit->insertion_point_index++] = key->ascii_code;
		}
	}
	return;
}

void edit_text_selection_reset(edit_text* edit)
{
	ASSERT(valid_edit_text(edit));
	edit_text_fix_selection(edit);
	edit->insertion_point_index = (int16)cstrlen(edit->buffer);
	edit->selection_start_index = NONE;
	return;
}

/* private code */

static void edit_text_fix_selection(edit_text* edit)
{
	const int16 length = (int16)cstrlen(edit->buffer);
	edit->insertion_point_index = PIN(edit->insertion_point_index, 0, length);
	edit->selection_start_index = PIN(edit->selection_start_index, NONE, length);
	
	if (edit->insertion_point_index == edit->selection_start_index)
	{
		edit->selection_start_index = NONE;
	}
	return;
}

static bool edit_text_get_selection_indices(edit_text* edit, int16* start_index, int16* end_index)
{
	ASSERT(valid_edit_text(edit));
	edit_text_fix_selection(edit);

	bool result = false;
	if (edit->selection_start_index != NONE)
	{
		const bool selection_greater_than_insertion = edit->selection_start_index > edit->insertion_point_index;
		*start_index = selection_greater_than_insertion ? edit->insertion_point_index : edit->selection_start_index;
		*end_index = selection_greater_than_insertion ? edit->selection_start_index : edit->insertion_point_index;
	  
		result = true;
	}

	return result;
}
