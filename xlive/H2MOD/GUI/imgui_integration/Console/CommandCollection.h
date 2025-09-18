#pragma once

#ifdef TERMINAL_ENABLED

class ConsoleCommand;

namespace CommandCollection
{
	extern std::vector<ConsoleCommand*> commandTable;
	
	void InitializeCommands();
	void InsertCommand(ConsoleCommand* newCommand);
}

#endif
