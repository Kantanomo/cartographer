#include "stdafx.h"
#include "SquirrelEngine.h"
#include "spdlog/fmt/bundled/printf.h"
#include <sqrat/include/sqrat.h>
#include <sqrat/include/sqratimport.h>

#include "Blam/Engine/Game/PhysicsConstants.h"
#include "Blam/Engine/Networking/Session/NetworkSession.h"
#include "H2MOD/Engine/Engine.h"
#include "H2MOD/EngineHooks/EngineHooks.h"
#include "H2MOD/Modules/EventHandler/EventHandler.hpp"
#include "H2MOD/Modules/HaloScript/HaloScript.h"
#include "H2MOD/Modules/Shell/Config.h"
#include "H2MOD/Modules/Shell/Shell.h"
#include "H2MOD/Modules/Shell/Startup/Startup.h"
#include "Blam/Cache/DataTypes/BlamDataTypes.h"

#define SCRAT_USE_EXCEPTIONS
namespace SquirrelEngineGlobals
{
	bool script_loaded = false;
	bool script_id = 0;
	std::wstring script_path = L"a";
	bool script_downloaded = false;
	bool first_run = true;
	void sqAddDebugText(std::string Msg)
	{
		LOG_DEBUG_SQ("{}", Msg);
	}

	static SQInteger _sqstd_aux_printerror(HSQUIRRELVM v)
	{
		SQPRINTFUNCTION pf = sq_geterrorfunc(v);
		if (pf) {
			const SQChar* sErr = 0;
			if (sq_gettop(v) >= 1) {
				if (SQ_SUCCEEDED(sq_getstring(v, 2, &sErr))) {
					pf(v, _SC("\nAN ERROR HAS OCCURRED [%s]\n"), sErr);
				}
				else {
					pf(v, _SC("\nAN ERROR HAS OCCURRED [unknown]\n"));
				}
				sqstd_printcallstack(v);
			}
		}
		return 0;
	}
	void log_info(HSQUIRRELVM v, const SQChar* desc, ...)
	{
		char nDbgMsg[2555];
		std::string dbMsg;
		va_list arglist;
		va_start(arglist, desc);
		//fmt::vsprintf(nDbgMsg, desc, arglist);
		va_end(arglist);

		LOG_INFO_SQ("{}", nDbgMsg);
	}

	void log_error(HSQUIRRELVM v, const SQChar* desc, ...)
	{
		char nDbgMsg[2555];
		std::string dbMsg;
		va_list arglist;
		va_start(arglist, desc);
		//fmt::vsprintf(nDbgMsg, desc, arglist);
		va_end(arglist);

		LOG_CRITICAL_SQ("{}", nDbgMsg);

	}

	void log_compile_error(HSQUIRRELVM v, const SQChar* desc, const SQChar* source, SQInteger line, SQInteger column)
	{
		LOG_CRITICAL_SQ("[Squirrel] Compiler error - line {}, column {}, desc {}, souce {}", line, column, desc, source);
	}

	void log_debug_hook(HSQUIRRELVM vm, SQInteger type, const SQChar* sourcename, SQInteger line, const SQChar* funcname)
	{
		//LOG_TRACE_SQ("[Squirrel] Debug type {}, source {}, line {}, function {}", type, sourcename, line, funcname);
	}

	void Initialize()
	{
		squirrel_log = h2log::create("squirrel", prepareLogFileName(L"squirrel"), true, 0);
	}

}

namespace SquirrelEngine
{

