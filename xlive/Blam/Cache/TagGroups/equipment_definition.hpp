#pragma once
#include "item_definition.hpp"
#include "refl-cpp/refl-cpp.hpp"
/*********************************************************************
* name: equipment
* group_tag : eqip
* header size : 316
* *********************************************************************/

#pragma pack(push,1)
struct s_equipment_group_definition :TagGroup<'eqip'>
{
	s_item_group_definition itemTag;
	enum class e_powerup_type : __int16
	{
		none = 0,
		double_speed = 1,
		over_shield = 2,
		active_camouflage = 3,
		fullspectrum_vision = 4,
		health = 5,
		grenade = 6,
	};
	e_powerup_type powerup_type;//0x12C
	enum class e_grenade_type : __int16
	{
		human_fragmentation = 0,
		covenant_plasma = 1,
	};
	e_grenade_type grenade_type;//0x12E
	float powerup_time;//0x130
	tag_reference pickup_sound;//0x134
};
TAG_GROUP_SIZE_ASSERT(s_equipment_group_definition, 0x13C);
#pragma pack(pop)

void debug_equipment(std::ostream& os, const s_equipment_group_definition& pt)
{
	
}

REFL_TYPE(s_equipment_group_definition, refl_impl::metadata::debug(debug_equipment), bases<>)
	REFL_FIELD(itemTag)
	REFL_FIELD(powerup_type, refl::attr::usage::field())
	REFL_FIELD(grenade_type, refl::attr::usage::field())
	REFL_FIELD(powerup_time, refl::attr::usage::field())
	REFL_FIELD(pickup_sound)
REFL_END