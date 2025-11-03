#include "stdafx.h"
#include "ik_point_iterator.h"

/* public code */

ik_point_iterator::ik_point_iterator(void)
{
	clear();
	return;
}

void ik_point_iterator::clear(void)
{
	marker = _string_id_invalid;
	attach_to_marker = _string_id_invalid;
	weapon_ik_index = NONE;
	pass = 0;
	mode_name = _string_id_invalid;
	weapon_class_name = _string_id_invalid;
	return;
}
