#pragma once
#include "..\Blam\Cache\TagGroups.hpp"
#include "Blam\Cache\DataTypes\BlamDataTypes.h"
#include "Blam/Math/real_math.h"
#include "refl-cpp/refl-cpp.hpp"
#include "refl-cpp/refl-exentended.hpp"

/*********************************************************************
* name: antenna
* group_tag : ant!
* header size : 160
* *********************************************************************/

#pragma pack(push,1)
struct s_antenna_group_definition :TagGroup<'ant!'>
{
	string_id attachment_marker_name;//0x0
	tag_reference bitmaps;//0x4
	tag_reference physics;//0xC
	PAD(0x50);//0x14
	float spring_strength_coefficient;//0x64
	float falloff_pixels;//0x68
	float cutoff_pixels;//0x6C
	PAD(0x28);//0x70
	struct s_vertices_block
	{
		float spring_strength_coefficient;//0x0
		PAD(0x18);//0x4
		angle angles_yaw;//0x1C
		angle angles_pitch;//0x20
		float length;//0x24
		__int16 sequence_index;//0x28
		PAD(0x2);//0x2A
		real_color_argb color;//0x2C
		real_color_argb lod_color;//0x3C
		PAD(0x34);//0x4C
	};
	TAG_BLOCK_SIZE_ASSERT(s_vertices_block, 0x80);
	tag_block<s_vertices_block> vertices;//0x98
};
TAG_GROUP_SIZE_ASSERT(s_antenna_group_definition, 0xA0);

#pragma pack(pop)

TAG_REFL_TAG_BLOCK_DEF(s_antenna_group_definition::s_vertices_block)
	TAG_REFL_PROPERTY(spring_strength_coefficient)
	TAG_REFL_ANGLE(angles_yaw)
	TAG_REFL_ANGLE(angles_pitch)
	TAG_REFL_PROPERTY(length)
	TAG_REFL_PROPERTY(sequence_index)
	TAG_REFL_REAL_COLOR_ARGB(color)
	TAG_REFL_REAL_COLOR_ARGB(lod_color)
REFL_END

TAG_REFL(s_antenna_group_definition)
	TAG_REFL_STRING_ID(attachment_marker_name)
	TAG_REFL_TAG_REFERENCE(bitmaps)
	TAG_REFL_TAG_REFERENCE(physics)
	TAG_REFL_PROPERTY(spring_strength_coefficient)
	TAG_REFL_PROPERTY(falloff_pixels)
	TAG_REFL_PROPERTY(cutoff_pixels)
	TAG_REFL_TAG_BLOCK(vertices)
REFL_END
