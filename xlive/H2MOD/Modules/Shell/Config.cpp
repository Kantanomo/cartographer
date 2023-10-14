#include "stdafx.h"

#include "Config.h"

#include "Blam/Engine/cartographer/settings/settings.h"
#include "H2MOD/Modules/Shell/Shell.h"
#include "H2MOD/Modules/CustomMenu/CustomMenu.h"
#include "H2MOD/Modules/OnScreenDebug/OnscreenDebug.h"
#include "H2MOD/Modules/Shell/Startup/Startup.h"
#include "Util/SimpleIni.h"

#pragma region Config IO
const wchar_t* H2ConfigFilenames[] = { L"%wshalo2config%d.ini", L"%wsh2serverconfig%d.ini" };
const wchar_t* H2ConfigJsonFilenames[] = { L"%wshalo2config%d.json", L"%wsh2serverconfig%d.json" };

std::string H2ConfigVersionNumber("1");
std::string H2ConfigVersionSection("H2ConfigurationVersion:" + H2ConfigVersionNumber);

//config variables

std::string cartographerURL = "https://cartographer.online";
std::string cartographerMapRepoURL = "http://www.h2maps.net/Cartographer/CustomMaps";

bool ownsConfigFile = false;
bool H2Config_isConfigFileAppDataLocal = false;

void SaveH2Config() {
	addDebugText("Saving H2Configuration File...");

	if (!H2IsDediServer) {
		extern int current_language_main;
		extern int current_language_sub;
		cartographer_settings.language_code.code_main = current_language_main;
		cartographer_settings.language_code.code_variant = current_language_sub;
	}

	wchar_t fileConfigPath[1024];
	if (FlagFilePathConfig) {
		wcscpy_s(fileConfigPath, ARRAYSIZE(fileConfigPath), FlagFilePathConfig);
		size_t len = wcslen(fileConfigPath);
		if (len >= 4 && wcsncmp(fileConfigPath + len - 4, L".ini", 4) == 0) {
			wchar_t jsonPath[1024];
			wcsncpy(jsonPath, FlagFilePathConfig, len - 4);
			jsonPath[len - 4] = L'\0';
			wcscat(jsonPath, L".json\0");
			wcscpy_s(fileConfigPath, jsonPath);
		}
	}
	else if (cartographer_settings.h2portable || !H2Config_isConfigFileAppDataLocal) {
		swprintf(fileConfigPath, ARRAYSIZE(fileConfigPath), H2ConfigJsonFilenames[H2IsDediServer], H2ProcessFilePath, _Shell::GetInstanceId());
	}
	else {
		swprintf(fileConfigPath, ARRAYSIZE(fileConfigPath), H2ConfigJsonFilenames[H2IsDediServer], H2AppDataLocal, _Shell::GetInstanceId());
	}

	addDebugText(L"Saving config: \"%ws\"", fileConfigPath);

	easy_json_struct json(fileConfigPath, &cartographer_settings);

	//TODO add error checking to the save function
	json.save();
	/*if (rc < 0) {
		addDebugText("json.load() failed with error: %d while trying to read configuration file!", (int)rc);
	}
	else
	{
		json.save();
	}*/

	addDebugText("End saving H2Configuration file.");
}
void ReadH2Config() {
	addDebugText("Reading H2Configuration file...");

	int readInstanceIdFile = _Shell::GetInstanceId();
	wchar_t local[1024];
	wcscpy_s(local, ARRAYSIZE(local), H2AppDataLocal);
	H2Config_isConfigFileAppDataLocal = false;

	errno_t err = 0;
	FILE* fileConfig = nullptr;
	wchar_t fileConfigPath[1024];

	if (FlagFilePathConfig) {
		wcscpy_s(fileConfigPath, ARRAYSIZE(fileConfigPath), FlagFilePathConfig);
		size_t len = wcslen(fileConfigPath);
		if (len >= 4 && wcsncmp(fileConfigPath + len - 4, L".ini", 4) == 0) {
			wchar_t jsonPath[1024];
			wcsncpy(jsonPath, FlagFilePathConfig, len - 4);
			jsonPath[len - 4] = L'\0';
			wcscat(jsonPath, L".json\0");
			wcscpy_s(fileConfigPath, jsonPath);
			addDebugText(L"Reading flag config: \"%ws\"", fileConfigPath);
		}
		else if (len >= 5 && wcsncmp(fileConfigPath + len - 5, L".json", 5) == 0) {
			addDebugText(L"Reading flag config: \"%ws\"", fileConfigPath);
		}
		else {
			addDebugText(L"invalid ini path \"%ws\" continuing..", fileConfigPath);
		}
		err = _wfopen_s(&fileConfig, fileConfigPath, L"rb");
	}
	else {
		do {
			wchar_t* checkFilePath = H2ProcessFilePath;
			if (H2Config_isConfigFileAppDataLocal) {
				checkFilePath = local;
			}

			swprintf(fileConfigPath, ARRAYSIZE(fileConfigPath), H2ConfigJsonFilenames[H2IsDediServer], checkFilePath, readInstanceIdFile);
			addDebugText(L"Reading config: \"%ws\"", fileConfigPath);
			err = _wfopen_s(&fileConfig, fileConfigPath, L"rb");

			if (err) {
				addDebugText("H2Configuration file does not exist, error code: 0x%x", err);
			}
			H2Config_isConfigFileAppDataLocal = !H2Config_isConfigFileAppDataLocal;
			if (err && !H2Config_isConfigFileAppDataLocal) {
				--readInstanceIdFile;
			}
		} while (err && readInstanceIdFile > 0);
		H2Config_isConfigFileAppDataLocal = !H2Config_isConfigFileAppDataLocal;
	}

	if (err) {
		addDebugText("ERROR: No H2Configuration files could be found!");
		CMForce_Update = true;
		H2Config_isConfigFileAppDataLocal = true;
	}
	else {
		ownsConfigFile = (readInstanceIdFile == _Shell::GetInstanceId());

		if (!H2IsDediServer) {
			extern int current_language_main;
			extern int current_language_sub;
			cartographer_settings.language_code.code_main = current_language_main;
			cartographer_settings.language_code.code_variant = current_language_sub;
		}
		fclose(fileConfig);

		easy_json_struct json(fileConfigPath, &cartographer_settings);

		auto rc = json.load();
		if (rc < 0)
		{
			addDebugText("json.load() failed with error: %d while trying to read configuration file!", (int)rc);
		}
		else
		{
			addDebugText("H2Configuration loaded");
		}

		if (!ownsConfigFile) {
			if (cartographer_settings.base_port < 64000 + 1)
				cartographer_settings.base_port += 1000;
			else if (cartographer_settings.base_port < 65535 - 10 + 1)
				cartographer_settings.base_port += 10;
		}
	}

	addDebugText("End reading H2Configuration file.");
}



