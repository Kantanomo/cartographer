#pragma once
#include "cseries/cseries_strings.h"

void InitH2Startup();
void H2DedicatedServerStartup();
void DeinitH2Startup();

// use only after initLocalAppData has been called
// by default useAppDataLocalPath is set to true, if not specified
inline void prepareLogFileName(const wchar_t* logFileName, c_static_wchar_string<MAX_PATH>* path, bool useAppDataLocalPath = true);

extern wchar_t g_h2_process_file_path[MAX_PATH];
extern wchar_t g_h2_appdata_local_path[MAX_PATH];
extern wchar_t* g_h2_config_path_override;