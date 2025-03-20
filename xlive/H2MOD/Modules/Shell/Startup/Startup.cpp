#include "stdafx.h"
#include "Startup.h"

#include "../Config.h"
#include "../H2MODShell.h"


#include "cseries/cseries_windows_debug_pc.h"
#include "shell/shell.h"
#include "shell/shell_windows.h"

#include "H2MOD.h"
#include "H2MOD/Modules/Accounts/AccountLogin.h"
#include "H2MOD/Modules/Accounts/Accounts.h"
#include "H2MOD/Modules/OnScreenDebug/OnscreenDebug.h"
#include "H2MOD/Utils/Utils.h"

#include "Util/filesys.h"

const wchar_t* k_client_process_name = L"Halo2Client";
const wchar_t* k_server_process_name = L"H2Server";

namespace filesystem = std::filesystem;

// xLiveLess specific logger
h2log *xlive_log = nullptr;

// Mod specific logger
h2log *h2mod_log = nullptr;

// General network logger
h2log *network_log = nullptr;

// Map loading logger
h2log *checksum_log = nullptr;

// OnScreenDebug logger
h2log *onscreendebug_log = nullptr;

// Console logger, receives output from all loggers
h2log *console_log = nullptr;


wchar_t g_h2_process_file_path[MAX_PATH];
wchar_t g_h2_appdata_local_path[MAX_PATH];

wchar_t* g_h2_config_path_override = NULL;

void PostH2Config() {

	wchar_t mutexName2[256];
	swprintf(mutexName2, ARRAYSIZE(mutexName2), L"Halo2BasePort#%d", H2Config_base_port);
	HANDLE mutex2 = CreateMutex(0, TRUE, mutexName2);
	DWORD lastErr2 = GetLastError();
	if (lastErr2 == ERROR_ALREADY_EXISTS) {
		addDebugText("Base port %d is already bound to!\nExpect MP to not work!", H2Config_base_port);
		_Shell::OpenMessageBox(NULL, MB_ICONWARNING, "BASE PORT BIND WARNING!", "Base port %d is already bound to!\nExpect MP to not work!", H2Config_base_port);
	}
	addDebugText("Base port: %d.", H2Config_base_port);
}

