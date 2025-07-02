#include "stdafx.h"
#include "scenario.h"

#include "scenario_definitions.h"

#include "cache/cache_files.h"
#include "creatures/creature_definitions.h"
#include "physics/collision_model_definitions.h"
#include "physics/physics_model_definitions.h"
#include "rasterizer/dx9/rasterizer_dx9_vertex_shaders.h"
#include "structures/structure_bsp_definitions.h"
#include "units/biped_definitions.h"
#include "units/vehicle_definitions.h"

/* private prototypes */

static void __cdecl scenario_tags_postprocess(void);

static void __cdecl scenario_apply_level_patches(void);

/* public code */

scenario* global_scenario_get(void) 
{
	return *Memory::GetAddress<scenario**>(0x479E74, 0x4A6430);
}

void set_global_scenario(scenario* _scenario)
{
	*Memory::GetAddress<scenario**>(0x479E74, 0x4A6430) = _scenario;
}

collision_bsp* global_collision_bsp_get(void)
{
	return *Memory::GetAddress<collision_bsp**>(0x479E64, 0x4A6420);
}

void scenario_apply_patches(void)
{
	PatchCall(Memory::GetAddress(0x28516), scenario_tags_postprocess);	// Default Maps
	PatchCall(Memory::GetAddress(0x284DE), scenario_tags_postprocess);	// Custom Maps
	return;
}

uint32 scenario_netgame_equipment_size(void)
{
	return global_scenario_get()->netgame_equipment.count;
}

void location_invalidate(s_location* object_location)
{
	object_location->leaf_index = NONE;
	object_location->cluster_index = NONE;
	object_location->bsp_index = get_global_structure_bsp_index();
	return;
}

void __cdecl scenario_location_from_point(s_location* location, real_point3d* point)
{
	INVOKE(0x281EE, 0x30CB1, scenario_location_from_point, location, point);
	return;
}

bool __cdecl scenario_location_underwater(s_location* location, real_point3d* point, int16* global_material_index)
{
	return INVOKE(0x27A03, 0x304C6, scenario_location_underwater, location, point, global_material_index);
}

void __cdecl scenario_location_from_leaf(s_location* location, int32 leaf_index)
{
	INVOKE(0x2819D, 0x30C60, scenario_location_from_leaf, location, leaf_index);
	return;
}

/* private code */

static void __cdecl scenario_tags_postprocess(void)
{
	tag_iterator itr;
	tag_iterator_new(&itr, _tag_group_none);
	
	for (datum i = tag_iterator_next(&itr); i != NONE; i = tag_iterator_next(&itr))
	{
		tag_group group = tag_instance_get(i)->group_tag;
		switch (group.group)
		{
		case _tag_group_biped:
		{
			biped_definitions_fixup(i);
			break;
		}
		case _tag_group_creature:
		{
			creature_definitions_fixup(i);
			break;
		}
		case _tag_group_vehicle:
		{
			vehicle_definitions_fixup(i);
			break;
		}
		case _tag_group_collision_model:
		{
			collision_model_definitions_fixup(i);
			break;
		}
		case _tag_group_physics_model:
		{
			physics_model_definitions_fixup(i, false);
			break;
		}
		case _tag_group_vertex_shader:
		{
			//rasterizer_dx9_vertex_shaders_replace_map_bytecode(i);
			break;
		}
		default:
			break;
		}

	}
	scenario_apply_level_patches();
	game_engine_apply_map_patches();
	return;
}

static void __cdecl scenario_apply_level_patches(void)
{
	return INVOKE(0x27EDD, 0x0, scenario_apply_level_patches);
}
