#pragma once

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

/* prototypes */

void kablam_apply_patches(void);

int32 __cdecl kablam_shell_argument_index_get(const wchar_t* string);

const wchar_t* __cdecl kablam_shell_argument_get(const wchar_t* string);