void UpgradeConfig()
{
	///////////////////////////////////////////////////////
	//THIS DOES NOT NEED TO MODIFIED AFTER UPDATE X.X.X.X//
	///////////////////////////////////////////////////////
	addDebugText("Checking upgrade status of H2Configuration file...");

	int readInstanceIdFile = _Shell::GetInstanceId();
	wchar_t local[1024];
	wcscpy_s(local, ARRAYSIZE(local), H2AppDataLocal);
	H2Config_isConfigFileAppDataLocal = false;

	errno_t err = 0;
	FILE* fileConfig = nullptr;
	wchar_t fileConfigPath[1024];
	wchar_t jsonPath[1024];

	if (FlagFilePathConfig) {
		wcscpy_s(fileConfigPath, ARRAYSIZE(fileConfigPath), FlagFilePathConfig);
		size_t len = wcslen(fileConfigPath);
		if (len >= 4 && wcsncmp(fileConfigPath + len - 4, L".ini", 4) == 0) {
			wcsncpy(jsonPath, fileConfigPath, len - 4);
			wcscat(jsonPath, L".json");
			addDebugText(L"Reading flag config: \"%ws\"", fileConfigPath);
		}
		else {
			addDebugText(L"invalid ini path \"%ws\" continuing..", fileConfigPath);
		}
		err = _wfopen_s(&fileConfig, fileConfigPath, L"rb");

	}
	else {
		do {
			wchar_t* checkFilePath = H2ProcessFilePath;
			if (H2Config_isConfigFileAppDataLocal) {
				checkFilePath = local;
			}
			swprintf(fileConfigPath, ARRAYSIZE(fileConfigPath), H2ConfigFilenames[H2IsDediServer], checkFilePath, readInstanceIdFile);
			swprintf(jsonPath, ARRAYSIZE(jsonPath), H2ConfigJsonFilenames[H2IsDediServer], checkFilePath, readInstanceIdFile);
			addDebugText(L"Reading config: \"%ws\"", fileConfigPath);
			err = _wfopen_s(&fileConfig, fileConfigPath, L"rb");

			if (err) {
				addDebugText("H2Configuration file does not exist, error code: 0x%x", err);
			}
			H2Config_isConfigFileAppDataLocal = !H2Config_isConfigFileAppDataLocal;
			if (err && !H2Config_isConfigFileAppDataLocal) {
				--readInstanceIdFile;
			}
		} while (err && readInstanceIdFile > 0);
		H2Config_isConfigFileAppDataLocal = !H2Config_isConfigFileAppDataLocal;
	}
	if (err) {
		addDebugText("INFO: No old H2Configuration file could be located to upgrade.");
	}
	else
	{
		ownsConfigFile = (readInstanceIdFile == _Shell::GetInstanceId());

		if (!H2IsDediServer) {
			extern int current_language_main;
			extern int current_language_sub;
			cartographer_settings.language_code.code_main = current_language_main;
			cartographer_settings.language_code.code_variant = current_language_sub;
		}

		CSimpleIniA ini;
		ini.SetUnicode();
		SI_Error rc = ini.LoadFile(fileConfig);
		if (rc < 0)
		{
			addDebugText("ini.LoadFile() failed with error: %d while trying to read configuration file!", (int)rc);
		}
		else
		{
			bool upgrade_check = ini.GetBoolValue(H2ConfigVersionSection.c_str(), "json_upgraded", false);
			if (!upgrade_check)
			{
				addDebugText("ini file has not been upgraded.. upgrading now");
				easy_json_struct json(jsonPath, &cartographer_settings);

				json["cartographer"].set("h2portable", ini.GetBoolValue(H2ConfigVersionSection.c_str(), "h2portable", false));
				json["cartographer"].set("base_port", ini.GetLongValue(H2ConfigVersionSection.c_str(), "base_port", 2000));
				json["cartographer"].set("upnp", ini.GetBoolValue(H2ConfigVersionSection.c_str(), "upnp", true));
				json["cartographer"].set("debug_log", ini.GetBoolValue(H2ConfigVersionSection.c_str(), "debug_log", false));
				json["cartographer"].set("debug_log_level", ini.GetLongValue(H2ConfigVersionSection.c_str(), "debug_log_level", cartographer_settings.debug_log_level));
				json["cartographer"].set("debug_log_console", ini.GetBoolValue(H2ConfigVersionSection.c_str(), "debug_log_console", cartographer_settings.debug_log_console));
				json["cartographer"].set("discord_enable", ini.GetBoolValue(H2ConfigVersionSection.c_str(), "discord_enable", cartographer_settings.discord_enable));
				json["cartographer"].set("language_label_capture", ini.GetBoolValue(H2ConfigVersionSection.c_str(), "language_label_capture", cartographer_settings.language_label_capture));
				json["cartographer"].set("enable_xdelay", ini.GetBoolValue(H2ConfigVersionSection.c_str(), "enable_xdelay", cartographer_settings.enable_xdelay));
				json["cartographer"].set("language_code", ini.GetValue(H2ConfigVersionSection.c_str(), "language_code", "-1x0"));

				json["game"].set("skip_intro", ini.GetBoolValue(H2ConfigVersionSection.c_str(), "skip_intro", cartographer_settings.game.skip_intro));
				json["game"].set("melee_fix", ini.GetBoolValue(H2ConfigVersionSection.c_str(), "melee_fix", cartographer_settings.game.melee_fix));
				json["game"].set("no_events", ini.GetBoolValue(H2ConfigVersionSection.c_str(), "no_events", cartographer_settings.game.no_events));
				json["game"].set("skeleton_biped", ini.GetBoolValue(H2ConfigVersionSection.c_str(), "skeleton_biped", cartographer_settings.game.skeleton_biped));

				json["game"]["video"].set("fps_limit", ini.GetLongValue(H2ConfigVersionSection.c_str(), "fps_limit", cartographer_settings.game.video.fps_limit));
				json["game"]["video"].set("static_lod_scale", ini.GetLongValue(H2ConfigVersionSection.c_str(), "static_lod_state", cartographer_settings.game.video.static_lod_scale));
				json["game"]["video"].set("field_of_view", ini.GetLongValue(H2ConfigVersionSection.c_str(), "field_of_view", cartographer_settings.game.video.field_of_view));
				json["game"]["video"].set("vehicle_field_of_view", ini.GetLongValue(H2ConfigVersionSection.c_str(), "vehicle_field_of_view", cartographer_settings.game.video.vehicle_field_of_view));
				json["game"]["video"].set("static_fp_fov", ini.GetBoolValue(H2ConfigVersionSection.c_str(), "static_fp_fov", false));
				json["game"]["video"].set("experimental_rendering", std::stoi(ini.GetValue(H2ConfigVersionSection.c_str(), "experimental_rendering", "0")));
				json["game"]["video"].set("refresh_rate", ini.GetLongValue(H2ConfigVersionSection.c_str(), "refresh_rate", cartographer_settings.game.video.refresh_rate));
				json["game"]["video"].set("shader_lod_max", ini.GetBoolValue(H2ConfigVersionSection.c_str(), "shader_lod_max", cartographer_settings.game.video.shader_lod_max));
				json["game"]["video"].set("light_suppressor", ini.GetBoolValue(H2ConfigVersionSection.c_str(), "light_suppressor", cartographer_settings.game.video.light_suppressor));
				json["game"]["video"].set("d3dex", ini.GetBoolValue(H2ConfigVersionSection.c_str(), "d3dex", cartographer_settings.game.video.d3dex));
				json["game"]["video"].set("override_shadows", std::stoi(ini.GetValue(H2ConfigVersionSection.c_str(), "override_shadows", "1")));
				json["game"]["video"].set("override_water", std::stoi(ini.GetValue(H2ConfigVersionSection.c_str(), "override_water", "1")));

				std::string crosshair_offset_str(ini.GetValue(H2ConfigVersionSection.c_str(), "crosshair_offset", "NaN"));
				if (crosshair_offset_str != "NaN")
					json["game"]["hud"].set("crosshair_offset", std::stof(crosshair_offset_str));
				else
					json["game"]["hud"].set("crosshair_offset", 0.138f);

				std::string crosshair_scale_str(ini.GetValue(H2ConfigVersionSection.c_str(), "crosshair_scale", "NaN"));
				if (crosshair_scale_str != "NaN")
					json["game"]["hud"].set("crosshair_scale", std::stof(crosshair_scale_str));
				else
					json["game"]["hud"].set("crosshair_scale", 1.0f);

				json["game"]["hud"].set("hide_ingame_chat", ini.GetBoolValue(H2ConfigVersionSection.c_str(), "hide_ingame_chat", cartographer_settings.game.hud.hide_ingame_chat));

				std::string raw_mouse_scale_str(ini.GetValue(H2ConfigVersionSection.c_str(), "mouse_raw_scale", "25"));
				std::string mouse_sens_str(ini.GetValue(H2ConfigVersionSection.c_str(), "mouse_sens", "0"));
				json["game"]["input"].set("raw_mouse_input", ini.GetBoolValue(H2ConfigVersionSection.c_str(), "raw_mouse_input", cartographer_settings.game.input.raw_mouse_input));
				json["game"]["input"].set("mouse_raw_scale", std::stof(raw_mouse_scale_str));
				json["game"]["input"].set("mouse_uniform_sens", ini.GetBoolValue(H2ConfigVersionSection.c_str(), "mouse_uniform_sens", cartographer_settings.game.input.mouse_uniform_sens));
				json["game"]["input"].set("mouse_sensitivity", std::stof(mouse_sens_str));
				json["game"]["input"].set("disable_ingame_keyboard", ini.GetBoolValue(H2ConfigVersionSection.c_str(), "disable_ingame_keyboard", cartographer_settings.game.input.disable_ingame_keyboard));
				json["game"]["input"].set("hotkey_help", ini.GetLongValue(H2ConfigVersionSection.c_str(), "hotkey_help", cartographer_settings.game.input.hotkey_help));
				json["game"]["input"].set("hotkey_align_window", ini.GetLongValue(H2ConfigVersionSection.c_str(), "hotkey_align_window", cartographer_settings.game.input.hotkey_align_window));
				json["game"]["input"].set("hotkey_window_mode", ini.GetLongValue(H2ConfigVersionSection.c_str(), "hotkey_window_mode", cartographer_settings.game.input.hotkey_window_mode));
				json["game"]["input"].set("hotkey_hide_ingame_chat", ini.GetLongValue(H2ConfigVersionSection.c_str(), "hotkey_hide_ingame_chat", cartographer_settings.game.input.hotkey_hide_ingame_chat));
				json["game"]["input"].set("hotkey_guide", ini.GetLongValue(H2ConfigVersionSection.c_str(), "hotkey_guide", cartographer_settings.game.input.hotkey_guide));
				json["game"]["input"].set("hotkey_console", ini.GetLongValue(H2ConfigVersionSection.c_str(), "hotkey_console", cartographer_settings.game.input.hotkey_console));

				std::string controller_sens_str(ini.GetValue(H2ConfigVersionSection.c_str(), "controller_sens", "0"));
				std::string deadzone_axial_x(ini.GetValue(H2ConfigVersionSection.c_str(), "deadzone_axial_x", "26.518"));
				std::string deadzone_axial_y(ini.GetValue(H2ConfigVersionSection.c_str(), "deadzone_axial_y", "26.518"));
				std::string deadzone_radial(ini.GetValue(H2ConfigVersionSection.c_str(), "deadzone_radial", "26.518"));
				json["game"]["input"].set("controller_sens", std::stof(controller_sens_str));
				json["game"]["input"].set("controller_modern", ini.GetBoolValue(H2ConfigVersionSection.c_str(), "controller_modern", cartographer_settings.game.input.controller_modern));
				json["game"]["input"].set("deadzone_type", std::stoi(ini.GetValue(H2ConfigVersionSection.c_str(), "deadzone_type", "0")));
				json["game"]["input"].set("deadzone_axial_x", std::stof(deadzone_axial_x));
				json["game"]["input"].set("deadzone_axial_y", std::stof(deadzone_axial_y));
				json["game"]["input"].set("deadzone_radial", std::stof(deadzone_radial));
				json["game"]["input"].set("controller_layout", std::string(ini.GetValue(H2ConfigVersionSection.c_str(), "controller_layout", "1-2-4-8-16-32-64-128-256-512-4096-8192-16384-32768")));

				const char* server_name = ini.GetValue(H2ConfigVersionSection.c_str(), "server_name", cartographer_settings.server.server_name);
				const char* server_playlist = ini.GetValue(H2ConfigVersionSection.c_str(), "server_playlist", cartographer_settings.server.playlist);
				const char* login_identifier = ini.GetValue(H2ConfigVersionSection.c_str(), "login_identifier", cartographer_settings.server.login_identifier);
				const char* login_password = ini.GetValue(H2ConfigVersionSection.c_str(), "login_password", cartographer_settings.server.login_password);
				std::string team_bit_mask(ini.GetValue(H2ConfigVersionSection.c_str(), "teams_enabled_bit_flags", cartographer_settings.server.enabled_teams_bit_str));

				json["server"].set("server_name", server_name);
				json["server"].set("server_playlist", server_playlist);
				json["server"].set("additional_pcr_time", ini.GetLongValue(H2ConfigVersionSection.c_str(), "additional_pcr_time", cartographer_settings.server.additional_pcr_time));
				json["server"].set("minimum_player_start", ini.GetLongValue(H2ConfigVersionSection.c_str(), "minimum_player_start", cartographer_settings.server.minimum_player_start));
				json["server"].set("vip_lock", ini.GetBoolValue(H2ConfigVersionSection.c_str(), "vip_lock", cartographer_settings.server.vip_lock));
				json["server"].set("shuffle_even_teams", ini.GetBoolValue(H2ConfigVersionSection.c_str(), "shuffle_even_teams", cartographer_settings.server.shuffle_even_teams));
				json["server"].set("login_identifier", login_identifier);
				json["server"].set("login_password", login_password);
				json["server"].set("teams_enabled_bit_flags", team_bit_mask);
				json["server"].set("enable_anti_cheat", ini.GetBoolValue(H2ConfigVersionSection.c_str(), "enable_anti_cheat", cartographer_settings.server.enable_anti_cheat));

#ifndef NDEBUG
				json["development"].set("forced_event", ini.GetLongValue(H2ConfigVersionSection.c_str(), "forced_event", cartographer_settings.development.forced_event));
#endif
				json.save();

				//ini.SetBoolValue(H2ConfigVersionSection.c_str(), "json_upgraded", true);
				//ini.SaveFile(fileConfig);
				fclose(fileConfig);
				_wfopen_s(&fileConfig, fileConfigPath, L"a+b");
				fputs("json_upgraded = true\n", fileConfig);
				fclose(fileConfig);
				addDebugText("ini file has been upgraded!");
			}
			else
			{
				addDebugText("ini file has already been upgraded.");
			}
		}
	}

}

#pragma region Config Init/Deinit
void InitH2Config() {
	cartographer_settings.game.input.disable_ingame_keyboard = _Shell::GetInstanceId() > 1 ? true : false;
	UpgradeConfig();
	ReadH2Config();
}
void DeinitH2Config() {
	SaveH2Config();
}
#pragma endregion
