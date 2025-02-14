#include "stdafx.h"
#include "multiplayer_variant_settings_interface_definition.h"

s_variant_setting_edit_reference* __cdecl multiplayer_variant_settings_interface_get_reference(e_variant_setting_category_type category_type)
{
	return INVOKE(0x23ACDC, 0, multiplayer_variant_settings_interface_get_reference, category_type);
}

int32 __cdecl multiplayer_variant_settings_interface_get_variant_setting_value(s_game_variant* variant,	e_default_variant_setting_category_type type)
{
	return INVOKE(0x23AA54, 0, multiplayer_variant_settings_interface_get_variant_setting_value, variant, type);
}

s_text_value_pair_reference_new* __cdecl multiplayer_variant_settings_interface_get_variant_setting_label(s_text_value_pair_definition* text_value_pair, int32 value)
{
	return INVOKE(0x23AD57, 0, multiplayer_variant_settings_interface_get_variant_setting_label, text_value_pair, value);
}
