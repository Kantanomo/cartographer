#include "stdafx.h"
#include "SquirrelGloals.h"
#include "H2MOD/Modules/Shell/Startup/Startup.h"

#define SCRAT_USE_EXCEPTIONS
namespace SquirrelEngineGlobals
{
	bool script_loaded = false;
	bool script_id = 0;
	std::wstring script_path = L"a";
	bool script_downloaded = false;
	bool first_run = true;
	bool enable_squirrel_debug = false;
	void sqAddDebugText(std::string Msg)
	{
		LOG_DEBUG_SQ("{}", Msg);
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
		if(enable_squirrel_debug)
			LOG_TRACE_SQ("[Squirrel] Debug type {}, source {}, line {}, function {}", type, sourcename, line, funcname);
	}

	void Initialize()
	{
		squirrel_log = h2log::create("squirrel", prepareLogFileName(L"squirrel"), true, 0);
	}

}
