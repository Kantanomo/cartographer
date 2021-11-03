#include "KantTesting.h"
#include "H2MOD\Tags\TagInterface.h"
#include "Blam/Cache/TagGroups/equipment_definition.hpp"
#include <string>
#include "H2MOD/Modules/Utils/Utils.h"

namespace KantTesting
{
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
				refl::util::for_each(refl::reflect<s_equipment_group_definition>(*equip).members, [&](auto member)
					{
						std::ostringstream stream;
						if constexpr (refl::descriptor::is_readable(member) && refl::descriptor::has_attribute<refl::attr::usage::field>(member))
						{
							//auto a = member(*equip);
							//LOG_INFO_GAME("{}", member.name.str());
							stream << refl::descriptor::get_display_name(member) << " = " << std::to_string(member.get(*equip));
						}
						LOG_INFO_GAME("[{}] {}", __FUNCTION__, stream.str());
					});
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
