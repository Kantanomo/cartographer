#include "stdafx.h"
#include "text_group.h"

void __cdecl text_group_get_unicode_string(datum unic_datum, string_id string_id, wchar_t* out_string)
{
	INVOKE(0x3E3AC, 0, text_group_get_unicode_string, unic_datum, string_id, out_string);
}
