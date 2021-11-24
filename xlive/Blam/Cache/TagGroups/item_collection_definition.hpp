#pragma once
#include "Blam\Cache\DataTypes\BlamDataTypes.h"
#include "Blam\Cache\TagGroups.hpp"
#include "refl-cpp/refl-exentended.hpp"

/*********************************************************************
* name: item_collection
* group_tag : itmc
* header size : 12
* *********************************************************************/
struct s_item_collection_group_definition :TagGroup<'itmc'>
{
	struct s_item_permutations_block
	{
		float weight;//0x0
		tag_reference item;//0x4
		string_id variant_name;//0xC
	};
	TAG_BLOCK_SIZE_ASSERT(s_item_permutations_block, 0x10);
	tag_block<s_item_permutations_block> item_permutations;//0x0
	int spawn_time_in_seconds;//0x8
};
TAG_GROUP_SIZE_ASSERT(s_item_collection_group_definition, 0xC);

TAG_REFL_TAG_BLOCK_FLAT(s_item_collection_group_definition::s_item_permutations_block)

TAG_REFL(s_item_collection_group_definition)
	TAG_REFL_TAG_BLOCK(item_permutations)
REFL_END