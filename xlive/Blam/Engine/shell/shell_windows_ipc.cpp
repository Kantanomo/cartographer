#include "stdafx.h"
#include "shell_windows_ipc.h"
#include "interface/user_interface_networking.h"

/* constants */

static const wchar_t* const k_ipc_mutex_name = L"Local\\cartographer_ipc_mutex";
static const wchar_t* const k_ipc_pipe_name = L"\\\\.\\pipe\\cartographer_ipc";

enum
{
	k_ipc_max_message_size = 512,
	k_ipc_connect_timeout = 5000,
	k_ipc_send_retry_count = 10,
	k_ipc_send_retry_delay = 100,
	k_ipc_thread_sleep_time = 1000
};

/* enum */

enum e_shell_windows_ipc_message_type : uint8
{
	_shell_windows_ipc_message_type_join = 0,

	k_shell_windows_ipc_message_type_count
};

/* structures */

struct shell_windows_ipc_state
{
	HANDLE mutex;
	HANDLE server_thread;
	volatile bool shutdown_requested;
	bool is_server;
};

#pragma pack(push, 1)
struct shell_windows_ipc_message_header
{
	e_shell_windows_ipc_message_type type;
};

struct shell_windows_ipc_join_message
{
	shell_windows_ipc_message_header header;
	XSESSION_INFO session;
};
#pragma pack(pop)

/* prototypes */

static bool shell_windows_ipc_try_claim_server_role();
static HANDLE shell_windows_ipc_create_server_pipe();

static bool shell_windows_ipc_wait_for_client(HANDLE pipe);
static void shell_windows_ipc_process_client(HANDLE pipe);

static DWORD WINAPI shell_windows_ipc_server_thread(LPVOID param);
static bool shell_windows_ipc_start_server();

static bool shell_windows_ipc_send_raw(uint8* data_buffer, uint32 data_size);
static bool shell_windows_ipc_send_with_retry(uint8* data_buffer, uint32 data_size);
static void shell_windows_ipc_handle_message(uint8* data_buffer, uint32 buffer_size);
static void shell_windows_ipc_handle_join_message(uint8* session_buffer, uint32 buffer_size);

/* globals */

static shell_windows_ipc_state g_ipc_state;

static CRITICAL_SECTION g_ipc_message_lock;

/* public code */

void shell_windows_ipc_initialize()
{
	if (shell_windows_ipc_try_claim_server_role())
	{
		g_ipc_state.is_server = true;
		InitializeCriticalSection(&g_ipc_message_lock);
		if (!shell_windows_ipc_start_server())
		{
			ReleaseMutex(g_ipc_state.mutex);
			CloseHandle(g_ipc_state.mutex);
			DeleteCriticalSection(&g_ipc_message_lock);
			g_ipc_state.mutex = NULL;
			g_ipc_state.is_server = false;
		}
	}
	else
	{
		g_ipc_state.is_server = false;
	}
}

void shell_windows_ipc_shutdown()
{
	g_ipc_state.shutdown_requested = true;

	if (g_ipc_state.server_thread != NULL)
	{
		HANDLE pipe = CreateFileW(
			k_ipc_pipe_name,
			GENERIC_WRITE,
			0,
			NULL,
			OPEN_EXISTING,
			0,
			NULL
		);

		if (pipe != INVALID_HANDLE_VALUE)
		{
			CloseHandle(pipe);
		}

		WaitForSingleObject(g_ipc_state.server_thread, 3000);
		CloseHandle(g_ipc_state.server_thread);
		g_ipc_state.server_thread = NULL;
	}

	if (g_ipc_state.mutex != NULL)
	{
		ReleaseMutex(g_ipc_state.mutex);
		CloseHandle(g_ipc_state.mutex);
		g_ipc_state.mutex = NULL;
	}

	DeleteCriticalSection(&g_ipc_message_lock);
}

bool shell_windows_ipc_is_server()
{
	return g_ipc_state.is_server;
}

bool shell_windows_ipc_notify_session_info(XSESSION_INFO* session)
{
	if (g_ipc_state.is_server)
	{
		return false;
	}

	if (session == NULL)
	{
		return false;
	}

	shell_windows_ipc_join_message msg{};

	msg.header.type = _shell_windows_ipc_message_type_join;

	msg.session = *session;

	return shell_windows_ipc_send_with_retry((uint8*)&msg, sizeof(msg));
}

/* private code */

static bool shell_windows_ipc_try_claim_server_role()
{
	g_ipc_state.mutex = CreateMutexW(NULL, FALSE, k_ipc_mutex_name);

	if (g_ipc_state.mutex == NULL)
	{
		return false;
	}

	uint32 wait_result = WaitForSingleObject(g_ipc_state.mutex, 0);

	switch (wait_result)
	{
		case WAIT_OBJECT_0:
			return true;

		case WAIT_ABANDONED:
			return true;

		default:
			CloseHandle(g_ipc_state.mutex);
			g_ipc_state.mutex = NULL;
			return false;
	}
}