	void start_squirrel_vm()
	{
		if (SquirrelEngineGlobals::first_run == true)
		{
			HSQUIRRELVM vm = sq_open(1000);
			bind_functions(vm);
			SquirrelEngineGlobals::first_run = false;
		}

		Sqrat::Script GlobalScript;

		sq_enabledebuginfo(Sqrat::DefaultVM::Get(), true);
		sq_notifyallexceptions(Sqrat::DefaultVM::Get(), true);
		sq_setcompilererrorhandler(Sqrat::DefaultVM::Get(), SquirrelEngineGlobals::log_compile_error);
		sq_setnativedebughook(Sqrat::DefaultVM::Get(), SquirrelEngineGlobals::log_debug_hook);
		sqstd_seterrorhandlers(Sqrat::DefaultVM::Get());
		sqstd_printcallstack(Sqrat::DefaultVM::Get());

		GlobalScript.CompileFile(_SC("Scripts/Globals.nut"));

		GlobalScript.Run();

		LOG_INFO_SQ("Starting VM");
	}
	void end_squirrel_vm()
	{
		LOG_INFO_SQ("Killing VM");

		sq_close(Sqrat::DefaultVM::Get());
		HSQUIRRELVM vm = sq_open(1000);
		bind_functions(vm);

		SquirrelEngineGlobals::script_loaded = false;
		SquirrelEngineGlobals::script_downloaded = true;
	}

	void script_start(std::string path)
	{
		/*LOG_TRACE_SQ("Loading defaults");*/

		end_squirrel_vm();
		start_squirrel_vm();
		Sqrat::Script varScript;
		sq_setprintfunc(Sqrat::DefaultVM::Get(), SquirrelEngineGlobals::log_info, SquirrelEngineGlobals::log_error);
		sq_enabledebuginfo(Sqrat::DefaultVM::Get(), true);
		sq_notifyallexceptions(Sqrat::DefaultVM::Get(), true);
		sq_setcompilererrorhandler(Sqrat::DefaultVM::Get(), SquirrelEngineGlobals::log_compile_error);
		sq_setnativedebughook(Sqrat::DefaultVM::Get(), SquirrelEngineGlobals::log_debug_hook);
		sq_newclosure(Sqrat::DefaultVM::Get(), SquirrelEngineGlobals::_sqstd_aux_printerror, 0);
		sq_seterrorhandler(Sqrat::DefaultVM::Get());
		sqstd_seterrorhandlers(Sqrat::DefaultVM::Get());

		Sqrat::string errMsg;
		//varScript.CompileFile(_SC(path.c_str()));
		if (varScript.CompileFile(_SC(path.c_str()), errMsg))
		{
			varScript.Run();
			SquirrelEngineGlobals::script_loaded = true;
			SquirrelEngineGlobals::script_id += 1;
			SquirrelEngineGlobals::script_path = L"";
		}
		else
		{
			SquirrelEngineGlobals::sqAddDebugText(errMsg);
		}
	}

	void on_map_load(e_engine_type type)
	{
		Sqrat::Function sqMapLoadFunc = Sqrat::RootTable().GetFunction(_SC("OnMapLoad"));
		if (!sqMapLoadFunc.IsNull())
			sqMapLoadFunc.Execute();
		/*else
			LOG_WARNING_SQ("[Squirrel] - No OnMapLoad function was found!");*/
	}

	void on_pre_player_spawn(datum unit_datum)
	{
		Sqrat::Function sqFunc = Sqrat::RootTable().GetFunction("OnPrePlayerSpawn");
		if (!sqFunc.IsNull())
		{
			try
			{
				sqFunc.Execute(unit_datum);
			}
			catch (Sqrat::Exception e)
			{
				SquirrelEngineGlobals::sqAddDebugText(e.Message());
			}
		}
		/*else {
			LOG_WARNING_FUNC("[Squirrel] - No OnPrePlayerSpawn function was found!");
		}*/
	}

	void on_player_spawn(datum unit_datum)
	{
		Sqrat::ErrorHandling::Enable(true);
		Sqrat::Function sqFunc = Sqrat::RootTable().GetFunction("OnPlayerSpawn");
		if (!sqFunc.IsNull())
		{
			try
			{
				sqFunc.Execute(unit_datum);
			}
			catch (Sqrat::Exception e)
			{
				SquirrelEngineGlobals::sqAddDebugText(e.Message());
			}
		}
		/*else {
			LOG_WARNING_FUNC("[Squirrel] - No OnPrePlayerSpawn function was found!");
		}*/
	}

