#include "stdafx.h"

#include "KantTesting.h"

#include "Blam/Engine/Objects/ObjectGlobals.h"
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

	typedef void(__cdecl t_object_set_position_direct)(datum object_index, real_vector3d* forward, real_point3d* position, real_vector3d* up, DWORD* unk);
	t_object_set_position_direct* c_object_set_position_direct;

	typedef void(__cdecl t_object_set_in_limbo)(datum object_index, bool enable);
	t_object_set_in_limbo* c_object_set_in_limbo;

	typedef void(__cdecl t_object_activate)(datum object_index);
	t_object_activate* c_object_activate;

	datum current_object;
	real_point3d current_location;
	void main_z(datum index, EventHandler::e_object_update_event type)
	{
		if(index == current_object)
		{
			auto object = object_get_fast_unsafe<s_object_data_definition>(index);
			object->up = *Memory::GetAddress<real_point3d*>(0x41272C);

			auto c1 = (object->orientation.k * object->up.j) - (object->orientation.j * object->up.k);
			auto c2 = (object->orientation.i * object->up.k) - (object->up.i * object->orientation.k);
			auto c3 = (object->up.i * object->orientation.j) - (object->orientation.i * object->up.j);

			auto p1 = (object->up.j * c1) - (object->up.i * c2);
			auto p2 = (object->up.i * c3) - (object->up.k * c1);
			auto p3 = (object->up.k * c2) - (object->up.j * c3);
			object->orientation.i = p3;
			object->orientation.j = p2;
			object->orientation.k = p1;

			if (normalize3d(&object->orientation) == 0.0f)
				object->orientation = *Memory::GetAddress<real_vector3d*>(0x412724);
			object->position.z = 2;
			c_object_set_position_direct(index, &current_location, &object->orientation, &object->up, 0);
		}
	}
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
											current_location = object_get_fast_unsafe<s_object_data_definition>(current_object)->position;
											EventHandler::register_callback(main_z, EventType::object_update, EventExecutionType::execute_after);
											//HaloScript::ObjectSetVelocity(output[i], 10, 0, 0);
											//auto object = object_get_fast_unsafe<s_object_data_definition>(output[i]);
											//c_object_set_position_direct(current_object, &object->orientation, &object->position, &object->up, 0);
											//c_object_attach_to_marker(output[i], 0, player->unit_index, 0);
											//c_object_activate(player->unit_index);

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
	typedef void(__cdecl t_get_early_moving_objects)(int* data, int* count);
	t_get_early_moving_objects* c_get_early_moving_objects;

	typedef void(__cdecl t_object_update)(datum object_index);
	t_object_update* c_object_update;

	typedef void(__cdecl t_early_moving_update_unk)(datum object_index);
	t_early_moving_update_unk* c_early_moving_update_unk;

	typedef void(__cdecl t_object_move)(datum object_index);
	t_object_move* c_object_move;

	typedef void(__cdecl t_weapon_fire_barrels_eval)();
	t_weapon_fire_barrels_eval* c_weapon_fire_barrels_eval;

	typedef void(__cdecl t_object_pre_delete_recursive)(datum object_index);
	t_object_pre_delete_recursive* c_object_pre_delete_recursive;

	typedef void(__cdecl t_object_delete_recursive)(datum object_index, bool deactivate_first);
	t_object_delete_recursive* c_object_delete_recursive;

	typedef void(__cdecl t_objects_garbage_collection)();
	t_objects_garbage_collection* c_objects_garbage_collection;

	void __cdecl objects_update()
	{
		int early_moving_data;
		int early_moving_count;
		c_get_early_moving_objects(&early_moving_data, &early_moving_count);

		s_object_globals::get()->objects_updating = true;

		for (auto i = 0; i < early_moving_count; i++)
		{
			auto datum_index = *(datum*)(early_moving_data + 4 * i);
			auto header = get_objects_header(datum_index);
			auto flags = header->flags;
			if ((flags & FLAG(e_object_header_flag::_object_header_active_bit)) != 0 &&
				(flags & FLAG(e_object_header_flag::_object_header_requires_motion_bit)) != 0 &&
				(flags & FLAG(e_object_header_flag::_object_header_being_deleted_bit)) == 0)
			{
				ObjectUpdateEventExecute(EventExecutionType::execute_before, datum_index, EventHandler::_e_object_update_event_early_mover);
				c_object_update(datum_index);
				ObjectUpdateEventExecute(EventExecutionType::execute_after, datum_index, EventHandler::_e_object_update_event_early_mover);

				flags = header->flags;
				if ((flags & FLAG(e_object_header_flag::object_header_flags_4)) != 0 && 
					(flags & FLAG(e_object_header_flag::_object_header_being_deleted_bit)) == 0)
				{
					c_early_moving_update_unk(datum_index);
				}
			}
		}

		s_data_iterator<s_object_header> iterator(get_objects_header());
		auto current = iterator.get_next_datum();
		if(current)
		{
			do
			{
				auto flags = current->flags;
				auto object = object_get_fast_unsafe<s_object_data_definition>(iterator.get_current_datum_index());
				if(current->type == projectile)
				{
					LOG_INFO_GAME("[{}] {}", __FUNCTION__, current->flags);
				}
				if ((flags & FLAG(e_object_header_flag::_object_header_active_bit)) != 0 &&
					(flags & FLAG(e_object_header_flag::_object_header_requires_motion_bit)) != 0 &&
					(flags & FLAG(e_object_header_flag::_object_header_being_deleted_bit)) == 0 &&
					(object->field_C0 & FLAG(8)) == 0)
				{
					ObjectUpdateEventExecute(EventExecutionType::execute_before, iterator.get_current_datum_index(), EventHandler::_e_object_update_event_default);
					c_object_update(iterator.get_current_datum_index());
					ObjectUpdateEventExecute(EventExecutionType::execute_after, iterator.get_current_datum_index(), EventHandler::_e_object_update_event_default);
				}
				current = iterator.get_next_datum();
			} while (current);
			s_object_globals::get()->objects_updating = false;
		}
		else
		{
			s_object_globals::get()->objects_updating = false;
		}
	}

	void __cdecl objects_post_update()
	{
		s_object_globals::get()->objects_updating = true;
		s_data_iterator<s_object_header> iterator(get_objects_header());
		auto current = iterator.get_next_datum();
		if(current)
		{
			do
			{
				auto index = iterator.get_current_datum_index();
				current->flags = static_cast<e_object_header_flag>(current->flags & (
					FLAG(_object_header_child_bit) |
					FLAG(object_header_flags_10) |
					FLAG(_object_header_being_deleted_bit) |
					FLAG(object_header_flags_4) |
					FLAG(_object_header_requires_motion_bit) |
					FLAG(_object_header_active_bit) |
					FLAG(_object_header_unk_bit)));

				auto flags = current->flags;
				if((flags & FLAG(_object_header_being_deleted_bit)) != 0 &&
					(flags & FLAG(_object_header_active_bit)) != 0 &&
					(flags & FLAG(_object_header_requires_motion_bit)) != 0 &&
					(flags & FLAG(object_header_flags_10)) == 0)
				{
					current->flags = static_cast<e_object_header_flag>(current->flags & (
						FLAG(_object_header_child_bit) |
						FLAG(_object_header_connected_to_map_bit) |
						FLAG(object_header_flags_10) |
						FLAG(object_header_flags_4) |
						FLAG(_object_header_requires_motion_bit) |
						FLAG(_object_header_active_bit) |
						FLAG(_object_header_unk_bit)));
					ObjectUpdateEventExecute(EventExecutionType::execute_before, index, EventHandler::_e_object_update_event_post);
					c_object_update(index);
					ObjectUpdateEventExecute(EventExecutionType::execute_before, index, EventHandler::_e_object_update_event_post);
					if ((current->flags & FLAG(object_header_flags_4)) != 0)
						c_object_move(index);
				}

				current = iterator.get_next_datum();
			} while (current);
		}

		c_weapon_fire_barrels_eval();
		iterator.reset();
		current = iterator.get_next_datum();
		if (current)
		{
			do
			{
				auto index = iterator.get_current_datum_index();
				if((current->flags & FLAG(object_header_flags_10)) != 0)
				{
					c_object_pre_delete_recursive(index);
					c_object_delete_recursive(index, 1);
				}
				current = iterator.get_next_datum();
			}
			while (current);
		}
		s_object_globals::get()->objects_updating = false;
		c_objects_garbage_collection();
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
			c_object_set_position_direct = Memory::GetAddress<t_object_set_position_direct*>(0x136B7F);
			c_object_set_in_limbo = Memory::GetAddress<t_object_set_in_limbo*>(0x136355);
			c_object_activate = Memory::GetAddress<t_object_activate*>(0x13204A);

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

			c_get_early_moving_objects = Memory::GetAddress<t_get_early_moving_objects*>(0x14BA08);
			c_object_update = Memory::GetAddress<t_object_update*>(0x1352A9);
			c_early_moving_update_unk = Memory::GetAddress<t_early_moving_update_unk*>(0x14BC7D);
			c_object_move = Memory::GetAddress<t_object_move*>(0x137E6D);
			c_weapon_fire_barrels_eval = Memory::GetAddress<t_weapon_fire_barrels_eval*>(0x160AB7);
			c_object_pre_delete_recursive = Memory::GetAddress<t_object_pre_delete_recursive*>(0x1386E1);
			c_object_delete_recursive = Memory::GetAddress<t_object_delete_recursive*>(0x13683D);
			c_objects_garbage_collection = Memory::GetAddress<t_objects_garbage_collection*>(0x1316A4);

			PatchCall(Memory::GetAddress(0x4A52D), objects_update);
			PatchCall(Memory::GetAddress(0x4A53C), objects_post_update);

		//	if (!Memory::isDedicatedServer())
			//{
			//tags::on_map_load(MapLoad);
		//	}
		}
	}
}
