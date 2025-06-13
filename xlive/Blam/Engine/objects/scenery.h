#pragma once
#include "object_definition.h"
#include "objects.h"

/* enums */

enum e_scenery_pathfinding_policy : int16
{
	_scenery_pathfinding_policy_dynamic = 0,
	_scenery_pathfinding_policy_cutout = 1,
	_scenery_pathfinding_policy_static = 2,
	_scenery_pathfinding_policy_none = 3
};

enum e_scenery_definition_flags : uint16
{
	_scenery_definition_physically_simulates_bit = 0,	// stimulates
	k_scenery_definition_flag_count
};

enum e_scenery_lightmapping_policy : int16
{
	_scenery_lightmapping_policy_per_vertex = 0,
	_scenery_lightmapping_policy_per_pixel = 1,		// not implemented
	_scenery_lightmapping_policy_dynamic = 2,
	_scenery_lightmapping_policy_none = NONE
};

/* structures */

struct _scenery_definition
{
	// Explaination("Pathfinding", "Indicate whether, by default, we should create pathfinding data for this type of scenery")

	e_scenery_pathfinding_policy pathfinding_policy;
	c_flags_no_init<e_scenery_definition_flags, uint16, k_scenery_definition_flag_count> flags;

	// Explaination("Lightmapping", "Indicate whether, by default, how we should lightmap this type of scenery")

	e_scenery_lightmapping_policy lightmapping_policy;
	int16 pad;
};
ASSERT_STRUCT_SIZE(_scenery_definition, 8);

struct scenery_definition
{
	_object_definition object;
	_scenery_definition scenery;
};
ASSERT_STRUCT_SIZE(scenery_definition, 196);


struct _scenery_datum
{
	uint32 flags;
	int16 field_4;
	e_scenery_pathfinding_policy pathfinding_policy;
	int32 lightmapping_policy;	// e_scenery_lightmapping_policy
	int32 field_C;
};

struct scenery_datum
{
	datum definition_index;
	_object_datum object;
	_scenery_datum scenery;
};
ASSERT_STRUCT_SIZE(scenery_datum, 316)