	bool on_player_team_change(int player_index, int team_index)
	{
		Sqrat::Function sqFunc = Sqrat::RootTable().GetFunction("OnChangeTeam");
		if (!sqFunc.IsNull())
			return *sqFunc.Evaluate<bool>(player_index, team_index).Get();
		else
		{
			//LOG_WARNING_FUNC("[Squirrel] - No OnChangeTeam function was found!");
			return true;
		}
	}

	bool on_auto_pickup_handler(int unit_datum, int object_datum)
	{
		Sqrat::Function sqFunc = Sqrat::RootTable().GetFunction("OnAutoPickup");
		if (!sqFunc.IsNull())
			return *sqFunc.Evaluate<bool>(unit_datum, object_datum).Get();
		else
		{
			return true;
		}
	}

	void on_player_death(datum unit_datum, datum damaging_datum)
	{
		Sqrat::Function sqFunc = Sqrat::RootTable().GetFunction("OnPlayerDeath");
		if (!sqFunc.IsNull())
			sqFunc.Execute(unit_datum, damaging_datum);
		/*else
			LOG_WARNING_FUNC("[Squirrel] - No OnPlayerDeath function was found!");*/
	}

	bool on_update_score(datum playerDatum, int killType)
	{
		Sqrat::Function sqFunc = Sqrat::RootTable().GetFunction("OnPlayerScoreUpdate");
		if (!sqFunc.IsNull())
			return *sqFunc.Evaluate<bool>(playerDatum, killType);
		else
		{
			//LOG_WARNING_FUNC("[Squirrel] - No OnPlayerScoreUpdate function was found!");
			return true;
		}
	}
	void on_before_game_tick()
	{
		Sqrat::Function func = Sqrat::RootTable().GetFunction("OnBeforeGameTick");
		if (!func.IsNull())
			func.Execute();
		/*else
			LOG_WARNING_FUNC("[Squirrel] - No OnBeforeGameTick function was found!");*/
	}
	void on_after_game_tick()
	{
		Sqrat::Function func = Sqrat::RootTable().GetFunction("OnAfterGameTick");
		if (!func.IsNull())
			func.Execute();
		/*else
			LOG_WARNING_FUNC("[Squirrel] - No OnAfterGameTick function was found!");*/
	}
	void Initialize()
	{
		EventHandler::register_callback(on_map_load, EventType::map_load, EventExecutionType::execute_before);
		EventHandler::register_callback(on_pre_player_spawn, EventType::player_spawn, EventExecutionType::execute_before);
		EventHandler::register_callback(on_player_spawn, EventType::player_spawn, EventExecutionType::execute_after);
		EventHandler::register_callback(on_player_death, EventType::player_death, EventExecutionType::execute_after);
		EventHandler::register_callback(on_before_game_tick, EventType::game_loop, EventExecutionType::execute_before);
		EventHandler::register_callback(on_after_game_tick, EventType::game_loop, EventExecutionType::execute_after);
		start_squirrel_vm();
	}

	void print_from_script(std::string message, log_level level)
	{
		switch (level)
		{
		case trace:
			LOG_TRACE_SQ("[Message from Script]: {}", message);
			break;
		case debug:
			LOG_DEBUG_SQ("[Message from Script]: {}", message);
			break;
		case info:
			LOG_INFO_SQ("[Message from Script]: {}", message);
			break;
		case warning:
			LOG_WARNING_SQ("[Message from Script]: {}", message);
			break;
		case error:
			LOG_ERROR_SQ("[Message from Script]: {}", message);
			break;
		case critical:
			LOG_CRITICAL_SQ("[Message from Script]: {}", message);
			break;
		}
	}

	void script_set_gravity(float value)
	{
		physics_constants::get()->gravity = value * physics_constants::get_default_gravity();
	}

	e_game_life_cycle script_get_game_life_cycle()
	{
		return Engine::get_game_life_cycle();
	}

