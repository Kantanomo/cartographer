#pragma once

void InitH2Config();
void DeinitH2Config();
void SaveH2Config();
void ReadH2Config();
void GetH2ConfigFolder(wchar_t* path_out);
void UpgradeConfig();

#ifndef _CARTOGRAPHER_DLL_CONF
// temporary config files 
// for testing purposes
#define USE_DEV_PREVIEW_CONFIG_FILE_PATHS 1
#endif
