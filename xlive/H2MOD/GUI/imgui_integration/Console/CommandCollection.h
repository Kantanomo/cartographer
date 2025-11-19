#pragma once

#ifdef TERMINAL_ENABLED

class ConsoleCommand;

extern CRITICAL_SECTION g_command_insert_section;

namespace CommandCollection
{
	extern std::vector<ConsoleCommand*> commandTable;

	void InitializeCommands();
	void InsertCommand(ConsoleCommand* newCommand);
}

#endif
