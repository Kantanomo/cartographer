#include "stdafx.h"
#include "KantTesting.h"

#include "game/game.h"
#include "Networking/logic/life_cycle_manager.h"
#include "H2MOD/Modules/Input/KeyboardInput.h"
#include "H2MOD/Modules/OnScreenDebug/OnscreenDebug.h"
#include "tag_files/files_windows.h"
#include "tag_files/tag_loader/tag_injection_manager.h"


namespace KantTesting
{
	void MapLoad()
	{
	}

	void Initialize()
	{
		tag_group type;
		type.group = _tag_group_biped;

		c_tag_injecting_manager manager;

		manager.set_active_map("carto_shared");
		manager.load_tag(_tag_group_biped, "objects\\characters\\masterchief_skeleton\\masterchief_skeleton", true);


		auto ab = 1233123;
	}
}
