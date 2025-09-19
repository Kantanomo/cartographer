#pragma once

/* enums */

/* TODO: figure out names */
enum e_font_id : int32
{
	_font_id_fixedsys_9 = 0,
	_font_id_1,
	_font_id_2,
	_font_id_3,
	_font_id_4,
	_font_id_5,
	_font_id_6,
	_font_id_7,
	_font_id_8,
	_font_id_9,
	_font_id_10,
	_font_id_11,
	k_public_font_id_count
};

/* prototypes */

void __cdecl font_initialize(void);

void __cdecl font_idle(void);

int32 __cdecl font_get_line_height(e_font_id font_id);
