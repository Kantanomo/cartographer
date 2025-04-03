#include "stdafx.h"
#include "multiplayer_variant_settings_interface_definition.h"

#include "multiplayer_variant_interface_headhunter_strings.h"
#include "text/unicode.h"

/* private code */

static void multiplayer_variant_settings_interface_parse_headhunter_moving_bin(e_multiplayer_variant_setting_interface_conversion_type* out_conversion_type, uint32* out_offset, void* out_variable)
{
	*out_conversion_type = _multiplayer_variant_setting_interface_conversion_type_int16;
	*out_offset = offsetof(s_game_variant, game_engine_variant.head_hunter.hill_move_time);
	*(uint32*)out_variable = 1.f;
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
	*(uint32*)out_variable = _king_engine_uncontested_hill_to_score_bit;
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

static void multiplayer_variant_settings_interface_get_variant_parameter_label_headhunter(int32 value, e_variant_setting_parameter_type type, wchar_t* out_string)
{
	const wchar_t* string = L"";

	switch (type)
	{
		case _variant_setting_parameter_type_headhunter_moving_head_bin:
		case _variant_setting_parameter_type_headhunter_point_multiplier:
		case _variant_setting_parameter_type_headhunter_suicide_point_loss:
		case _variant_setting_parameter_type_headhunter_death_point_loss:
		case _variant_setting_parameter_type_headhunter_uncontested_bin:
		{
			string = g_multiplayer_variant_interface_bool_value_strings[get_current_language()][value];
			break;
		}
		case _variant_setting_parameter_type_headhunter_speed_with_heads:
		{
			switch ((e_ctf_engine_player_speed)value)
			{
				case _ctf_engine_player_speed_normal:
				case _ctf_engine_player_speed_fast:
				case _ctf_engine_player_speed_slow:
					string = g_multiplayer_variant_interface_headhunter_speed_with_head_strings[get_current_language()][value];
					break;
			}
			break;
		}
		case _variant_setting_parameter_type_headhunter_max_heads_carried:
		{
			switch ((e_headhunter_max_heads_carried)value)
			{
				case _headhunter_max_heads_carried_none:
				case _headhunter_max_heads_carried_one:
				case _headhunter_max_heads_carried_five:
				case _headhunter_max_heads_carried_ten:
					string = g_multiplayer_variant_interface_headhunter_max_heads_carried_strings[get_current_language()][value];
					break;
			}
			break;
		}
	}
	usnzprintf(out_string, 512, string);
}

static int32 multiplayer_variant_settings_interface_get_variant_parameter_value_count_headhnter(e_variant_setting_parameter_type type)
{
	switch (type)
	{
		case _variant_setting_parameter_type_headhunter_moving_head_bin:
		case _variant_setting_parameter_type_headhunter_point_multiplier:
		case _variant_setting_parameter_type_headhunter_suicide_point_loss:
		case _variant_setting_parameter_type_headhunter_death_point_loss:
		case _variant_setting_parameter_type_headhunter_uncontested_bin:
		{
			return 2;
		}
		case _variant_setting_parameter_type_headhunter_speed_with_heads:
		{
			return k_ctf_engine_player_speed_count;
		}
		case _variant_setting_parameter_type_headhunter_max_heads_carried:
		{
			return k_headhunter_max_heads_carried_count;
		}
	}
	return NONE;
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

s_variant_setting_edit_reference* __cdecl multiplayer_variant_settings_interface_get_category_reference(e_variant_setting_category_type category_type)
{
	switch(category_type)
	{
		// return the setting reference for king on headhunter game modes
		case _variant_setting_category_type_game_head_hunter:
			return INVOKE(0x23ACDC, 0, multiplayer_variant_settings_interface_get_category_reference, _variant_setting_category_type_game_king);

		default:
			return INVOKE(0x23ACDC, 0, multiplayer_variant_settings_interface_get_category_reference, category_type);
	}

}

s_text_value_pair_definition* multiplayer_variant_settings_interface_get_text_value_pair_for_parameter(e_variant_setting_parameter_type parameter_type)
{
	return INVOKE(0x23ABCC, 0, multiplayer_variant_settings_interface_get_text_value_pair_for_parameter, parameter_type);
}

int32 __cdecl multiplayer_variant_settings_interface_get_variant_parameter_value(s_game_variant* variant, e_variant_setting_parameter_type type)
{
	return INVOKE(0x23AA54, 0, multiplayer_variant_settings_interface_get_variant_parameter_value, variant, type);
}

void __cdecl multiplayer_variant_settings_interface_set_variant_parameter_value(s_game_variant* variant, e_variant_setting_parameter_type parameter_type, int32 value)
{
	INVOKE(0x23A8C2, 0, multiplayer_variant_settings_interface_set_variant_parameter_value, variant, parameter_type, value);
}

s_text_value_pair_reference_new* __cdecl multiplayer_variant_settings_interface_get_variant_parameter_label(s_text_value_pair_definition* text_value_pair, int32 value)
{
	return INVOKE(0x23AD57, 0, multiplayer_variant_settings_interface_get_variant_parameter_label, text_value_pair, value);
}

void multiplayer_variant_settings_interface_get_custom_variant_parameter_label(s_game_variant* variant, e_variant_setting_parameter_type type, int32 value, wchar_t* out_string)
{
	switch(variant->variant_game_engine_index)
	{
		case _game_engine_type_headhunter:
		{
			multiplayer_variant_settings_interface_get_variant_parameter_label_headhunter(value, type, out_string);
			return;
		}
	}

	usnzprintf(out_string, 512, L"");
}
void multiplayer_variant_settings_interface_get_custom_variant_parameter_title(s_game_variant* variant, int32 index, wchar_t* out_string)
{
	switch (variant->variant_game_engine_index)
	{
		case _game_engine_type_headhunter:
		{
			usnzprintf(out_string, 512, g_multiplayer_variant_interface_headhunter_parameter_title_strings[get_current_language()][index]);
			return;
		}
	}

	usnzprintf(out_string, 512, L"");
}

void multiplayer_variant_settings_interface_get_custom_variant_parameter_title(s_game_variant* variant, e_variant_setting_parameter_type type, wchar_t* out_string)
{
	switch (variant->variant_game_engine_index)
	{
		case _game_engine_type_headhunter:
		{
			for (int32 i = 0; i < k_multiplayer_variant_headhunter_parameter_count; ++i)
			{
				if (g_multiplayer_variant_interface_headhunter_parameter_types[i] == type)
				{
					usnzprintf(out_string, 512, g_multiplayer_variant_interface_headhunter_parameter_title_strings[get_current_language()][i]);
					return;
				}
			}
		}
	}

	usnzprintf(out_string, 512, L"");
}

void multiplayer_variant_settings_interface_get_custom_variant_parameter_description(s_game_variant* variant,e_variant_setting_parameter_type type, wchar_t* out_string)
{
	switch (variant->variant_game_engine_index)
	{
		case _game_engine_type_headhunter:
		{
			for (int32 i = 0; i < k_multiplayer_variant_headhunter_parameter_count; ++i)
			{
				if (g_multiplayer_variant_interface_headhunter_parameter_types[i] == type)
				{
					usnzprintf(out_string, 512, g_multiplayer_variant_interface_headhunter_parameter_description_strings[get_current_language()][i]);
					return;
				}
			}
		}
	}

	usnzprintf(out_string, 512, L"");
}

int32 multiplayer_variant_settings_interface_get_custom_variant_parameter_value_count(s_game_variant* variant, e_variant_setting_parameter_type type)
{
	switch (variant->variant_game_engine_index)
	{
		case _game_engine_type_headhunter:
			return multiplayer_variant_settings_interface_get_variant_parameter_value_count_headhnter(type);
	}

	return 1;
}

bool multiplayer_variant_settings_interface_parameter_is_custom(s_game_variant* variant, e_variant_setting_parameter_type type)
{
	switch(variant->variant_game_engine_index)
	{
		case _game_engine_type_headhunter:
		{
			for (const auto headhunter_parameter_type : g_multiplayer_variant_interface_headhunter_parameter_types)
				if (headhunter_parameter_type == type)
				{
					// todo: construct a way to do this that doesn't require manually checking for these two types
					if(headhunter_parameter_type != _variant_setting_parameter_type_king_moving_hill && headhunter_parameter_type != _variant_setting_parameter_type_king_uncontested_hill)
						return true;
				}
		}
	}
	return false;
}
