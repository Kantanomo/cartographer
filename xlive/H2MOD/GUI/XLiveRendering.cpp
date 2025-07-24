#include "stdafx.h"
#include "XLiveRendering.h"

#include "imgui.h"

#include "backends/imgui_impl_dx9.h"
#include "rasterizer/dx9/rasterizer_dx9_main.h"

#include "H2MOD/GUI/ImGui_Integration/ImGui_Handler.h"
#include "H2MOD/Modules/Accounts/AccountLogin.h"
#include "H2MOD/Modules/Input/KeyboardInput.h"


extern void initialize_instance();
extern LRESULT IMGUI_IMPL_API ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

bool doDrawIMGUI = false;

HWND H2hWnd;
D3DPRESENT_PARAMETERS g_d3dPresentParameters;

int verticalRes = 0;
int horizontalRes = 0;

void XLiveRendering::InitializeD3D9(D3DPRESENT_PARAMETERS* presentParameters)
{
	g_d3dPresentParameters = *presentParameters;

	ImGuiHandler::Initalize(presentParameters->hDeviceWindow);
}

void XLiveRendering::D3D9ReleaseResources()
{
	ImGuiHandler::release_motd_texture();
	ImGui_ImplDX9_InvalidateDeviceObjects();
}

// #5297: XLiveInitializeEx
int WINAPI XLiveInitializeEx(XLIVE_INITIALIZE_INFO* pXii, DWORD dwVersion)
{
	LOG_TRACE_XLIVE("XLiveInitializeEx()");

	initialize_instance();

	if (pXii->pD3D) {
		XLiveRendering::InitializeD3D9((D3DPRESENT_PARAMETERS*)pXii->pD3DPP);
	}

	UpdateMasterStatus(-1, "Status: Initializing");

	LOG_TRACE_XLIVE("XLiveInitializeEx() - dwVersion = {0:x}", dwVersion);
	return 0;
}

// #5000: XLiveInitialize
HRESULT WINAPI XLiveInitialize(XLIVE_INITIALIZE_INFO* pXii)
{
	return XLiveInitializeEx(pXii, 0);
}

// #5003: XLiveUninitialize
int WINAPI XLiveUninitialize()
{
	LOG_TRACE_XLIVE("XLiveUninitialize");
	
	XLiveRendering::D3D9ReleaseResources();

	return 0;
}

// #5005: XLiveOnCreateDevice
int WINAPI XLiveOnCreateDevice(IUnknown* pD3D, VOID* vD3DPP)
{	
	//LOG_TRACE_XLIVE("XLiveOnCreateDevice  (pD3D = %X, pD3DPP = %X)", pD3D, vD3DPP);
	return 0;
}

// #5007: XLiveOnResetDevice
int WINAPI XLiveOnResetDevice(D3DPRESENT_PARAMETERS* pD3DPP)
{
	g_d3dPresentParameters = *pD3DPP;

	//Have to invalidate ImGUI on device reset, otherwise it hangs the device in a reset loop.
	//https://github.com/ocornut/imgui/issues/1464#issuecomment-347469716

	ImGuiHandler::release_motd_texture();
	ImGui_ImplDX9_InvalidateDeviceObjects();
	//LOG_TRACE_XLIVE("XLiveOnResetDevice");
	return 0;
}

// #5006 XLiveOnDestroyDevice
HRESULT WINAPI XLiveOnDestroyDevice()
{
	ImGuiHandler::release_motd_texture();
	ImGui_ImplDX9_InvalidateDeviceObjects();
	XLiveRendering::D3D9ReleaseResources();
	
	//LOG_TRACE_XLIVE("XLiveOnDestroyDevice");
	return S_OK;
}

int achievement_height = 0;
bool achievement_freeze = false;
int achievement_timer = 0;

char* autoUpdateText = 0;

// #5001
int WINAPI XLiveInput(XLIVE_INPUT_INFO* pPii)
{
	static bool has_initialised_input = false;
	if (!has_initialised_input) {
		extern RECT rectScreenOriginal;
		H2hWnd = pPii->hWnd;
		GetWindowRect(H2hWnd, &rectScreenOriginal);
		has_initialised_input = true;
	}

	if ((pPii->uMSG == WM_KEYDOWN || pPii->uMSG == WM_SYSKEYDOWN)
		&& (GetKeyState(pPii->wParam) & 0x8000))
	{
		// hotkeys
		KeyboardInput::ExecuteHotkey(pPii->wParam);
	}
	
	return S_OK;
}

// #5030: XLivePreTranslateMessage
BOOL WINAPI XLivePreTranslateMessage(const LPMSG lpMsg)
{
	return false;
}

// #5002: XLiveRender
HRESULT WINAPI XLiveRender()
{
	static std::mutex renderMtx;
	std::lock_guard lg(renderMtx);

	IDirect3DDevice9Ex* device = rasterizer_dx9_device_get_interface();

	if (!device) {
		return E_UNEXPECTED;
	}

	if (FAILED(device->TestCooperativeLevel())) {
		return E_UNEXPECTED;
	}

	ImGuiHandler::DrawImgui();
	return S_OK;
}

