#include "stdafx.h"
#include "game_variant.h"

#include "saved_game_files.h"
#include "game/game.h"
#include "text/text_group.h"

s_game_variant* get_game_variant(void)
{
	return &game_options_get()->game_variant;
}

void __cdecl game_variant_create_default_new(s_game_variant* variant, e_game_variant_description_index game_variant_type)
{
	if (game_variant_type < k_base_game_variant_description_count)
	{
		INVOKE(0x5B33D, 0x3CF9D, game_variant_create_default_new, variant, game_variant_type);
	}
    else
    {
		switch (game_variant_type)
		{
		case _game_variant_description_zombies:
			{
				break;
			}
		case _game_variant_description_headhunter:
			{
				variant->variant_game_engine_index = _game_engine_type_headhunter;
				variant->game_engine_flags = (e_game_engine_flags)(variant->game_engine_flags & ~0xFFEu | 0xFFA);
				variant->round_setting = _game_engine_round_setting_1_round;
				variant->round_time_limit = 0;
				variant->join_in_progress_setting = _game_engine_join_in_progress_on;

				memset((int8*)&variant->max_players, 0, 12);

				variant->respawn_time = 5;
				variant->suicide_penalty = 5;
				variant->shield_setting = _game_engine_shield_normal;
				variant->team_score_setting = _game_engine_team_score_sum;
				variant->team_respawn_setting = _game_engine_team_respawn_inheritance;
				variant->betrayal_penalty = 10;
				variant->unk = 0;

				memset((int8*)&variant->vehicle_respawn_setting, 0, 12);

				break;
			}
		}
    }
}
bool __cdecl game_variant_validate(s_game_variant* variant)
{
	// todo: rewrite validation function to include new variants
	return true;
    return INVOKE(0x5B720, 0x3D380, game_variant_validate, variant);
}