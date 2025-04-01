#include "stdafx.h"
#include "Startup.h"

#include "../Config.h"
#include "../H2MODShell.h"


#include "cseries/cseries_windows_debug_pc.h"
#include "shell/shell.h"
#include "shell/shell_windows.h"

#include "H2MOD.h"
#include "H2MOD/Modules/Accounts/AccountLogin.h"
#include "H2MOD/Modules/OnScreenDebug/OnscreenDebug.h"
#include "H2MOD/Utils/Utils.h"

/* constants */

static const wchar_t k_client_process_name[] = L"Halo2Client";
static const wchar_t k_server_process_name[] = L"H2Server";
static const wchar_t k_microsoft_folder[] = L"\\Microsoft";
static const wchar_t k_appdata_default_path[] = L"\\Halo 2\\";
static const wchar_t k_appdata_dev_preview_path[] = L"DevPreview\\";

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

CRITICAL_SECTION log_section;

static void startup_force_working_directory_to_process_directory(void);

void PostH2Config() {

	wchar_t mutexName2[256];
	swprintf(mutexName2, ARRAYSIZE(mutexName2), L"Halo2BasePort#%d", H2Config_base_port);
	/*HANDLE mutex2 =*/ CreateMutex(0, TRUE, mutexName2);
	DWORD lastErr2 = GetLastError();
	if (lastErr2 == ERROR_ALREADY_EXISTS)
	{
		addDebugText("Base port %d is already bound to!\nExpect MP to not work!", H2Config_base_port);
		_Shell::OpenMessageBox(NULL, MB_ICONWARNING, "BASE PORT BIND WARNING!", "Base port %d is already bound to!\nExpect MP to not work!", H2Config_base_port);
	}
	addDebugText("Base port: %d.", H2Config_base_port);
}

void InitLocalAppData()
{
	addDebugText("Find AppData Local.");

	const wchar_t* localappdata_env = _wgetenv(L"localappdata");

	wchar_t appdata_path[MAX_PATH];
	ustrncpy(appdata_path, localappdata_env, NUMBEROF(appdata_path));
	ustrncat(appdata_path, k_microsoft_folder, NUMBEROF(k_microsoft_folder));
	ustrncat(appdata_path, k_appdata_default_path, NUMBEROF(k_appdata_default_path));

	// Make sure main folder exists and create if not
	const errno_t error = _waccess_s(appdata_path, 6);
	if (error != ERROR_SUCCESS)
	{
		CreateDirectoryW(appdata_path, NULL);
	}
	
	// Run folder checks if we're using dev preview paths 
	if (USE_DEV_PREVIEW_CONFIG_FILE_PATHS)
	{
		ustrncat(appdata_path, k_appdata_dev_preview_path, NUMBEROF(k_appdata_dev_preview_path));
		
		// Make sure dev preview folder exists and create if not
		const errno_t dev_path_error = _waccess_s(appdata_path, 6);
		if (dev_path_error != ERROR_SUCCESS)
		{
			CreateDirectoryW(appdata_path, NULL);
		}
	}

	ustrncpy(g_h2_appdata_local_path, appdata_path, MAX_PATH);
	return;
}

// use only after initLocalAppData has been called
// by default useAppDataLocalPath is set to true, if not specified
void prepareLogFileName(const wchar_t* logFileName, c_static_wchar_string<MAX_PATH>* path)
{
	const wchar_t* process_name = shell_is_dedicated_server() ? k_server_process_name : k_client_process_name;
	
	// We set the instance string based on whether or not 
	const wchar_t* instance_string;
	wchar_t instance_number_string[4];
	if (config_use_instance_name())
	{
		instance_string =  g_shell_windows_instance_name;
	}
	else
	{
		usnprintf(instance_number_string, NUMBEROF(instance_number_string), L"%d", g_instance_number);
		instance_string = instance_number_string;
	}


	c_static_wchar_string<MAX_PATH> folders;
	folders.append(L"logs\\");
	folders.append(process_name);
	folders.append(L"\\instance");
	folders.append(instance_string);

	path->set(g_h2_portable ? L"" : g_h2_appdata_local_path);
	path->append(folders.get_string());

	// try making logs directory
	if (!filesystem::create_directories(path->get_string()) && !filesystem::is_directory(filesystem::status(path->get_string())))
	{
		// try locally if we didn't already
		if (!g_h2_portable
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

	DETOUR_COMMIT();	
	
	InitLocalAppData();

	// initialize curl
	curl_global_init(CURL_GLOBAL_ALL);
	atexit([] { curl_global_cleanup(); });

	addDebugText(shell_is_dedicated_server() ? "Process is Dedi-Server" : "Process is Client");

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

void startup_initialize_log_directories(void)
{
	InitOnScreenDebugText();

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

	LeaveCriticalSection(&log_section);
	return;
}

static void startup_force_working_directory_to_process_directory(void)
{
	int ArgCnt;
	LPWSTR* ArgList = CommandLineToArgvW(GetCommandLineW(), &ArgCnt);

	c_static_wchar_string<MAX_PATH> path(ArgList[0]);
	
	// Remove the exe name by terminating the string at the last backslash character
	const int32 backslash_index = path.last_index_of(L"\\");
	path.get_buffer()[backslash_index] = L'\0';

	ustrncpy(g_h2_process_file_path, path.get_string(), MAX_PATH);

	// Force the current working directory to the one where halo2.exe is located
	SetCurrentDirectoryW(g_h2_process_file_path);
	return;
}
