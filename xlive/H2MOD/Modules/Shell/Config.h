#pragma once

#include "H2MOD.h"
#include "H2MOD/Modules/Input/ControllerInput.h"

void InitH2Config();
void DeinitH2Config();
void SaveH2Config();
void ReadH2Config();
void UpgradeConfig();

#ifndef _CARTOGRAPHER_DLL_CONF
// temporary config files 
// for testing purposes
#define USE_DEV_PREVIEW_CONFIG_FILE_PATHS 1
#endif

struct _H2Config_language {
	int code_main;
	int code_variant;
};


extern std::string cartographerURL;
extern std::string cartographerMapRepoURL;

extern bool H2Config_isConfigFileAppDataLocal;

extern int H2Config_field_of_view;
extern int H2Config_vehicle_field_of_view;
extern bool H2Config_static_first_person;
extern float H2Config_mouse_sens;
extern bool H2Config_mouse_uniform;
extern float H2Config_controller_sens;
extern bool H2Config_controller_modern;
extern float H2Config_Deadzone_A_X;
extern float H2Config_Deadzone_A_Y;
extern float H2Config_Deadzone_Radial;
extern __int16 H2Config_refresh_rate;
extern bool H2Config_shader_lod_max;
extern bool H2Config_light_suppressor;
extern bool H2Config_d3d9ex;
extern float H2Config_crosshair_offset;
extern bool H2Config_disable_ingame_keyboard;
extern bool H2Config_hide_ingame_chat;
extern bool H2Config_xDelay;
extern bool H2Config_voice_chat;
extern char H2Config_dedi_server_name[XUSER_NAME_SIZE];
extern char H2Config_dedi_server_playlist[256];
extern int H2Config_additional_pcr_time;
extern bool H2Config_debug_log;
extern int H2Config_debug_log_level;
extern bool H2Config_debug_log_console;
extern char H2Config_login_identifier[255];
extern char H2Config_login_password[255];
extern short H2Config_team_bit_flags;
extern bool H2Config_team_flag_array[8];
extern char H2Config_stats_authkey[32 + 1];
extern bool H2Config_vip_lock;
extern bool H2Config_even_shuffle_teams;
extern bool H2Config_koth_random;
extern bool H2Config_anti_cheat_enabled;

extern int H2Config_hotkeyIdHelp;
extern int H2Config_hotkeyIdAlignWindow;
extern int H2Config_hotkeyIdWindowMode;
extern int H2Config_hotkeyIdToggleHideIngameChat;
extern int H2Config_hotkeyIdGuide;
extern int H2Config_hotkeyIdConsole;
extern int H2Config_minimum_player_start;

extern float H2Config_raw_mouse_scale;
extern float H2Config_crosshair_scale;
extern ControllerInput::CustomControllerLayout H2Config_CustomLayout;

extern bool H2Config_upnp_enable;
extern bool H2Config_melee_fix;
extern bool H2Config_no_events;
extern bool H2Config_spooky_boy;

#ifndef NDEBUG
extern int H2Config_forced_event;
#endif
