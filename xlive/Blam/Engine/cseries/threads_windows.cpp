#include "stdafx.h"
#include "threads_windows.h"

/* globals */

const DWORD g_thread_system_initialized = GetCurrentThreadId();

/* public code */

bool is_main_thread(void)
{
	return g_thread_system_initialized == GetCurrentThreadId();
}
