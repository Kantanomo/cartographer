#pragma once


/* typedef */

typedef void (*shell_windows_ipc_handler)(const void* data, uint32 size);

/* enum */

enum e_shell_windows_ipc_message_type : uint8
{
	_shell_windows_ipc_message_type_join = 0,

	k_shell_windows_ipc_message_type_count
};

/* types */

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

/* public interface */

void shell_windows_ipc_initialize();
void shell_windows_ipc_shutdown();
bool shell_windows_ipc_is_server();
bool shell_windows_ipc_notify_session_info(XSESSION_INFO* session);