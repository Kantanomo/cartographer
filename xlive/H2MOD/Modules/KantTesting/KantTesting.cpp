#include "KantTesting.h"
#include "H2MOD\Tags\TagInterface.h"
#include "Blam/Cache/TagGroups/equipment_definition.hpp"
#include "H2MOD/Modules/Utils/Utils.h"
#include <string>
namespace KantTesting
{
	template<typename T>
	void reflect(T item)
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
							LOG_INFO_GAME("[{}] {}", __FUNCTION__, stream.str());
						}
						if constexpr (refl::descriptor::has_attribute<tag_refl::refl_tag_block>(member))
						{
							LOG_ERROR_GAME("[{}] START {}", __FUNCTION__, refl::descriptor::get_display_name(member));
							auto a = member.get(item);
							constexpr auto type = refl::reflect(a);
							//
							// This works???
							//
							refl::util::for_each(type.members, [&](auto member_)
								{
									if constexpr (refl::descriptor::has_attribute<refl::attr::usage::function>(member_) && refl::descriptor::is_function(member_))
									{
										int data_size = refl::runtime::invoke<int>(a, "data_size");
										int type_size = refl::runtime::invoke<int>(a, "type_size");
										LOG_INFO_GAME("[{}] {:x} - {:x}", __FUNCTION__, data_size, type_size);
									}
								});
							//
							//This doesn't?
							//
							//int data_size = refl::runtime::invoke<int>(a, "data_size");
							//int type_size = refl::runtime::invoke<int>(a, "type_size");
							//LOG_INFO_GAME("[{}] {:x} - {:x}", __FUNCTION__, data_size, type_size);


							LOG_ERROR_GAME("[{}] END {}", __FUNCTION__, refl::descriptor::get_display_name(member));
						}
						else if constexpr (refl::descriptor::has_attribute<refl::attr::usage::member>(member))
						{
							LOG_INFO_GAME("[{}] START {}", __FUNCTION__, refl::descriptor::get_display_name(member));
							reflect(member(item));
							LOG_INFO_GAME("[{}] END {}", __FUNCTION__, refl::descriptor::get_display_name(member));
						}
					}
				});
		}
	}
	void mapLoad()
	{
		//if(h2mod->GetEngineType() == Multiplayer)
		//{
		auto equip_datums = tags::find_tags(blam_tag::tag_group_type::equipment);
		std::vector<s_equipment_group_definition*> equips;
		for (auto equip_datum : equip_datums)
		{
			auto equip = tags::get_tag<blam_tag::tag_group_type::equipment, s_equipment_group_definition>(equip_datum.first);
			if (equip)
			{
				reflect(*equip);
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
