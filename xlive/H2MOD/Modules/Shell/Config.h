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
