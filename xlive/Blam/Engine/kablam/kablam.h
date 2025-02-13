#pragma once
#include "H2MOD/GUI/ImGui_Integration/Console/CommandsUtil.h"

/* enums */

enum e_server_console_commands
{
	_kablam_command_ban,
	_kablam_command_description,
	_kablam_command_exit,
	_kablam_command_kick,
	_kablam_command_live,
	_kablam_command_name,
	_kablam_command_play,
	_kablam_command_players,
	_kablam_command_playing,
	_kablam_command_privacy,
	_kablam_command_sendmsg,
	_kablam_command_skip,
	_kablam_command_statsfolder,
	_kablam_command_status,
	_kablam_command_unban,
	_kablam_command_vip,
	_kablam_command_any,
	k_kablam_command_count
};

/* classes */

class c_kablam_interface
{
	int32 field_4;
	bool field_8;

public:
	virtual bool execute() = 0;
	virtual bool function_1() = 0;

	c_kablam_interface(int32 a1, bool a2)
	{
		field_4 = a1;
		field_8 = a2;
	}
};
ASSERT_STRUCT_SIZE(c_kablam_interface, 12);

class c_kablam_command_send_msg : protected c_kablam_interface
{
	wchar_t message[121];
	int32 field_100;

public:
	c_kablam_command_send_msg(const wchar_t* _message) 
		: c_kablam_interface(8, true)
	{
		wcscpy_s(message, ARRAYSIZE(message), _message);
		field_100 = 0;
	}

	virtual bool execute() override
	{
		return INVOKE_TYPE(0x0, 0x7175, bool(__thiscall*)(c_kablam_command_send_msg*), this);
	}

	virtual bool function_1() override
	{
		return false;
	}
};
ASSERT_STRUCT_SIZE(c_kablam_command_send_msg, 260);

namespace ServerConsole
{
	static int __cdecl OutputCb(StringHeaderFlags flags, const char* fmt, ...);

	void ApplyHooks();
	int LogToDedicatedServerConsoleWide(const wchar_t* fmt, ...);
	int LogToDedicatedServerConsole(const char* fmt, ...);
	void SendCommand(wchar_t** command, int32 split_commands_size, bool a3);
	void AddVip(std::wstring gamerTag);
	void ClearVip();
	void SendMsg(const wchar_t* message, bool timeout = false);
}

/* prototypes */

void kablam_apply_patches(void);
