#pragma once

/* prototypes */

void shell_windows_ipc_initialize();
void shell_windows_ipc_shutdown();
bool shell_windows_ipc_is_server();
bool shell_windows_ipc_notify_session_info(XSESSION_INFO* session);