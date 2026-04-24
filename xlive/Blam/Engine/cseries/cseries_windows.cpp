#include "stdafx.h"
#include "cseries_windows.h"

#include "main/main.h"

/* globals */

static bool g_set_always_a_debugger_present = false;
static bool g_set_never_a_debugger_present = false;

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
	bool present = g_set_always_a_debugger_present || IsDebuggerPresent();
	return !g_set_never_a_debugger_present && !force_crash_uploads && present;
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

void handle_fatal_error(int32 code, const char* error)
{
	char string[512];
	csprintf(
		string,
		NUMBEROF(string),
		"%s\nWould you like a programmer to debug this?\n(You can also view more extensive error messages in debug.txt)",
		error != NULL ? error : "<unknown error>"
	);

	char env[2];
	if (!GetEnvironmentVariableA("DONT_TREAD_ON_ME_WITH_DEBUGGING_DIALOGS", env, NUMBEROF(env)))
	{
		if (MessageBoxA(NULL, string, code == NONE ? "ASSERTION FAILED" : "FATAL EXECUTION ERROR", MB_YESNO | MB_TASKMODAL) == IDYES)
		{
			char dir[512] = {};
			GetCurrentDirectoryA(NUMBEROF(dir), dir);
			
			const int32 reason = code != NONE ? code : EXCEPTION_BREAKPOINT;
			csnappendf(dir, NUMBEROF(dir), "\\bin\\windbg.exe.lnk -p %ld -e %ld -c \"~000 S\"", GetCurrentProcessId(), reason);
			if (!system(dir))
			{
				while (true)
				{
					bool result = false;
					const HMODULE h_kernel32 = LoadLibraryA("Kernel32.dll");
					if (h_kernel32)
					{
						decltype(&IsDebuggerPresent) IsDebuggerPresentProc = ((decltype(&IsDebuggerPresent))
							GetProcAddress(
								h_kernel32,
								"IsDebuggerPresent"
							)
						);
						if (IsDebuggerPresentProc)
						{
							result = IsDebuggerPresentProc() != false;
						}
						FreeLibrary(h_kernel32);
					}

					if (!force_crash_uploads && !g_set_never_a_debugger_present && (g_set_always_a_debugger_present || result))
					{
						break;
					}

					Sleep(0);
				}
			}
		}
	}
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

uint32 system_seconds(void)
{
	return (uint32)_time64(NULL);
}