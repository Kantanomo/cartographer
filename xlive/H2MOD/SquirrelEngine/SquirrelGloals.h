#pragma once
#include <sqrat/include/sqrat.h>

#include "squirrel/include/squirrel.h"

namespace SquirrelEngineGlobals
{
	extern bool script_loaded;
	extern bool script_id;
	extern std::wstring script_path;
	extern bool script_downloaded;
	extern bool first_run;
	extern bool enable_squirrel_debug;
	void sqAddDebugText(std::string Msg);
	
	void log_info(HSQUIRRELVM v, const SQChar* desc, ...);
	void log_error(HSQUIRRELVM v, const SQChar* desc, ...);
	void log_compile_error(HSQUIRRELVM v, const SQChar* desc, const SQChar* source, SQInteger line, SQInteger column);
	void log_debug_hook(HSQUIRRELVM vm, SQInteger type, const SQChar* sourcename, SQInteger line, const SQChar* funcname);

	void Initialize();

}