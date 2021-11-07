#include "KantTesting.h"
#include "H2MOD\Tags\TagInterface.h"
#include "Blam/Cache/TagGroups/equipment_definition.hpp"
#include "H2MOD/Modules/Utils/Utils.h"
#include <string>

#include "Blam/Cache/TagGroups/biped_definition.hpp"

namespace KantTesting
{
	template<typename T>
	__declspec(noinline) void reflect_tag_block(T item)
	{
		if constexpr (refl::is_reflectable(item)) {
			constexpr auto type = refl::reflect(item);
			//
			// This works???
			//
			if (item.size > 0) {
				int data_size = refl::runtime::invoke<int>(item, "data_size");
				int type_size = refl::runtime::invoke<int>(item, "type_size");
				constexpr auto func = refl::util::find_one(type.members, [](auto m) { return m.name == "begin"; }); // -> function_descriptor<Circle, 0>{...}
				LOG_INFO_GAME("[{}] {:x} - {:x}", __FUNCTION__, data_size, type_size);
				//refl::util::for_each(type.members, [&](auto member_)
				//	{
				//		if constexpr (refl::descriptor::has_attribute<refl::attr::usage::function>(member_) && refl::descriptor::is_function(member_))
				//		{
				//			//int data_size = refl::runtime::invoke<int>(a, "data_size");
				//			//int type_size = refl::runtime::invoke<int>(a, "type_size");
				//			////auto test = refl::runtime::invoke<void*>(a, "operator[]");
				//			////auto b = member_()
				//			//LOG_INFO_GAME("[{}] {:x} - {:x}", __FUNCTION__, data_size, type_size);
				//			if (member_.name == "begin")
				//			{
				//				auto c = member_(item);
				//				LOG_INFO_GAME("[{}] {:x}", __FUNCTION__, (unsigned long)std::addressof(*c));
				//			}
				//			LOG_INFO_GAME("[{}] {}", __FUNCTION__, refl::descriptor::get_display_name(member_));
				//		}
				//	});
			}
			//
			//This doesn't?
			//
			//int data_size = refl::runtime::invoke<int>(a, "data_size");
			//int type_size = refl::runtime::invoke<int>(a, "type_size");


			//LOG_INFO_GAME("[{}] {:x} - {:x}", __FUNCTION__, data_size, type_size);
		}
	}
	template<typename T>
	__declspec(noinline) void reflect(T item)
	{
		if (refl::is_reflectable(item))
		{
			refl::util::for_each(refl::reflect<T>(item).members, [&](auto member)
				{
					if constexpr (refl::descriptor::is_readable(member))
					{
						if constexpr (refl::descriptor::has_attribute<tag_refl::property>(member))
						{
							std::ostringstream stream;
							stream << refl::descriptor::get_display_name(member) << " = " << std::to_string(member.get(item));
							LOG_INFO_GAME("{}", stream.str());
						}
						if constexpr (refl::descriptor::has_attribute<tag_refl::refl_tag_block>(member))
						{
							auto block = member(item);
							if constexpr (refl::is_reflectable(block))
							{
								if(block.size > 0 && block.data > 0)
								{
									LOG_ERROR_GAME("START {}", refl::descriptor::get_display_name(member));
									for(auto i = 0; i < block.size; i++)
										reflect(*block[i]);
									LOG_ERROR_GAME("END {}", refl::descriptor::get_display_name(member));
								}
							}
						}
						else if constexpr (refl::descriptor::has_attribute<refl::attr::usage::member>(member))
						{
							LOG_INFO_GAME("START {}", refl::descriptor::get_display_name(member));
							reflect(member(item));
							LOG_INFO_GAME("END {}", refl::descriptor::get_display_name(member));
						}
					}
				});
		}
	}
	void mapLoad()
	{
		//if(h2mod->GetEngineType() == Multiplayer)
		//{
		auto equip_datums = tags::find_tags(blam_tag::tag_group_type::biped);
		std::vector<s_biped_group_definition*> equips;
		for (auto equip_datum : equip_datums)
		{
			auto equip = tags::get_tag<blam_tag::tag_group_type::biped, s_biped_group_definition>(equip_datum.first);
			if (equip)
			{
				LOG_INFO_GAME("REFLECT {}", equip_datum.second);
				reflect(*equip);
				LOG_INFO_GAME("END REFLECT {}", equip_datum.second);
				//reflect_tag_block(equip->itemTag.predicted_bitmaps);
				//refl::util::for_each(refl::reflect<s_equipment_group_definition>(*equip).members, [&](auto member)
				//	{
				//		std::ostringstream stream;
				//		if constexpr (refl::descriptor::is_readable(member) && refl::descriptor::has_attribute<refl::attr::usage::field>(member))
				//		{
				//			//auto a = member(*equip);
				//			//LOG_INFO_GAME("{}", member.name.str());
				//			stream << refl::descriptor::get_display_name(member) << " = " << std::to_string(member.get(*equip));
				//		}
				//		LOG_INFO_GAME("[{}] {}", __FUNCTION__, stream.str());
				//	});
			}
		}
		//}
	}

	void Initialize()
	{
		if (ENABLEKANTTEST) {
			if (!Memory::isDedicatedServer()) {
				tags::on_map_load(mapLoad);
			}
		}
	}
}
