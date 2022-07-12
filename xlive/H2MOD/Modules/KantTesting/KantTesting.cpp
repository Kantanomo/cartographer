#include "stdafx.h"

#include "KantTesting.h"
#include "Blam\Cache\DataTypes\BlamPrimitiveType.h"
#include "Blam\Cache\TagGroups\biped_definition.hpp"
#include "Blam\Cache\TagGroups\globals_definition.hpp"
#include "Blam\Cache\TagGroups\model_definition.hpp"
#include "Blam\Cache\TagGroups\scenario_definition.hpp"
#include "Blam\Cache\TagGroups\scenario_lightmap_definition.hpp"
#include "Blam\Cache\TagGroups\scenario_structure_bsp_definition.hpp"
#include "Blam\Cache\TagGroups\weapon_definition.hpp"
#include "Blam\Engine\Game\GameEngineGlobals.h"
#include "Blam\Engine\Game\GameGlobals.h"
#include "Blam\Engine\Players\Players.h"
#include "Blam\LazyBlam\LazyBlam.hpp"
#include "H2MOD\Engine\Engine.h"
#include "H2MOD\Modules\Shell\Config.h"
#include "H2MOD\Modules\EventHandler\EventHandler.hpp"
#include "Blam\Engine\Memory\bitstream.h"
#include "Blam\Engine\Memory\bitstream.h"
#include "Blam/Enums/HaloStrings.h"
#include "H2MOD/Modules/DirectorHooks/DirectorHooks.h"
#include "H2MOD/Modules/HaloScript/HaloScript.h"
#include "H2MOD/Modules/Input/KeyboardInput.h"
#include "H2MOD/Modules/ObserverMode/ObserverMode.h"
#include "H2MOD\Modules\PlayerRepresentation\PlayerRepresentation.h"
#include "H2MOD\Tags\MetaExtender.h"
#include "H2MOD\Tags\MetaLoader\tag_loader.h"
#include "Util\Hooks\Hook.h"


