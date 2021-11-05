#pragma once
#include "object_definition.hpp"
/*********************************************************************
* name: item
* group_tag : item
* header size : 300
* *********************************************************************/

#pragma pack(push,1)
struct s_item_group_definition :TagGroup<'item'>
{
	s_object_group_definition objectTag;
	enum class e_flags : __int32
	{
		always_maintains_z_up = FLAG(0),
		destroyed_by_explosions = FLAG(1),
		unaffected_by_gravity = FLAG(2),
	};
	e_flags flags;//0xBC
	__int16 old_message_index;//0xC0
	__int16 sort_order;//0xC2
	float multiplayer_onground_scale;//0xC4
	float campaign_onground_scale;//0xC8
	string_id pickup_message;//0xCC
	string_id swap_message;//0xD0
	string_id pickup_or_dual_msg;//0xD4
	string_id swap_or_dual_msg;//0xD8
	string_id dualonly_msg;//0xDC
	string_id picked_up_msg;//0xE0
	string_id singluar_quantity_msg;//0xE4
	string_id plural_quantity_msg;//0xE8
	string_id switchto_msg;//0xEC
	string_id switchto_from_ai_msg;//0xF0
	tag_reference unused;//0xF4
	tag_reference collision_sound;//0xFC
	struct s_predicted_bitmaps_block
	{
		tag_reference bitmap;//0x0
	};
	TAG_BLOCK_SIZE_ASSERT(s_predicted_bitmaps_block, 0x8);
	tag_block<s_predicted_bitmaps_block> predicted_bitmaps;//0x104
	tag_reference detonation_damage_effect;//0x10C
	real_bounds detonation_delay;//0x114
	
	tag_reference detonating_effect;//0x11C
	tag_reference detonation_effect;//0x124
};
TAG_GROUP_SIZE_ASSERT(s_item_group_definition, 0x12C);
#pragma pack(pop)

TAG_REFL(s_item_group_definition)
	TAG_REFL_BASE_STRUCT(objectTag)
	TAG_REFL_PROPERTY(flags)
	TAG_REFL_PROPERTY(old_message_index)
	TAG_REFL_PROPERTY(sort_order)
	TAG_REFL_PROPERTY(multiplayer_onground_scale)
	TAG_REFL_PROPERTY(campaign_onground_scale)
	TAG_REFL_STRING_ID(pickup_message)
	TAG_REFL_STRING_ID(swap_message)
	TAG_REFL_STRING_ID(pickup_or_dual_msg)
	TAG_REFL_STRING_ID(dualonly_msg)
	TAG_REFL_STRING_ID(picked_up_msg)
	TAG_REFL_STRING_ID(singluar_quantity_msg)
	TAG_REFL_STRING_ID(plural_quantity_msg)
	TAG_REFL_STRING_ID(switchto_msg)
	TAG_REFL_STRING_ID(switchto_from_ai_msg)
	TAG_REFL_TAG_REFERENCE(unused)
	TAG_REFL_TAG_REFERENCE(collision_sound)
	TAG_REFL_TAG_BLOCK(predicted_bitmaps)
	TAG_REFL_TAG_BLOCK(detonation_damage_effect)
	TAG_REFL_REAL_BOUNDS(detonation_delay)
	TAG_REFL_TAG_REFERENCE(detonating_effect)
	TAG_REFL_TAG_REFERENCE(detonation_effect)
REFL_END

TAG_REFL(s_item_group_definition::s_predicted_bitmaps_block)
	TAG_REFL_TAG_REFERENCE(bitmap)
REFL_END