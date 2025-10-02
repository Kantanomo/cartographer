#pragma once

void InitGSDownload();
void DeinitGSDownload();

void GSDownloadInit();
void GSDownloadCheck();
void GSDownloadDL();
void GSDownloadInstall();
void GSDownloadCancel();

extern char* g_auto_update_text;

extern bool updater_has_files_to_download;
extern bool updater_has_files_to_install;
extern bool updater_has_up_to_date;
extern long long sizeOfDownload;
extern long long sizeOfDownloaded;
