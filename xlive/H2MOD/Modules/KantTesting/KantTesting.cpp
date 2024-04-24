#include "stdafx.h"
#include "KantTesting.h"

#include "game/game.h"
#include "Networking/logic/life_cycle_manager.h"
#include "H2MOD/Modules/Input/KeyboardInput.h"
#include "H2MOD/Modules/OnScreenDebug/OnscreenDebug.h"
#include "H2MOD/Tags/MetaLoader/xml_loader.h"
#include "tag_files/files_windows.h"


namespace KantTesting
{
	void MapLoad()
	{
	}

	void Initialize()
	{
		tag_group type;
		type.group = _tag_group_biped;
		c_xml_definition_agent lol(type, "C:\\Halo2\\mods\\plugins\\bipd.xml");
		//c_xml_definition_agent lol(type, "C:\\Halo2\\mods\\plugins\\sky.xml");
		FILE* a = fopen("C:\\Halo2\\mods\\maps\\carto_shared.map", "r");
		c_xml_definition_loader heh(lol.get_definition(), a, 0xE29A00BF);
		//c_xml_definition_loader heh(lol.get_definition(), a, 0xE19B001Du);
		auto ab = 1233123;
	}
}
