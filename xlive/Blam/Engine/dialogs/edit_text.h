#pragma once

/* structures */

struct edit_text
{
	char* buffer;
	int16 maximum_length;
	int16 insertion_point_index;
	int16 selection_start_index;
};

/* prototypes */

void edit_text_new(edit_text* edit);

void edit_text_handle_key(edit_text* edit, const struct key_stroke* key);

void edit_text_selection_reset(edit_text* edit);

/* macros */

#define valid_edit_text(edit) edit && edit->buffer && edit->maximum_length > 0 && (int16)cstrlen(edit->buffer) <= edit->maximum_length