void InitLocalAppData() {
	addDebugText("Find AppData Local.");

	wchar_t* userprofile = _wgetenv(L"USERPROFILE");

	wchar_t local2[MAX_PATH];

	swprintf(local2, ARRAYSIZE(local2), L"%ws\\AppData\\Local\\", userprofile);
	struct _stat64i32 sb;
	if (_wstat(local2, &sb) == 0 && sb.st_mode & S_IFDIR) {
		swprintf(local2, ARRAYSIZE(local2), L"%ws\\AppData\\Local\\Microsoft\\", userprofile);
		CreateDirectoryW(local2, NULL);
		int fperrno1 = GetLastError();
		if (fperrno1 == ERROR_ALREADY_EXISTS || fperrno1 == ERROR_SUCCESS) {
#if USE_DEV_PREVIEW_CONFIG_FILE_PATHS
			swprintf(local2, ARRAYSIZE(local2), L"%ws\\AppData\\Local\\Microsoft\\Halo 2\\DevPreview\\", userprofile);
#else
			swprintf(local2, ARRAYSIZE(local2), L"%ws\\AppData\\Local\\Microsoft\\Halo 2\\", userprofile);
#endif
			CreateDirectoryW(local2, NULL);
			int fperrno1 = GetLastError();
			if (fperrno1 == ERROR_ALREADY_EXISTS || fperrno1 == ERROR_SUCCESS) {
				int appdatabuflen = wcslen(local2) + 1;
				wcscpy_s(g_h2_appdata_local_path, appdatabuflen, local2);
			}
		}
	}
	else if (swprintf(local2, ARRAYSIZE(local2), L"%ws\\Local Settings\\Application Data\\", userprofile), _wstat(local2, &sb) == 0 && sb.st_mode & S_IFDIR)
	{
		swprintf(local2, ARRAYSIZE(local2), L"%ws\\Local Settings\\Application Data\\Microsoft\\", userprofile);
		CreateDirectoryW(local2, NULL);
		int fperrno1 = GetLastError();
		if (fperrno1 == ERROR_ALREADY_EXISTS || fperrno1 == ERROR_SUCCESS) {
#if USE_DEV_PREVIEW_CONFIG_FILE_PATHS
			swprintf(local2, ARRAYSIZE(local2), L"%ws\\Local Settings\\Application Data\\Microsoft\\Halo 2\\DevPreview\\", userprofile);
#else
			swprintf(local2, ARRAYSIZE(local2), L"%ws\\Local Settings\\Application Data\\Microsoft\\Halo 2\\", userprofile);
#endif
			CreateDirectoryW(local2, NULL);
			int fperrno1 = GetLastError();
			if (fperrno1 == ERROR_ALREADY_EXISTS || fperrno1 == ERROR_SUCCESS) {
				int appdatabuflen = wcslen(local2) + 1;
				wcscpy_s(g_h2_appdata_local_path, appdatabuflen, local2);
			}
		}
	}

	if (g_h2_appdata_local_path == nullptr) {
		int appdatabuflen = wcslen(g_h2_process_file_path) + 1;
		wcscpy_s(g_h2_appdata_local_path, appdatabuflen, g_h2_process_file_path);
		addDebugText("ERROR: Could not find AppData Local. Using Process File Path:");
		addDebugText(g_h2_appdata_local_path);
	}
	else {
		addDebugText("Found AppData Local: %s", g_h2_appdata_local_path);
	}
}

CRITICAL_SECTION log_section;

// use only after initLocalAppData has been called
// by default useAppDataLocalPath is set to true, if not specified
void prepareLogFileName(const wchar_t* logFileName, c_static_wchar_string<MAX_PATH>* path, bool useAppDataLocalPath) {
	const wchar_t* process_name = shell_is_dedicated_server() ? k_server_process_name : k_client_process_name;
	
	path->set(useAppDataLocalPath ? g_h2_appdata_local_path : L"");

	wchar_t instance_string[3] = {};
	swprintf(instance_string, NUMBEROF(instance_string), L"%d", _Shell::GetInstanceId());
	
	c_static_wchar_string<MAX_PATH> folders;
	folders.append(L"logs\\");
	folders.append(process_name);
	folders.append(L"\\instance");
	folders.append(instance_string);

	path->append(folders.get_string());

	// try making logs directory
	if (!filesystem::create_directories(path->get_string()) && !filesystem::is_directory(filesystem::status(path->get_string())))
	{
		// try locally if we didn't already
		if (useAppDataLocalPath
			&& filesystem::create_directories(folders.get_string()) || filesystem::is_directory(filesystem::status(folders.get_string())))
			path->set(folders.get_string());
		else
			path->set(L""); // fine then
	}

	path->append(L"\\");
	path->append(logFileName);
	path->append(L".log");
	return;
}

