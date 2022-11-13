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
#include "Blam/Engine/Networking/NetworkMessageTypeCollection.h"
#include "H2MOD/Tags/TagInterface.h"
#include "H2MOD/Tags/MetaLoader/tag_loader.h"

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
		vsprintf(nDbgMsg, desc, arglist);
		va_end(arglist);

		LOG_INFO_SQ("{}", nDbgMsg);
	}

	void log_error(HSQUIRRELVM v, const SQChar* desc, ...)
	{
		char nDbgMsg[2555];
		std::string dbMsg;
		va_list arglist;
		va_start(arglist, desc);
		vsprintf(nDbgMsg, desc, arglist);
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
	void wprint_from_script(std::wstring message, log_level level)
	{
		switch (level)
		{
		case trace:
			LOG_TRACE_SQ(L"[Message from Script]: {}", message);
			break;
		case debug:
			LOG_DEBUG_SQ(L"[Message from Script]: {}", message);
			break;
		case info:
			LOG_INFO_SQ(L"[Message from Script]: {}", message);
			break;
		case warning:
			LOG_WARNING_SQ(L"[Message from Script]: {}", message);
			break;
		case error:
			LOG_ERROR_SQ(L"[Message from Script]: {}", message);
			break;
		case critical:
			LOG_CRITICAL_SQ(L"[Message from Script]: {}", message);
			break;
		}
	}

	std::string ws2s(std::wstring string)
	{
		using convert_typeX = std::codecvt_utf8<wchar_t>;
		std::wstring_convert<convert_typeX, wchar_t> converterX;

		return converterX.to_bytes(string);
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

	bool script_validate_object_datum(datum object_datum, datum tag_datum)
	{
		//????
		auto object = object_get_fast_unsafe<s_object_data_definition>(object_datum);
		if (object->tag_definition_index == tag_datum)
			return true;
		return false;
	}

	void script_play_custom_sound(wchar_t* name, int sleep)
	{
		h2mod->custom_sound_play(name, sleep);
	}

	void script_disable_sound(datum tag_datum)
	{
		h2mod->disable_sounds(tag_datum);
	}

	void script_set_local_team(int local_player_id, int team_index)
	{
		h2mod->set_local_team_index(local_player_id, team_index);
	}

	byte script_get_local_team()
	{
		return h2mod->get_local_team_index();
	}

	void script_call_update_player_score(datum playerIndex)
	{
		EngineHooks::call_update_player_score(playerIndex);
	}

	std::string script_get_player_xuid_by_id(int player_id)
	{
		player_id = player_id & 0xFFFF;
		if (player_id == -1)
			return std::string("");

		return std::to_string(NetworkSession::GetPlayerId(player_id));
	}

	int script_get_peer_id_by_xuid(std::string xuid_string)
	{
		if (xuid_string.empty())
			return 0;

		XUID xuid = std::stoll(xuid_string);
		return NetworkSession::GetPeerIndexFromId(xuid);
	}

	std::string script_get_name_by_xuid(std::string xuid_string)
	{
		if (xuid_string.empty())
			return std::string("");

		XUID xuid = std::stoll(xuid_string);
		auto player_id = NetworkSession::GetPlayerIndexFromId(xuid);
		return ws2s(NetworkSession::GetPlayerName(player_id));
	}

	int script_get_player_id_by_xuid(std::string xuid_string)
	{
		if (xuid_string.empty())
			return 0;

		XUID xuid = std::stoll(xuid_string);
		return NetworkSession::GetPlayerIndexFromId(xuid);
	}

	int script_get_player_index_from_unit_datum_index(datum unit_index)
	{
		return h2mod->get_player_index_from_unit_datum_index(unit_index);
	}

	std::string script_get_player_name_by_id(int player_id)
	{
		return ws2s(NetworkSession::GetPlayerName(player_id));
	}

	datum script_inject_tag(std::string map_name, std::string tag_path, int type)
	{
		auto tag_datum = tag_loader::Get_tag_datum(tag_path, static_cast<blam_tag::tag_group_type>(type), map_name);
		if (!DATUM_IS_NONE(tag_datum))
		{
			tag_loader::Load_tag(tag_datum, true, map_name);
			tag_loader::Push_Back();
			auto new_datum = tag_loader::ResolveNewDatum(tag_datum);
			return new_datum;
		}
		return -1;
	}

	void bind_functions(HSQUIRRELVM vm)
	{
		using namespace Sqrat;
		
		DefaultVM::Set(vm);
		RootTable().Func("print", &print_from_script);
		RootTable().Func("wprint", &wprint_from_script);
		RootTable().Func("set_gravity", &script_set_gravity);
		RootTable().Func("get_game_life_cycle", &script_get_game_life_cycle);
		RootTable().Func("game_leave_session", &script_leave_session);
		RootTable().Func("get_player_distance_from_player", &script_player_distance_from_player);
		RootTable().Func("set_player_grenades_count", &script_set_player_grenades);
		RootTable().Func("update_player_score", &script_call_update_player_score);
		RootTable().Func("object_validate_type", &script_validate_object_type);
		RootTable().Func("object_create", &script_spawn_object);
		RootTable().Func("object_create_at_player", &script_spawn_object_at_player);
		RootTable().Func("object_validate_datum", &script_validate_object_datum);
		RootTable().Func("get_engine_type", &Engine::get_current_engine_type);
		RootTable().Func("give_player_weapon", &call_give_player_weapon);
		RootTable().Func("play_sound", &script_play_custom_sound);
		RootTable().Func("disable_sound", &script_disable_sound);
		RootTable().Func("send_team_change", &NetworkMessage::SendTeamChange);
		RootTable().Func("set_local_team", &script_set_local_team);
		RootTable().Func("get_local_team", &script_get_local_team);
		RootTable().Func("get_player_index_by_unit_index", &script_get_player_index_from_unit_datum_index);
		RootTable().Func("inject_tag", &script_inject_tag);


		//==========================
		//==Network session calls===
		//==========================

		RootTable().Func("get_player_count", &NetworkSession::GetPlayerCount);
		RootTable().Func("get_player_name_by_index", &script_get_player_name_by_id);
		RootTable().Func("local_peer_is_host", &NetworkSession::LocalPeerIsSessionHost);
		RootTable().Func("get_local_peer_index", &NetworkSession::GetLocalPeerIndex);
		RootTable().Func("get_peer_index_by_id", &NetworkSession::GetPeerIndex);
		RootTable().Func("get_peer_count", &NetworkSession::GetPeerCount);
		RootTable().Func("get_player_id_by_name", &NetworkSession::GetPlayerIdByName);
		RootTable().Func("get_player_xuid_by_id", &script_get_player_xuid_by_id);
		RootTable().Func("get_peer_id_by_xuid", &script_get_peer_id_by_xuid);
		RootTable().Func("get_player_name_by_xuid", &script_get_name_by_xuid);
		RootTable().Func("kick_peer", &NetworkSession::KickPeer);
		RootTable().Func("end_game", &NetworkSession::EndGame);
		RootTable().Func("get_variant_name", &NetworkSession::GetGameVariantName);




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


		//z
		RootTable().Bind("tag_instance",
			Class<tags::tag_instance>(vm, "tag_instance")
			.Var("type", &tags::tag_instance::type)
			.Var("datum_index", &tags::tag_instance::datum_index)
			.Var("data_offset", &tags::tag_instance::data_offset)
			.Var("size", &tags::tag_instance::size)
		);

		RootTable().Bind("angle",
			Class<angle>(vm, "angle")
			.Ctor<float>()
			.Var("rad", &angle::rad)
			.Func("equals", &angle::operator==)
			.Func("as_degree", &angle::as_degree)
			.Func("as_rad", &angle::as_rad)
		);

		RootTable().Bind("real_euler_angles3d",
			Class<real_euler_angles3d>(vm, "real_euler_angles3d")
			.Ctor<angle, angle, angle>()
			.Var("yaw", &real_euler_angles3d::yaw)
			.Var("pitch", &real_euler_angles3d::pitch)
			.Var("roll", &real_euler_angles3d::roll)
		);

		RootTable().Bind("real_vector3d", 
			Class<real_vector3d>(vm, "real_vector3d")
			.Ctor<float, float, float>()
			.Var("x", &real_vector3d::x)
			.Var("y", &real_vector3d::y)
			.Var("z", &real_vector3d::z)
			.Func("equals", &real_vector3d::operator==)
			.Func("get_angle", &real_vector3d::get_angle)
		);


		ConstTable().Enum("log_lvel", Enumeration(vm)
			.Const("trace", (int)log_level::trace)
			.Const("debug", (int)log_level::debug)
			.Const("info", (int)log_level::info)
			.Const("warning", (int)log_level::warning)
			.Const("error", (int)log_level::error)
			.Const("critical", (int)log_level::critical)
		);
		
		ConstTable().Enum("grenade_type", Enumeration(vm)
			.Const("fragmentation", e_grenades::Fragmentation)
			.Const("Plasma", e_grenades::Plasma)
		);

		ConstTable().Enum("game_life_cycle", Enumeration(vm)
			.Const("none", e_game_life_cycle::_life_cycle_none)
			.Const("pre_game`", e_game_life_cycle::_life_cycle_pre_game)
			.Const("start_game", e_game_life_cycle::_life_cycle_start_game)
			.Const("in_game", e_game_life_cycle::_life_cycle_in_game)
			.Const("post_game", e_game_life_cycle::_life_cycle_post_game)
			.Const("joining", e_game_life_cycle::_life_cycle_joining)
			.Const("match_making", e_game_life_cycle::_life_cycle_matchmaking)
		);

		ConstTable().Enum("engine_type", Enumeration(vm)
			.Const("single_player", e_engine_type::_single_player)
			.Const("multiplayer", e_engine_type::_multiplayer)
			.Const("main_menu", e_engine_type::_main_menu)
			.Const("multiplayer_shared", e_engine_type::_mutiplayer_shared)
			.Const("single_player_shared", e_engine_type::_single_player_shared)
		);

		ConstTable().Enum("object_type", Enumeration(vm)
			.Const("biped", e_object_type::biped)
			.Const("vehicle", e_object_type::vehicle)
			.Const("weapon", e_object_type::weapon)
			.Const("equipment", e_object_type::equipment)
			.Const("garbage", e_object_type::garbage)
			.Const("projectile", e_object_type::projectile)
			.Const("scenery", e_object_type::scenery)
			.Const("machine", e_object_type::machine)
			.Const("control", e_object_type::control)
			.Const("light_fixture", e_object_type::light_fixture)
			.Const("sound_scenery", e_object_type::sound_scenery)
			.Const("crate", e_object_type::crate)
			.Const("creature", e_object_type::creature)
		);

		ConstTable().Enum("blam_tag", Enumeration(vm)
			.Const("none", (int)blam_tag::tag_group_type::none)
			.Const("model", (int)blam_tag::tag_group_type::model)
			.Const("rendermodel", (int)blam_tag::tag_group_type::rendermodel)
			.Const("collisionmodel", (int)blam_tag::tag_group_type::collisionmodel)
			.Const("physicsmodel", (int)blam_tag::tag_group_type::physicsmodel)
			.Const("bitmap", (int)blam_tag::tag_group_type::bitmap)
			.Const("colortable", (int)blam_tag::tag_group_type::colortable)
			.Const("multilingualunicodestringlist", (int)blam_tag::tag_group_type::multilingualunicodestringlist)
			.Const("unit", (int)blam_tag::tag_group_type::unit)
			.Const("biped", (int)blam_tag::tag_group_type::biped)
			.Const("vehicle", (int)blam_tag::tag_group_type::vehicle)
			.Const("scenery", (int)blam_tag::tag_group_type::scenery)
			.Const("crate", (int)blam_tag::tag_group_type::crate)
			.Const("creature", (int)blam_tag::tag_group_type::creature)
			.Const("physics", (int)blam_tag::tag_group_type::physics)
			.Const("object", (int)blam_tag::tag_group_type::object)
			.Const("contrail", (int)blam_tag::tag_group_type::contrail)
			.Const("weapon", (int)blam_tag::tag_group_type::weapon)
			.Const("light", (int)blam_tag::tag_group_type::light)
			.Const("effect", (int)blam_tag::tag_group_type::effect)
			.Const("particle", (int)blam_tag::tag_group_type::particle)
			.Const("particlemodel", (int)blam_tag::tag_group_type::particlemodel)
			.Const("particlephysics", (int)blam_tag::tag_group_type::particlephysics)
			.Const("globals", (int)blam_tag::tag_group_type::globals)
			.Const("sound", (int)blam_tag::tag_group_type::sound)
			.Const("soundlooping", (int)blam_tag::tag_group_type::soundlooping)
			.Const("item", (int)blam_tag::tag_group_type::item)
			.Const("equipment", (int)blam_tag::tag_group_type::equipment)
			.Const("antenna", (int)blam_tag::tag_group_type::antenna)
			.Const("lightvolume", (int)blam_tag::tag_group_type::lightvolume)
			.Const("liquid", (int)blam_tag::tag_group_type::liquid)
			.Const("cellularautomata", (int)blam_tag::tag_group_type::cellularautomata)
			.Const("cellularautomata2d", (int)blam_tag::tag_group_type::cellularautomata2d)
			.Const("stereosystem", (int)blam_tag::tag_group_type::stereosystem)
			.Const("cameratrack", (int)blam_tag::tag_group_type::cameratrack)
			.Const("projectile", (int)blam_tag::tag_group_type::projectile)
			.Const("device", (int)blam_tag::tag_group_type::device)
			.Const("devicemachine", (int)blam_tag::tag_group_type::devicemachine)
			.Const("devicecontrol", (int)blam_tag::tag_group_type::devicecontrol)
			.Const("devicelightfixture", (int)blam_tag::tag_group_type::devicelightfixture)
			.Const("pointphysics", (int)blam_tag::tag_group_type::pointphysics)
			.Const("scenariostructurelightmap", (int)blam_tag::tag_group_type::scenariostructurelightmap)
			.Const("scenariostructurebsp", (int)blam_tag::tag_group_type::scenariostructurebsp)
			.Const("scenario", (int)blam_tag::tag_group_type::scenario)
			.Const("shader", (int)blam_tag::tag_group_type::shader)
			.Const("shadertemplate", (int)blam_tag::tag_group_type::shadertemplate)
			.Const("shaderlightresponse", (int)blam_tag::tag_group_type::shaderlightresponse)
			.Const("shaderpass", (int)blam_tag::tag_group_type::shaderpass)
			.Const("vertexshader", (int)blam_tag::tag_group_type::vertexshader)
			.Const("pixelshader", (int)blam_tag::tag_group_type::pixelshader)
			.Const("decoratorset", (int)blam_tag::tag_group_type::decoratorset)
			.Const("decorators", (int)blam_tag::tag_group_type::decorators)
			.Const("sky", (int)blam_tag::tag_group_type::sky)
			.Const("wind", (int)blam_tag::tag_group_type::wind)
			.Const("soundenvironment", (int)blam_tag::tag_group_type::soundenvironment)
			.Const("lensflare", (int)blam_tag::tag_group_type::lensflare)
			.Const("planarfog", (int)blam_tag::tag_group_type::planarfog)
			.Const("patchyfog", (int)blam_tag::tag_group_type::patchyfog)
			.Const("meter", (int)blam_tag::tag_group_type::meter)
			.Const("decal", (int)blam_tag::tag_group_type::decal)
			.Const("colony", (int)blam_tag::tag_group_type::colony)
			.Const("damageeffect", (int)blam_tag::tag_group_type::damageeffect)
			.Const("dialogue", (int)blam_tag::tag_group_type::dialogue)
			.Const("itemcollection", (int)blam_tag::tag_group_type::itemcollection)
			.Const("vehiclecollection", (int)blam_tag::tag_group_type::vehiclecollection)
			.Const("weaponhudinterface", (int)blam_tag::tag_group_type::weaponhudinterface)
			.Const("grenadehudinterface", (int)blam_tag::tag_group_type::grenadehudinterface)
			.Const("unithudinterface", (int)blam_tag::tag_group_type::unithudinterface)
			.Const("newhuddefinition", (int)blam_tag::tag_group_type::newhuddefinition)
			.Const("hudnumber", (int)blam_tag::tag_group_type::hudnumber)
			.Const("hudglobals", (int)blam_tag::tag_group_type::hudglobals)
			.Const("multiplayerscenariodescription", (int)blam_tag::tag_group_type::multiplayerscenariodescription)
			.Const("detailobjectcollection", (int)blam_tag::tag_group_type::detailobjectcollection)
			.Const("soundscenery", (int)blam_tag::tag_group_type::soundscenery)
			.Const("hudmessagetext", (int)blam_tag::tag_group_type::hudmessagetext)
			.Const("userinterfacescreenwidgetdefinition", (int)blam_tag::tag_group_type::userinterfacescreenwidgetdefinition)
			.Const("userinterfacelistskindefinition", (int)blam_tag::tag_group_type::userinterfacelistskindefinition)
			.Const("userinterfaceglobalsdefinition", (int)blam_tag::tag_group_type::userinterfaceglobalsdefinition)
			.Const("userinterfacesharedglobalsdefinition", (int)blam_tag::tag_group_type::userinterfacesharedglobalsdefinition)
			.Const("textvaluepairdefinition", (int)blam_tag::tag_group_type::textvaluepairdefinition)
			.Const("multiplayervariantsettingsinterfacedefinition", (int)blam_tag::tag_group_type::multiplayervariantsettingsinterfacedefinition)
			.Const("materialeffects", (int)blam_tag::tag_group_type::materialeffects)
			.Const("garbage", (int)blam_tag::tag_group_type::garbage)
			.Const("style", (int)blam_tag::tag_group_type::style)
			.Const("character", (int)blam_tag::tag_group_type::character)
			.Const("aidialogueglobals", (int)blam_tag::tag_group_type::aidialogueglobals)
			.Const("aimissiondialogue", (int)blam_tag::tag_group_type::aimissiondialogue)
			.Const("scenariosceneryresource", (int)blam_tag::tag_group_type::scenariosceneryresource)
			.Const("scenariobipedsresource", (int)blam_tag::tag_group_type::scenariobipedsresource)
			.Const("scenariovehiclesresource", (int)blam_tag::tag_group_type::scenariovehiclesresource)
			.Const("scenarioequipmentresource", (int)blam_tag::tag_group_type::scenarioequipmentresource)
			.Const("scenarioweaponsresource", (int)blam_tag::tag_group_type::scenarioweaponsresource)
			.Const("scenariosoundsceneryresource", (int)blam_tag::tag_group_type::scenariosoundsceneryresource)
			.Const("scenariolightsresource", (int)blam_tag::tag_group_type::scenariolightsresource)
			.Const("scenariodevicesresource", (int)blam_tag::tag_group_type::scenariodevicesresource)
			.Const("scenariodecalsresource", (int)blam_tag::tag_group_type::scenariodecalsresource)
			.Const("scenariocinematicsresource", (int)blam_tag::tag_group_type::scenariocinematicsresource)
			.Const("scenariotriggervolumesresource", (int)blam_tag::tag_group_type::scenariotriggervolumesresource)
			.Const("scenarioclusterdataresource", (int)blam_tag::tag_group_type::scenarioclusterdataresource)
			.Const("scenariocreatureresource", (int)blam_tag::tag_group_type::scenariocreatureresource)
			.Const("scenariodecoratorsresource", (int)blam_tag::tag_group_type::scenariodecoratorsresource)
			.Const("scenariostructurelightingresource", (int)blam_tag::tag_group_type::scenariostructurelightingresource)
			.Const("scenariohssourcefile", (int)blam_tag::tag_group_type::scenariohssourcefile)
			.Const("scenarioairesource", (int)blam_tag::tag_group_type::scenarioairesource)
			.Const("scenariocommentsresource", (int)blam_tag::tag_group_type::scenariocommentsresource)
			.Const("breakablesurface", (int)blam_tag::tag_group_type::breakablesurface)
			.Const("materialphysics", (int)blam_tag::tag_group_type::materialphysics)
			.Const("soundclasses", (int)blam_tag::tag_group_type::soundclasses)
			.Const("multiplayerglobals", (int)blam_tag::tag_group_type::multiplayerglobals)
			.Const("soundeffecttemplate", (int)blam_tag::tag_group_type::soundeffecttemplate)
			.Const("soundeffectcollection", (int)blam_tag::tag_group_type::soundeffectcollection)
			.Const("chocolatemountain", (int)blam_tag::tag_group_type::chocolatemountain)
			.Const("modelanimationgraph", (int)blam_tag::tag_group_type::modelanimationgraph)
			.Const("cloth", (int)blam_tag::tag_group_type::cloth)
			.Const("screeneffect", (int)blam_tag::tag_group_type::screeneffect)
			.Const("weathersystem", (int)blam_tag::tag_group_type::weathersystem)
			.Const("soundmix", (int)blam_tag::tag_group_type::soundmix)
			.Const("sounddialogueconstants", (int)blam_tag::tag_group_type::sounddialogueconstants)
			.Const("soundcachefilegestalt", (int)blam_tag::tag_group_type::soundcachefilegestalt)
			.Const("cachefilesound", (int)blam_tag::tag_group_type::cachefilesound)
			.Const("mousecursordefinition", (int)blam_tag::tag_group_type::mousecursordefinition)
			.Const("uldg", (int)blam_tag::tag_group_type::udlg)
		);
	}
}