namespace KantTesting
{
	const std::string weat_tag_path = "scenarios\\multi\\lockout\\lockout_big";
	datum w_datum;
	void MapLoad()
	{
		if (h2mod->GetEngineType() == _multiplayer)
		{
			/*
			auto w_datum_i = tag_loader::Get_tag_datum(weat_tag_path, blam_tag::tag_group_type::weathersystem, "carto_shared");
			if (!DATUM_IS_NONE(w_datum_i))
			{
				tag_loader::Load_tag(w_datum_i, true, "carto_shared");
				tag_loader::Push_Back();
				w_datum = tag_loader::ResolveNewDatum(w_datum_i);
				if (!DATUM_IS_NONE(w_datum))
				{
					auto scen = tags::get_tag_fast<s_scenario_group_definition>(tags::get_tags_header()->scenario_datum);
					auto sbsp = tags::get_tag_fast<s_scenario_structure_bsp_group_definition>(scen->structure_bsps[0]->structure_bsp.TagIndex);

					auto weat_block = MetaExtender::add_tag_block2<s_scenario_structure_bsp_group_definition::s_weather_palette_block>((unsigned long)std::addressof(sbsp->weather_palette));
					weat_block->name = "snow_cs";
					weat_block->weather_system.TagGroup = blam_tag::tag_group_type::weathersystem;
					weat_block->weather_system.TagIndex = w_datum;

					for (auto& cluster : sbsp->clusters)
					{
						cluster.weather = sbsp->weather_palette.size - 1;
					}

				}
			}*/
			auto mode_chief_mp_datum = tags::find_tag(blam_tag::tag_group_type::model, "objects\\characters\\masterchief\\masterchief_mp");
			auto mode_chief_mp = tags::get_tag<blam_tag::tag_group_type::model, s_model_group_definition>(mode_chief_mp_datum);
			auto base_variant = mode_chief_mp->variants[0];
			auto new_variant = MetaExtender::add_tag_block2<s_model_group_definition::s_variants_block>((unsigned long)std::addressof(mode_chief_mp->variants));
			new_variant->name = 0xABABABA;
			new_variant->dialogue.TagGroup = base_variant->dialogue.TagGroup;
			new_variant->dialogue.TagIndex = base_variant->dialogue.TagIndex;
			new_variant->runtime_model_region_0 = base_variant->runtime_model_region_0;
			new_variant->runtime_model_region_1 = base_variant->runtime_model_region_1;
			new_variant->runtime_model_region_2 = base_variant->runtime_model_region_2;
			new_variant->runtime_model_region_3 = base_variant->runtime_model_region_3;
			new_variant->runtime_model_region_4 = base_variant->runtime_model_region_4;
			new_variant->runtime_model_region_5 = base_variant->runtime_model_region_5;
			new_variant->runtime_model_region_6 = base_variant->runtime_model_region_6;
			new_variant->runtime_model_region_7 = base_variant->runtime_model_region_7;
			new_variant->runtime_model_region_8 = base_variant->runtime_model_region_8;
			new_variant->runtime_model_region_9 = base_variant->runtime_model_region_9;
			new_variant->runtime_model_region_10 = base_variant->runtime_model_region_10;
			new_variant->runtime_model_region_11 = base_variant->runtime_model_region_11;
			new_variant->runtime_model_region_12 = base_variant->runtime_model_region_12;
			new_variant->runtime_model_region_13 = base_variant->runtime_model_region_13;
			new_variant->runtime_model_region_14 = base_variant->runtime_model_region_14;
			new_variant->runtime_model_region_15 = base_variant->runtime_model_region_15;
			for (auto i = 0; i < base_variant->regions.size; i++)
			{
				auto region = base_variant->regions[i];
				auto new_region = MetaExtender::add_tag_block2<s_model_group_definition::s_variants_block::s_regions_block>((unsigned long)std::addressof(new_variant->regions));
				new_region->region_name = region->region_name;
				new_region->runtime_model_region_index = region->runtime_model_region_index;
				new_region->region_runtime_flags = region->region_runtime_flags;
				new_region->parent_variant = region->parent_variant;
				new_region->sort_order = region->sort_order;
				for (auto k = 0; k < region->permutations.size; k++)
				{
					auto permutation = region->permutations[k];
					auto new_permutation = MetaExtender::add_tag_block2<s_model_group_definition::s_variants_block::s_regions_block::s_permutations_block>((unsigned long)std::addressof(new_region->permutations));
					new_permutation->permutation_name = permutation->permutation_name;
					new_permutation->model_permutation_index = permutation->model_permutation_index;
					new_permutation->flags = permutation->flags;
					new_permutation->probability_0 = permutation->probability_0;
					new_permutation->runtime_permutation_index_0 = permutation->runtime_permutation_index_0;
					new_permutation->runtime_permutation_index_1 = permutation->runtime_permutation_index_1;
					new_permutation->runtime_permutation_index_2 = permutation->runtime_permutation_index_2;
					new_permutation->runtime_permutation_index_3 = permutation->runtime_permutation_index_3;
					new_permutation->runtime_permutation_index_4 = permutation->runtime_permutation_index_4;
					new_permutation->unk_1 = permutation->unk_1;
					new_permutation->unk2 = permutation->unk2;
					new_permutation->unk3 = permutation->unk3;
				}
			}

			auto e_datum_i = tag_loader::Get_tag_datum("scenarios\\objects\\multi\\carto_shared\\emoji_head\\emoji_head", blam_tag::tag_group_type::scenery, "carto_shared");
			if (!DATUM_IS_NONE(e_datum_i)) 
			{
				tag_loader::Load_tag(e_datum_i, true, "carto_shared");
				tag_loader::Push_Back();
				auto e_datum = tag_loader::ResolveNewDatum(e_datum_i);
				if (!DATUM_IS_NONE(e_datum)) 
				{
					auto new_object = MetaExtender::add_tag_block2<s_model_group_definition::s_variants_block::s_objects_block>((unsigned long)std::addressof(new_variant->objects));
					new_object->parent_marker = string_id(184552154);
					new_object->child_object.TagGroup = blam_tag::tag_group_type::scenery;
					new_object->child_object.TagIndex = e_datum;
				}
			}
			auto repb = player_representation::add_representation(-1, -1, -1, s_player::e_character_type::Kant, new_variant->name);
			//auto rep = player_representation::clone_representation(2, s_player::e_character_type::Kant);
			//rep->third_person_variant = new_variant->name;
		}
	}


	typedef int(__cdecl t_objects_in_sphere)(int a1, int object_type, DWORD* a3, real_point3d* a4, real_point2d* a5, int* a6, __int16 a7);
	t_objects_in_sphere* c_objects_in_sphere;

	typedef bool(__cdecl t_objects_can_see_objects_internal)(datum observer, datum target, float angle);
	t_objects_can_see_objects_internal* c_objects_can_see_objects_internal;

	typedef void(__cdecl t_object_attach_to_marker)(datum target, string_id target_marker, datum object, string_id object_marker);
	t_object_attach_to_marker* c_object_attach_to_marker;

	typedef void(__cdecl t_objects_detach)(datum object_index);
	t_objects_detach* c_objects_detach;

	typedef void(__cdecl t_object_set_velocities)(datum index, real_vector3d* translation, real_point3d* angular);
	t_object_set_velocities* c_object_set_velocities;
	auto tran = new real_vector3d{ 0, 50, 0 };
	auto angu = new real_point3d{ 0,20,0 };