///Before the game window appears
void InitH2Startup() {
	InitializeCriticalSection(&log_section);

	DETOUR_BEGIN();
	cseries_windows_debug_initialize();
	Memory::Initialize();

	shell_windows_initialize();
	shell_apply_patches();
	shell_windows_apply_patches();
	// ### TODO remove entirely
	_Shell::Initialize();
	DETOUR_COMMIT();

	int ArgCnt;
	LPWSTR* ArgList = CommandLineToArgvW(GetCommandLineW(), &ArgCnt);
	int rtncodepath = GetWidePathFromFullWideFilename(ArgList[0], g_h2_process_file_path);
	if (rtncodepath == -1) {
		std::wstring path = GetExeDirectoryWide();
		path.append(L"\\");
		_swprintf(g_h2_process_file_path, path.c_str());
	}

	// fix the game not finding the files it needs if the current directory is not the install directory
	SetCurrentDirectoryW(GetExeDirectoryWide().c_str());
	//If H2ProcessFilePath is empty (Server Console Mode?) set to working directory
	
	InitLocalAppData();

	// initialize curl
	curl_global_init(CURL_GLOBAL_ALL);
	atexit([] { curl_global_cleanup(); });

	// after localAppData filepath initialized, we can initialize OnScreenDebugLog
	InitOnScreenDebugText();

	addDebugText(shell_is_dedicated_server() ? "Process is Dedi-Server" : "Process is Client");

	// DO NOT FUCKING TOUCH THIS
	if (ArgList != NULL)
	{
		for (int i = 0; i < ArgCnt; i++)
		{
			if (wcsstr(ArgList[i], L"-h2config=") != NULL)
			{
				if (wcslen(ArgList[i]) < 255)
				{
					int pfcbuflen = wcslen(ArgList[i] + 10) + 1;
					g_h2_config_path_override = (wchar_t*)malloc(sizeof(wchar_t) * pfcbuflen);
					swprintf(g_h2_config_path_override, pfcbuflen, ArgList[i] + 10);
				}
			}
		}
	}
	// END OF THE COMMENT ABOVE

	InitH2Config();
	PostH2Config();

	EnterCriticalSection(&log_section);

	// prepare default log files if enabled, after we read the H2Config
	bool should_enable_console_log = H2Config_debug_log && H2Config_debug_log_console;
	console_log = h2log::create_console("CONSOLE MAIN", should_enable_console_log, H2Config_debug_log_level);

	c_static_wchar_string<MAX_PATH> path;
	prepareLogFileName(L"h2xlive", &path);
	xlive_log = h2log::create("XLive", path.get_string(), H2Config_debug_log, H2Config_debug_log_level);
	LOG_DEBUG_XLIVE(DLL_VERSION_STR);

	prepareLogFileName(L"h2mod", &path);
	h2mod_log = h2log::create("H2MOD", path.get_string(), H2Config_debug_log, H2Config_debug_log_level);
	LOG_DEBUG_GAME(DLL_VERSION_STR);
	
	prepareLogFileName(L"h2network", &path);
	network_log = h2log::create("Network", path.get_string(), H2Config_debug_log, H2Config_debug_log_level);
	LOG_DEBUG_NETWORK(DLL_VERSION_STR);

	//checksum_log = h2log::create("Checksum", prepareLogFileName(L"checksum"), true, 0);
	LeaveCriticalSection(&log_section);
	InitH2Accounts();

	H2MOD::Initialize();

	addDebugText("ProcessStartup finished.");
}

///After the game window appears
void H2DedicatedServerStartup() {
	// initialize default data to run under LAN
	// if the server runs in LIVE mode, check XLiveSignIn/XLiveSignOut in AccountLogin.cpp
	if (shell_is_dedicated_server())
	{
		addDebugText("Signing in dedicated server locally.");

		AccountEdit_remember = false;

		BYTE abEnet[6];
		BYTE abOnline[20];
		XNetRandom(abEnet, sizeof(abEnet));
		XNetRandom(abOnline, sizeof(abOnline));
		ConfigureUserDetails("[Username]", "12345678901234567890123456789012", rand(), 0, H2Config_ip_lan, ByteToHexStr(abEnet, sizeof(abEnet)).c_str(), ByteToHexStr(abOnline, sizeof(abOnline)).c_str(), false);
	}
}

void DeinitH2Startup() {
	extern void DeinitRunLoop();
	DeinitRunLoop();
	extern void DeinitCustomLanguage();
	DeinitCustomLanguage();
	DeinitH2Accounts();
	DeinitH2Config();

	free(g_h2_config_path_override);
	return;
}
