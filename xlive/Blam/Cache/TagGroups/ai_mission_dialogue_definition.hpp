#pragma once

#include "Blam\Cache\TagGroups.hpp"
#include "Blam\Cache\DataTypes\BlamDataTypes.h"
#include "refl-cpp/refl-exentended.hpp"

/*********************************************************************
* name: ai_mission_dialogue
* group_tag : mdlg
* header size : 8
* *********************************************************************/

#pragma pack(push,1)
struct s_ai_mission_dialogue_group_definition :TagGroup<'mdlg'>
{
	struct s_lines_block
	{
		string_id name;//0x0
		struct s_variants_block
		{
			string_id variant_designation;//0x0
			tag_reference sound;//0x4
			string_id sound_effect;//0xC
		};
		TAG_BLOCK_SIZE_ASSERT(s_variants_block, 0x10);
		tag_block<s_variants_block> variants;//0x4
		string_id default_sound_effect;//0xC
	};
	TAG_BLOCK_SIZE_ASSERT(s_lines_block, 0x10);
	tag_block<s_lines_block> lines;//0x0
};
#pragma pack(pop)
TAG_GROUP_SIZE_ASSERT(s_ai_mission_dialogue_group_definition, 0x8);

TAG_REFL_TAG_BLOCK_DEF(s_ai_mission_dialogue_group_definition::s_lines_block::s_variants_block)
	TAG_REFL_STRING_ID(variant_designation)
	TAG_REFL_TAG_REFERENCE(sound)
	TAG_REFL_STRING_ID(sound_effect)
REFL_END

TAG_REFL_TAG_BLOCK_DEF(s_ai_mission_dialogue_group_definition::s_lines_block)
	TAG_REFL_STRING_ID(name)
	TAG_REFL_TAG_BLOCK(variants)
	TAG_REFL_STRING_ID(default_sound_effect)
REFL_END

TAG_REFL(s_ai_mission_dialogue_group_definition)
	TAG_REFL_TAG_BLOCK(lines)
REFL_END
