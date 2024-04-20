#include "stdafx.h"

#include "Mook.h"
#include "../SpecialEventHelpers.h"

#include "items/weapon_definitions.h"
#include "tag_files/tag_loader/tag_injection.h"


void mook_event_map_load()
{
	datum ball_weapon_datum = tags::find_tag(_tag_group_weapon, "objects\\weapons\\multiplayer\\ball\\ball");
	datum bomb_weapon_datum = tags::find_tag(_tag_group_weapon, "objects\\weapons\\multiplayer\\assault_bomb\\assault_bomb");
	datum mook_ball_weapon_datum = tag_loader::get_tag_datum_by_name ("scenarios\\objects\\multi\\carto_shared\\basketball\\basketball", _tag_group_weapon, "carto_shared");

	tag_injection_set_active_map(k_events_map);
	if (!tag_injection_active_map_verified())
		return;

	datum mook_ball_weapon_datum = tag_injection_load(_tag_group_weapon, "scenarios\\objects\\multi\\carto_shared\\basketball\\basketball", true);

	if (mook_ball_weapon_datum != NONE && ball_weapon_datum != NONE && bomb_weapon_datum != NONE)
	{
		tag_loader::preload_tag_data_from_cache(mook_ball_weapon_datum, true, "carto_shared");
		tag_loader::push_loaded_tag_data();

		mook_ball_weapon_datum = tag_loader::resolve_cache_index_to_injected(mook_ball_weapon_datum);

		auto mook_ball_weapon = tags::get_tag<_tag_group_weapon, _weapon_definition>(mook_ball_weapon_datum, true);

		replace_fp_and_3p_models_from_weapon(ball_weapon_datum, mook_ball_weapon->item.object.model.index, mook_ball_weapon->item.object.model.index);
		replace_fp_and_3p_models_from_weapon(bomb_weapon_datum, mook_ball_weapon->item.object.model.index, mook_ball_weapon->item.object.model.index);
	}
}
