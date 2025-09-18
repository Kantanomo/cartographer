#pragma once

typedef struct _XLIVE_INITIALIZE_INFO {
	UINT cbSize;
	DWORD dwFlags;
	IUnknown* pD3D;
	VOID* pD3DPP;
	LANGID langID;
	WORD wReserved1;
	PCHAR pszAdapterName;
	WORD wLivePortOverride;
	WORD wReserved2;
} XLIVE_INITIALIZE_INFO;

typedef struct XLIVE_INPUT_INFO {
	UINT cbSize;
	HWND hWnd;
	UINT uMSG;
	WPARAM wParam;
	LPARAM lParam;
	BOOL fHandled;
	LRESULT lRet;
} XLIVE_INPUT_INFO;

#define DISPLAY_DEV_TESTING_MENU 0

namespace XLiveRendering
{
	void InitializeD3D9(D3DPRESENT_PARAMETERS* presentParameters);
	void D3D9ReleaseResources();
};

extern HWND H2hWnd;

HRESULT WINAPI XLiveInitialize(XLIVE_INITIALIZE_INFO* pXii);

int WINAPI XLiveOnResetDevice(D3DPRESENT_PARAMETERS* pD3DPP);
