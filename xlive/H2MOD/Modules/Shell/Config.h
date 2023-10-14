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

extern char H2Config_dedi_server_name[XUSER_NAME_SIZE];
extern char H2Config_dedi_server_playlist[256];
extern int H2Config_additional_pcr_time;
extern char H2Config_login_identifier[255];
extern char H2Config_login_password[255];
extern short H2Config_team_bit_flags;
extern bool H2Config_team_flag_array[8];
extern char H2Config_stats_authkey[32 + 1];
extern bool H2Config_vip_lock;
extern bool H2Config_even_shuffle_teams;
extern bool H2Config_koth_random;
extern bool H2Config_anti_cheat_enabled;

extern int H2Config_minimum_player_start;



#ifndef NDEBUG
extern int H2Config_forced_event;
#endif
