#include "stdafx.h"
#include "cseries.h"

#include "shell/shell.h"

/* constants */

enum
{
	MAXIMUM_MEMCPY_SIZE = 0x20000000,
	MAXIMUM_MEMMOVE_SIZE = 0x20000000,
	MAXIMUM_MEMSET_SIZE = 0x20000000,
	MAXIMUM_MEMCMP_SIZE = 0x20000000,
	MAXIMUM_STRING_SIZE = 0x40000,
};

/* globals */

// TODO: figure out a decent way to strip this from release builds
char g_temporary[256] = {};

#ifdef ASSERTS_ENABLED
bool g_catch_exceptions = true;
#endif

/* public code */

void cseries_initialize(void)
{
	// TODO: implement debug logic
	return;
}

#ifdef ASSERTS_ENABLED
void display_assert(const char* condition, char const* file, int32 line, bool assertion_failed)
{
	if (assertion_failed && !is_debugger_present())
	{
		error(_error_log, "");
		// sub_402370(1);
	}

	error(_error_log, "");
	if (is_debugger_present())
	{
		const char* condition_string = condition != NULL ? condition : "";
		const char* error_type = assertion_failed ? "ASSERT" : "WARNING";
		error(_error_log, "%s(%d): %s: %s", file, line, error_type, condition_string);
	}
	else
	{
		error(_error_log, "%s", shell_get_version());
		const char* error_type = assertion_failed ? "### ASSERTION FAILED: " : "### RUNTIME WARNING: ";
		error(_error_log, "%s at %s,#%d", error_type, file, line);
		if (condition)
		{
			error(_error_log, "  %s", condition);
		}
	}

	if (assertion_failed)
	{
		// cseries_error_callbacks_run();
		if (!is_debugger_present())
		{
			RaiseException(0x73746Bu, 0, 0, 0);
			// halt_and_catch_fire();
			// cseries_windows_trigger_debug_window(-1, condition);
		}
	}
	return;
}
#endif

void* csmemmove(void* destination, void* source, size_t size)
{
	ASSERT(size == 0 || (destination && source));
	ASSERT(size >= 0 && size <= MAXIMUM_MEMMOVE_SIZE);
	return memmove(destination, source, size);
}

void memmove_guarded(void* write_start, const void* src, size_t size, void* bounds_lower, size_t bounds_size)
{
	if (size > 0)
	{
#ifdef ASSERTS_ENABLED
		const void* write_end = (int8*)write_start + size - 1;
		const void* bounds_upper = (int8*)bounds_lower + bounds_size - 1;
		ASSERT(bounds_upper >= bounds_lower);
		ASSERT(bounds_size > 0);
		ASSERT((write_start >= bounds_lower) && (write_start <= bounds_upper));
		ASSERT((write_end >= bounds_lower) && (write_end <= bounds_upper));
#endif
		memmove(write_start, src, size);
	}
	return;
}

void* csmemset(void* destination, int32 val, size_t size)
{
	ASSERT(size == 0 || destination);
	ASSERT(size >= 0 && size <= MAXIMUM_MEMSET_SIZE);
	return memset(destination, val, size);
}

void* csmemcpy(void* destination, const void* source, size_t size)
{
	ASSERT(size == 0 || (destination && source));
	ASSERT(size >= 0 && size < MAXIMUM_MEMCPY_SIZE);
	ASSERT((byte*)source + size <= (byte*)destination || (byte*)destination + size <= (byte*)source);

	return memcpy(destination, source, size);
}

int csmemcmp(const void* p1, const void* p2, size_t size)
{
	ASSERT(size == 0 || (p1 && p2));
	ASSERT(size >= 0 && size < MAXIMUM_MEMCMP_SIZE);
	return memcmp(p1, p2, size);
}

int32 vsprintf(char* buffer, size_t size, const char* format, va_list va_args)
{
	ASSERT(buffer);
	ASSERT(format);
	ASSERT(size > 0);

	const int32 result = (int32)_vsnprintf_s(buffer, size, _TRUNCATE, format, va_args);
	return result;
}

int32 vsnprintf(char* buffer, size_t size, size_t max_count, const char* format, char* ap)
{
	ASSERT(buffer);
	ASSERT(format);
	ASSERT(size > 0);

	const int32 result = (int32)_vsnprintf_s(buffer, size, max_count, format, ap);
	return result;
}

const char* csprintf(char* buffer, size_t size, const char* format, ...)
{
	va_list va_args;
	va_start(va_args, format);
	(void)vsprintf(buffer, size, format, va_args);
	va_end(va_args);
	return buffer;
}

const char* csnprintf(char* buffer, size_t size, size_t max_count, const char* format, ...)
{
	va_list va_args;
	va_start(va_args, format);
	(void)vsnprintf(buffer, size, max_count, format, va_args);
	va_end(va_args);
	return buffer;
}

size_t csstrnlen(const char* s, size_t size)
{
	ASSERT(s);
	ASSERT(size >= 0 && size < MAXIMUM_STRING_SIZE);

	// Do a manual loop through every character until we reach the null terminator to get the size
	// This is originally how it was in h2
	size_t length = 0;
	for (; length < size; ++length)
	{
		if (s[length] == '\0')
		{
			break;
		}
	}
	return length;
}

char* csstrncpy(char* s1, const char* s2, size_t size)
{
	ASSERT(s1 && s2);
	ASSERT(size > 0 && size < MAXIMUM_STRING_SIZE);
	strncpy_s(s1, size, s2, UINT_MAX);
	return s1;
}

char* csstrncat(char* s1, char const* s2, size_t size)
{
	ASSERT(s1 && s2);
	ASSERT(size > 0 && size <= MAXIMUM_STRING_SIZE);
	strncat_s(s1, size, s2, UINT_MAX);
	return s1;
}

char* csstrnupr(char* s, size_t size)
{
	ASSERT(s);
	ASSERT(size >= 0 && size < MAXIMUM_STRING_SIZE);

	for (size_t i = 0; s[i] != '\0' && i < size; ++i)
	{
		s[i] = (char)toupper(s[i]);
	}

	return s;
}

int32 csstricmp(const char* s1, const char* s2)
{
	ASSERT(s1 && s2);
	return strcmp(s1, s2);
}

int32 csstrncmp(const char* s1, const char* s2, size_t size)
{
	ASSERT(s1 && s2);
	ASSERT(size >= 0 && size < MAXIMUM_STRING_SIZE);
	return strncmp(s1, s2, size);
}