static HANDLE shell_windows_ipc_create_server_pipe()
{
	HANDLE pipe = CreateNamedPipeW(
		k_ipc_pipe_name,
		PIPE_ACCESS_INBOUND,
		PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
		1,
		0,
		k_ipc_max_message_size,
		0,
		NULL
	);

	if (pipe == INVALID_HANDLE_VALUE)
	{
		ASSERT(GetLastError());
	}

	return pipe;
}

static bool shell_windows_ipc_wait_for_client(HANDLE pipe)
{
	bool result = ConnectNamedPipe(pipe, NULL);

	if (result)
	{
		return true;
	}

	uint32 error = GetLastError();

	if (error == ERROR_PIPE_CONNECTED)
	{
		ASSERT(error);
		return true;
	}

	return false;
}

static void shell_windows_ipc_process_client(HANDLE pipe)
{
	uint8 buffer[k_ipc_max_message_size];
	uint32 bytes_read = 0;

	if (!ReadFile(pipe, buffer, sizeof(buffer), &bytes_read, NULL))
	{
		return;
	}

	if (bytes_read == 0)
	{
		return;
	}

	shell_windows_ipc_handle_message(buffer, bytes_read);
}

static DWORD WINAPI shell_windows_ipc_server_thread(LPVOID param)
{
	SUPRESS_UNUSED(param);

	while (!g_ipc_state.shutdown_requested)
	{
		HANDLE pipe = shell_windows_ipc_create_server_pipe();

		if (pipe == INVALID_HANDLE_VALUE)
		{
			Sleep(k_ipc_thread_sleep_time);
			continue;
		}

		if (shell_windows_ipc_wait_for_client(pipe))
		{
			shell_windows_ipc_process_client(pipe);
		}

		DisconnectNamedPipe(pipe);
		CloseHandle(pipe);
	}

	return 0;
}

static bool shell_windows_ipc_start_server()
{
	g_ipc_state.shutdown_requested = false;

	g_ipc_state.server_thread = CreateThread(
		NULL,
		0,
		shell_windows_ipc_server_thread,
		NULL,
		0,
		NULL
	);

	return g_ipc_state.server_thread != NULL;
}

static bool shell_windows_ipc_send_raw(uint8* data_buffer, uint32 data_size)
{
	if (data_buffer == nullptr || data_size == 0 || data_size > k_ipc_max_message_size)
	{
		return false;
	}

	if (!WaitNamedPipeW(k_ipc_pipe_name, k_ipc_connect_timeout))
	{
		return false;
	}

	HANDLE pipe = CreateFileW(
		k_ipc_pipe_name,
		GENERIC_WRITE,
		0,
		NULL,
		OPEN_EXISTING,
		0,
		NULL
	);

	if (pipe == INVALID_HANDLE_VALUE)
	{
		return false;
	}

	uint32 bytes_written = 0;
	bool success = WriteFile(pipe, data_buffer, data_size, &bytes_written, NULL);

	FlushFileBuffers(pipe);
	CloseHandle(pipe);

	if (!success)
	{
		return false;
	}

	if (bytes_written != data_size)
	{
		return false;
	}

	return true;
}

static bool shell_windows_ipc_send_with_retry(uint8* data_buffer, uint32 data_size)
{
	for (uint32 attempt = 0; attempt < k_ipc_send_retry_count; attempt++)
	{
		if (attempt > 0)
		{
			Sleep(k_ipc_send_retry_delay);
		}

		if (shell_windows_ipc_send_raw(data_buffer, data_size))
		{
			return true;
		}
	}

	return false;
}

static void shell_windows_ipc_handle_join_message(uint8* session_buffer, uint32 buffer_size)
{
	if (buffer_size < sizeof(XSESSION_INFO))
	{
		return;
	}

	XSESSION_INFO session;

	memcpy(&session, session_buffer, sizeof(XSESSION_INFO));

	EnterCriticalSection(&g_ipc_message_lock);
	g_game_auto_join.auto_join_session = session;
	g_game_auto_join.do_auto_join = true;
	LeaveCriticalSection(&g_ipc_message_lock);
}

static void shell_windows_ipc_handle_message(uint8* data_buffer, uint32 buffer_size)
{
	if (data_buffer == nullptr || buffer_size < sizeof(shell_windows_ipc_message_header))
	{
		return;
	}

	e_shell_windows_ipc_message_type type = (e_shell_windows_ipc_message_type)data_buffer[0];

	uint8* message_buffer = &data_buffer[1];

	switch (type)
	{
		case _shell_windows_ipc_message_type_join:
			shell_windows_ipc_handle_join_message(message_buffer, buffer_size - 1);
			break;
	}
}