	void script_leave_session()
	{
		NetworkSession::LeaveSession();
	}

	float script_player_distance_from_player(int player_1, int player_2)
	{
		if (player_1 == -1 || player_2 == -1)
			return 0;

		return h2mod->get_distance(player_1, player_2);
	}

	void script_set_player_grenades(int playerIndex, e_grenades type, byte count, bool resetEquipment)
	{
		if (playerIndex == -1)
			return;
		h2mod->set_player_unit_grenades_count(playerIndex, type, count, resetEquipment);
	}

	bool script_get_gamepad_input_pressed(ControllerInput::XINPUT_BUTTONS button)
	{
		if (_Shell::IsGameMinimized())
			return false;

		return ControllerInput::check_gamepad_input_state(button);
	}

	bool script_check_key_state(int keycode)
	{
		if (_Shell::IsGameMinimized())
			return false;

		return (GetKeyState(keycode) & 0x8000);
	}

	bool script_validate_object_type(datum object_datum, e_object_type type)
	{
		if (object_datum == -1)
			return false;

		if (get_objects_header(object_datum)->type == type)
			return true;

		return false;
	}

	void script_spawn_object(datum object_datum, float x, float y, float z, float i, float j, float k)
	{
		s_object_placement_data nObject;
		Engine::Objects::create_new_placement_data(&nObject, object_datum, -1, 0);
		nObject.position.x = x;
		nObject.position.y = y;
		nObject.position.z = z;
		nObject.translational_velocity.i = i;
		nObject.translational_velocity.j = j;
		nObject.translational_velocity.k = k;
		datum new_datum = Engine::Objects::object_new(&nObject);
		if (!DATUM_IS_NONE(new_datum))
			Engine::Objects::simulation_action_object_create(new_datum);
	}

	void script_spawn_object_at_player(datum object_datum, datum player_datum, float x, float y, float z, float i, float j, float k)
	{
		auto biped_object = object_get_fast_unsafe<s_biped_data_definition>(player_datum);
		s_object_placement_data nObject;
		Engine::Objects::create_new_placement_data(&nObject, object_datum, -1, 0);
		nObject.position = biped_object->position;
		nObject.translational_velocity = biped_object->translational_velocity;
		datum new_datum = Engine::Objects::object_new(&nObject);
		if (!DATUM_IS_NONE(new_datum))
			Engine::Objects::simulation_action_object_create(new_datum);
	}

	void bind_functions(HSQUIRRELVM vm)
	{
		using namespace Sqrat;
		
		DefaultVM::Set(vm);
		RootTable().Func("print", &print_from_script);
		RootTable().Func("set_gravity", &script_set_gravity);
		RootTable().Func("get_game_life_cycle", &script_get_game_life_cycle);
		RootTable().Func("game_leave_session", &script_leave_session);
		RootTable().Func("get_player_distance_from_player", &script_player_distance_from_player);
		RootTable().Func("set_player_grenades_count", &script_set_player_grenades);
		RootTable().Func("update_player_score", &EngineHooks::call_update_player_score);
		RootTable().Func("object_validate_type", &script_validate_object_type);
		RootTable().Func("object_create", &script_spawn_object);
		RootTable().Func("object_create_at_player", &script_spawn_object_at_player);


		//======================
		//==Halo Script Calls===
		//======================

		RootTable().Func("hs_object_destroy", &HaloScript::ObjectDestroy);
		RootTable().Func("hs_unit_kill", &HaloScript::UnitKill);
		RootTable().Func("hs_unit_in_vehicle", &HaloScript::UnitInVehicle);
		RootTable().Func("hs_unit_get_health", &HaloScript::UnitGetHealth);
		RootTable().Func("hs_unit_get_shield", &HaloScript::UnitGetShield);
		

		RootTable().Func("get_gamepad_input_pressed", &script_get_gamepad_input_pressed);
		RootTable().Func("check_key_state", &script_check_key_state);
	}
}
