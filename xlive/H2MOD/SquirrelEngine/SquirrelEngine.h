#pragma once
#include <sqrat/include/sqrat.h>

#include "Blam/Cache/DataTypes/BlamPrimitiveType.h"
#include "Blam/Engine/Game/GameOptions.h"
#include "squirrel/include/squirrel.h"


namespace SquirrelEngine
{
	void script_start(std::string path);
	void start_squirrel_vm();
	void end_squirrel_vm();
	static SQInteger _sqstd_aux_printerror(HSQUIRRELVM v);
	void bind_functions(HSQUIRRELVM vm);
	void on_map_load(e_engine_type type);
	bool on_player_team_change(int player_index, int team_index);
	bool on_auto_pickup_handler(int unit_datum, int object_datum);
	bool on_update_score(datum playerDatum, int killType);

	void Initialize();
}