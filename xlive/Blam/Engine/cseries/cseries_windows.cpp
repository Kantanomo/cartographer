#include "stdafx.h"
#include "cseries_windows.h"

/* globals */

static bool g_debugger_flag_0 = false;
static bool g_debugger_flag_1 = false;
static bool g_debugger_flag_2 = false;

/* public code */

void cseries_windows_tool_apply_patches(void)
{
	PatchCall(Memory::GetAddress(0x3F2B6), system_get_user_name);
	PatchCall(Memory::GetAddress(0x3F6D4), system_get_user_name);
	PatchCall(Memory::GetAddress(0xDE75D), system_get_user_name);
	PatchCall(Memory::GetAddress(0x2389A5), system_get_user_name);
	return;
}

bool is_debugger_present(void)
{
	bool present = g_debugger_flag_0 || IsDebuggerPresent();
	return !g_debugger_flag_1 && !g_debugger_flag_2 && present;
}

void display_debug_string(const char* format)
{
	char output[4096];
	if (is_debugger_present())
	{
		output[0] = '\0';
		csstrncpy(output, format, NUMBEROF(output));
		csstrncat(output, "\n", NUMBEROF(output));
		OutputDebugStringA(output);
	}
	return;
}

void system_get_date_and_time(char* string, int16 size, bool exclude_milliseconds)
{
	SYSTEMTIME time;
	GetLocalTime(&time);
	
	if (exclude_milliseconds)
	{
		csprintf(
			string,
			size,
			"%02d%02d%02d_%02d%02d%02d",
			time.wMonth,
			time.wDay,
			time.wYear % 100,
			time.wHour,
			time.wMinute,
			time.wSecond);
	}
	else
	{
		csprintf(
			string,
			size,
			"%02d.%02d.%02d %02d:%02d:%02d.%03d",
			time.wMonth,
			time.wDay,
			time.wYear % 100,
			time.wHour,
			time.wMinute,
			time.wSecond,
			time.wMilliseconds);
	}
	return;
}

void handle_fatal_error(int32 a1, const char* str)
{
	// TODO: implement
	return;
}

void __cdecl system_get_user_name(char* name, int16 size)
{
	wchar_t buffer[2];
	const bool dont_give_out_username = GetEnvironmentVariable(L"H2TOOL_DONT_GIVE_OUT_MY_USERNAME", buffer, NUMBEROF(buffer)) != 0;	// Added
	
	const DWORD dword_size = size;
	if (dont_give_out_username || !GetUserNameA(name, (LPDWORD)&dword_size))
	{
		csstrncpy(name, "User Name", size);
		SetLastError(0);
	}
	return;
}

void center_window(HWND hwnd)
{
	int screen_w = GetSystemMetrics(SM_CXSCREEN);
	int screen_h = GetSystemMetrics(SM_CYSCREEN);

	RECT rect;
	if (GetWindowRect(hwnd, &rect))
	{
		uint32 width = rect.right - rect.left;
		uint32 height = rect.bottom - rect.top;

		// Centering is done here
		SetWindowPos(hwnd, HWND_TOP, (screen_w - width) / 2, (screen_h - height) / 2, 0, 0, SWP_NOSIZE);
	}

	return;
}
