#pragma once

/* constants */

#define XONLINE_E_STORAGE_FILE_NOT_FOUND 0x8015C004

/* enums */

typedef enum _XSTORAGE_FACILITY
{
	XSTORAGE_FACILITY_GAME_CLIP = 1,
	XSTORAGE_FACILITY_PER_TITLE = 2,
	XSTORAGE_FACILITY_PER_USER_TITLE = 3
} XSTORAGE_FACILITY;

/* structures */

#pragma pack(push, 1)
typedef struct _XSTORAGE_DOWNLOAD_TO_MEMORY_RESULTS {
	DWORD dwBytesTotal;
	XUID xuidOwner;
	FILETIME ftCreated;
} XSTORAGE_DOWNLOAD_TO_MEMORY_RESULTS;
#pragma pack(pop)

/* prototypes */

DWORD WINAPI XStorageBuildServerPath(
	DWORD dwUserIndex,
	XSTORAGE_FACILITY StorageFacility,
	const void* pvStorageFacilityInfo,
	DWORD dwStorageFacilityInfoSize,
	WCHAR* pwszItemName,
	WCHAR* pwszServerPath,
	DWORD* pdwServerPathLength);

DWORD WINAPI XStorageDownloadToMemory(
	DWORD dwUserIndex,
	const WCHAR* wszServerPath,
	DWORD dwBufferSize,
	const BYTE* pbBuffer,
	DWORD cbResults,
	XSTORAGE_DOWNLOAD_TO_MEMORY_RESULTS* pResults,
	XOVERLAPPED* pXOverlapped);

DWORD WINAPI XStorageDelete(DWORD dwUserIndex, const WCHAR* wszServerPath, XOVERLAPPED* pXOverlapped);
