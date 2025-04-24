#pragma once

/* prototypes */

void write_crash_dump_files(_EXCEPTION_POINTERS* ExceptionInfo, c_static_wchar_string<MAX_PATH>* report_path, c_static_wchar_string<MAX_PATH>* archive_path);
