#pragma once
#include <sqrat/include/sqrat.h>

#include "Blam/Cache/DataTypes/BlamPrimitiveType.h"
#include "Blam/Engine/Game/GameOptions.h"
#include "squirrel/include/squirrel.h"



namespace SquirrelEngineGlobals
{
	extern bool script_loaded;
	extern bool script_id;
	extern std::wstring script_path;
	extern bool script_downloaded;
	void sqAddDebugText(std::string Msg);
	static SQInteger _sqstd_aux_printerror(HSQUIRRELVM v);
	void log_info(HSQUIRRELVM v, const SQChar* desc, ...);
	void log_error(HSQUIRRELVM v, const SQChar* desc, ...);
	void log_compile_error(HSQUIRRELVM v, const SQChar* desc, const SQChar* source, SQInteger line, SQInteger column);
	void log_debug_hook(HSQUIRRELVM vm, SQInteger type, const SQChar* sourcename, SQInteger line, const SQChar* funcname);

	void Initialize();

}

namespace SquirrelEngine
{
	void script_start(std::string path);
	void start_squirrel_vm();
	void end_squirrel_vm();

	void bind_functions(HSQUIRRELVM vm);
	void on_map_load(e_engine_type type);
	bool on_player_team_change(int player_index, int team_index);
	bool on_auto_pickup_handler(int unit_datum, int object_datum);
	bool on_update_score(datum playerDatum, int killType);

	void Initialize();

	template<class F>
	void BindSquirrelFunction(HSQUIRRELVM vm, std::string function_name, F function)
	{

	}
}