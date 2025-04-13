#pragma once
#include "CommandHandler.h"

namespace CommandCollection
{
	extern std::vector<ConsoleCommand*> commandTable;
	
	void InitializeCommands();
	void InsertCommand(ConsoleCommand* newCommand);
}
