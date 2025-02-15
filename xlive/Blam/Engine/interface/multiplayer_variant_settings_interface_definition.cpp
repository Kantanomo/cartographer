#include "stdafx.h"
#include "multiplayer_variant_settings_interface_definition.h"

#include "text/unicode.h"

/* private code */

static void multiplayer_variant_settings_interface_parse_headhunter_moving_bin(e_multiplayer_variant_setting_interface_conversion_type* out_conversion_type, uint32* out_offset, void* out_variable)
{
	*out_conversion_type = _multiplayer_variant_setting_interface_conversion_type_flags_32;
	*out_offset = offsetof(s_game_variant, game_engine_variant.head_hunter.flags);
	*(uint32*)out_variable = _headhunter_engine_flag_moving_bin;
}

static void multiplayer_variant_settings_interface_parse_headhunter_point_multiplier(e_multiplayer_variant_setting_interface_conversion_type* out_conversion_type, uint32* out_offset, void* out_variable)
{
	*out_conversion_type = _multiplayer_variant_setting_interface_conversion_type_flags_32;
	*out_offset = offsetof(s_game_variant, game_engine_variant.head_hunter.flags);
	*(uint32*)out_variable = _headhunter_engine_flag_point_multiplier;
}

static void multiplayer_variant_settings_interface_parse_headhunter_suicide_point_loss(e_multiplayer_variant_setting_interface_conversion_type* out_conversion_type, uint32* out_offset, void* out_variable)
{
	*out_conversion_type = _multiplayer_variant_setting_interface_conversion_type_flags_32;
	*out_offset = offsetof(s_game_variant, game_engine_variant.head_hunter.flags);
	*(uint32*)out_variable = _headhunter_engine_flag_suicide_point_loss;
}

static void multiplayer_variant_settings_interface_parse_headhunter_death_point_loss(e_multiplayer_variant_setting_interface_conversion_type* out_conversion_type, uint32* out_offset, void* out_variable)
{
	*out_conversion_type = _multiplayer_variant_setting_interface_conversion_type_flags_32;
	*out_offset = offsetof(s_game_variant, game_engine_variant.head_hunter.flags);
	*(uint32*)out_variable = _headhunter_engine_flag_death_point_loss;
}

static void multiplayer_variant_settings_interface_parse_headhunter_uncontested_bin(e_multiplayer_variant_setting_interface_conversion_type* out_conversion_type, uint32* out_offset, void* out_variable)
{
	*out_conversion_type = _multiplayer_variant_setting_interface_conversion_type_flags_32;
	*out_offset = offsetof(s_game_variant, game_engine_variant.head_hunter.flags);
	*(uint32*)out_variable = _headhunter_engine_flag_uncontested_bin;
}

static void multiplayer_variant_settings_interface_parse_headhunter_speed_with_heads(e_multiplayer_variant_setting_interface_conversion_type* out_conversion_type, uint32* out_offset, void* out_variable)
{
	*out_conversion_type = _multiplayer_variant_setting_interface_conversion_type_int32;
	*out_offset = offsetof(s_game_variant, game_engine_variant.head_hunter.speed_with_heads);
	*(real32*)out_variable = 1.0f;
}

static void multiplayer_variant_settings_interface_parse_headhunter_max_heads_carried(e_multiplayer_variant_setting_interface_conversion_type* out_conversion_type, uint32* out_offset, void* out_variable)
{
	*out_conversion_type = _multiplayer_variant_setting_interface_conversion_type_int8;
	*out_offset = offsetof(s_game_variant, game_engine_variant.head_hunter.max_heads_carried);
	*(real32*)out_variable = 1.0f;
}

static void multiplayer_variant_settings_interface_variant_setting_label_boolean_value(int32 value, wchar_t* out_string)
{
	if (value == 1)
		usnzprintf(out_string, 512, L"On");
	else
		usnzprintf(out_string, 512, L"Off");
}

/* public code */

void multiplayer_variant_settings_interface_apply_patches()
{
	WritePointer(Memory::GetAddress(0x464E04), &multiplayer_variant_settings_interface_parse_headhunter_moving_bin);
	WritePointer(Memory::GetAddress(0x464E08), &multiplayer_variant_settings_interface_parse_headhunter_point_multiplier);
	WritePointer(Memory::GetAddress(0x464E0C), &multiplayer_variant_settings_interface_parse_headhunter_suicide_point_loss);
	WritePointer(Memory::GetAddress(0x464E10), &multiplayer_variant_settings_interface_parse_headhunter_death_point_loss);
	WritePointer(Memory::GetAddress(0x464E14), &multiplayer_variant_settings_interface_parse_headhunter_uncontested_bin);
	WritePointer(Memory::GetAddress(0x464E18), &multiplayer_variant_settings_interface_parse_headhunter_speed_with_heads);
	WritePointer(Memory::GetAddress(0x464E1C), &multiplayer_variant_settings_interface_parse_headhunter_max_heads_carried);
}

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

void multiplayer_variant_settings_interface_get_variant_setting_label_direct(s_game_variant* variant, e_default_variant_setting_category_type type, wchar_t* out_string)
{
	int32 value = multiplayer_variant_settings_interface_get_variant_setting_value(variant, type);

	switch(type)
	{
		case _default_variant_setting_category_type_headhunter_moving_head_bin:
		case _default_variant_setting_category_type_headhunter_point_multiplier:
		case _default_variant_setting_category_type_headhunter_suicide_point_loss:
		case _default_variant_setting_category_type_headhunter_death_point_loss:
		case _default_variant_setting_category_type_headhunter_uncontested_bin:
		{
			multiplayer_variant_settings_interface_variant_setting_label_boolean_value(value, out_string);
			return;
		}
		case _default_variant_setting_category_type_headhunter_speed_with_heads:
		{
			switch ((e_ctf_engine_player_speed)value)
			{
				case _ctf_engine_player_speed_slow:
					usnzprintf(out_string, 512, L"Slow");
					return;
				case _ctf_engine_player_speed_normal:
					usnzprintf(out_string, 512, L"Normal");
					return;
				case _ctf_engine_player_speed_fast:
					usnzprintf(out_string, 512, L"Fast");
					return;
			}
		}
		case _default_variant_setting_category_type_headhunter_max_heads_carried:
		{
			switch ((e_headhunter_max_heads_carried)value)
			{
			case _headhunter_max_heads_carried_none:
				usnzprintf(out_string, 512, L"Unlimited");
				return;
			case _headhunter_max_heads_carried_one:
				usnzprintf(out_string, 512, L"One");
				return;
			case _headhunter_max_heads_carried_five:
				usnzprintf(out_string, 512, L"Five");
				return;
			case _headhunter_max_heads_carried_ten:
				usnzprintf(out_string, 512, L"Ten");
				return;
			}	
		}
	}
	usnzprintf(out_string, 512, L"");
}