	typedef void(__cdecl t_item_maintains_z_up)(datum index);
	t_item_maintains_z_up* c_item_maintains_z_up;
	datum current_object;
	void __cdecl examine_objects_nearby(int player_index)
	{
		auto player = s_player::GetPlayer(player_index);
		if(!DATUM_IS_NONE(player->unit_index))
		{
			auto unit = object_get_fast_unsafe<s_biped_data_definition>(player->unit_index);
			if (DATUM_IS_NONE(unit->parent_datum))
			{
				auto searchRadius = unit->radius + 0.8f;

				real_point2d unk_point = { searchRadius, searchRadius + 3 };
				int unk_loc[2] = { 4, -5 };
				DWORD* location = unit->location;
				real_point3d* center = &unit->center;

				auto iter = 0;
				int output[64];
				/*do
				{*/
					auto found = c_objects_in_sphere(0, 0, location, center, &unk_point, output, 64);
					if(found > 0)
					{
						LOG_INFO_GAME("{} found {} objects", __FUNCTION__, found);
						for(auto i = 0; i < found; i++)
						{
							auto object = object_get_fast_unsafe<s_object_data_definition>(output[i]);
							auto object_type = 1 << object->object_type;
							if((object_type & (FLAG(e_object_type::creature | e_object_type::sound_scenery | e_object_type::projectile | e_object_type::garbage))) == 0)
							{

								auto distance = *center - object->center;
								auto a = (object->radius + searchRadius) * (object->radius + searchRadius);
								auto b = distance.x * distance.x;
								auto c = distance.y * distance.y;
								auto d = distance.z * distance.z;

								if (a >= b + c + d) 
								{
									switch (object->object_type)
									{
									case vehicle:
									case weapon:
									case equipment:
									case scenery:
									case crate:
									case creature:
										LOG_INFO_GAME("{} found valid object {:x} {}", __FUNCTION__, output[i], tags::get_tag_name(object->tag_definition_index));
										if(c_objects_can_see_objects_internal(player->unit_index, output[i], 30))
										{
											LOG_INFO_GAME("{} Looking at object {:x} {}", __FUNCTION__, output[i], tags::get_tag_name(object->tag_definition_index));
											//c_object_attach_to_marker(player->unit_index, 0, output[i], 0);
											ObserverMode::SwitchObserverMode(ObserverMode::observer_followcam);
											ObserverMode::SetTarget(output[i]);
											current_object = output[i];
											HaloScript::ObjectSetVelocity(output[i], 10, 0, 0);
											//c_object_attach_to_marker(output[i], 0, player->unit_index, 0);
										}
										break;
									default:;
									}
								}
							}
						}
					}
				//}
			}
		}
	}
	int a = VK_NUMPAD0;
	int b = VK_NUMPAD1;
	int forward = VK_NUMPAD8;
	int back = VK_NUMPAD2;
	int left = VK_NUMPAD4;
	int right = VK_NUMPAD6;
	void moveobject(int dir)
	{
		switch(dir)
		{
		case 0:
			HaloScript::ObjectSetVelocity(current_object, 10, 0, 0);
			c_item_maintains_z_up(current_object);
			break;
		case 1:
			HaloScript::ObjectSetVelocity(current_object, 0, 10, 0);
			c_item_maintains_z_up(current_object);
			break;
		case 2:
			HaloScript::ObjectSetVelocity(current_object, 0, -10, 0);
			c_item_maintains_z_up(current_object);
			break;
		case 3:
			HaloScript::ObjectSetVelocity(current_object, -10, 0, 0);
			c_item_maintains_z_up(current_object);
			break;
		}
	}
	void Initialize()
	{
		if (ENABLEKANTTEST) {

			c_objects_in_sphere = Memory::GetAddress<t_objects_in_sphere*>(0x1331BA);
			c_objects_can_see_objects_internal = Memory::GetAddress<t_objects_can_see_objects_internal*>(0xFDBD5);
			c_object_attach_to_marker = Memory::GetAddress<t_object_attach_to_marker*>(0x1381E1);
			c_objects_detach = Memory::GetAddress<t_objects_detach*>(0x137A84);
			c_object_set_velocities = Memory::GetAddress<t_object_set_velocities*>(0x135123);
			c_item_maintains_z_up = Memory::GetAddress<t_item_maintains_z_up*>(0x181349);
			KeyboardInput::RegisterHotkey(&a, [] { examine_objects_nearby(0); });
			KeyboardInput::RegisterHotkey(&b, []
				{
					DirectorHooks::SetDirectorMode(DirectorHooks::e_firstperson);
					/*auto player = s_player::GetPlayer(0);
					if (!DATUM_IS_NONE(player->unit_index))
					{
						auto unit = object_get_fast_unsafe<s_biped_data_definition>(player->unit_index);
						if (!DATUM_IS_NONE(unit->parent_datum))
						{
							c_objects_detach(DATUM_INDEX_TO_ABSOLUTE_INDEX(player->unit_index));
						}
					}*/
				});
			KeyboardInput::RegisterHotkey(&forward,[]{ moveobject(0); });
			KeyboardInput::RegisterHotkey(&back, [] { moveobject(3); });
			KeyboardInput::RegisterHotkey(&left, [] { moveobject(1); });
			KeyboardInput::RegisterHotkey(&right, [] { moveobject(2); });
		//	if (!Memory::isDedicatedServer())
			//{
			//tags::on_map_load(MapLoad);
		//	}
		}
	}
}
