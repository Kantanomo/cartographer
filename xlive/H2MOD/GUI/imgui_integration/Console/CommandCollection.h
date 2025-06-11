#pragma once

#ifdef TERMINAL_ENABLED

#include "CommandHandler.h"

namespace CommandCollection
{
	extern std::vector<ConsoleCommand*> commandTable;
	
	void InitializeCommands();
	void InsertCommand(ConsoleCommand* newCommand);
}

#endif
