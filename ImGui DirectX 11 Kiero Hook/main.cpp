#include "includes.h"
#include "attributes.h"
#include "offsets.h"
#include "types.h"
#include "process.h"
#include "string.h"
#include "matrix.h"
#include "IniWriter.h"
#include "IniReader.h"
#include "XorStr.hpp"
#include "global.h"
#include <iostream>
#include <winuser.h>
#include <thread>
#include <vector>
#include <string>
#include <Windows.h>
#include <tlhelp32.h>
#include <psapi.h>
#include "Snowdrop.h"
#include "Main.h"
#include "VisualManager.h"
#include "ConfigManager.h"
#pragma comment(lib, "psapi.lib")


// Credential goes to https://www.codeproject.com/Articles/10809/A-Small-Class-to-Read-INI-File
// IniWriter and IniReader by Xiangxiong Jian
// licensed under The Code Project Open License (CPOL)

LPVOID lpRemain = nullptr;

extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

Present oPresent;
HWND window = NULL;
WNDPROC oWndProc;
ID3D11Device* pDevice = NULL;
ID3D11DeviceContext* pContext = NULL;
ID3D11RenderTargetView* mainRenderTargetView;

static float res_x, res_y;

uintptr_t g_pBase;
Main* g_mainHandle = nullptr;

static bool init = false;
static bool game_is_initialized = false;

static int MENU_KEY;
static bool show_menu = true;
bool menu_key_pressed = false;
constexpr int GUI_COLUMN_OFFSET = 200;



bool UseFOV;
int FovAmount;
bool UseFOVZoom;
int ZoomSpeed;
int ZoomFovAmount;
bool useFirstPerson;


// read_memory: Reads a value of type T from the specified memory address.
// - Returns the value at the address if successful.
// - If an exception occurs (e.g., invalid address), returns a default-constructed T.
template<typename T> T read_memory(uintptr_t address)
{
	try { return *(T*)address; }
	catch (...) {
		return T();
	}
}

// write_memory: Writes a value of type T to the specified memory address.
// - Attempts to assign the value at the address.
// - Silently fails if an exception occurs (e.g., invalid address).
template<typename T> void write_memory(uintptr_t address, T value)
{
	try { *(T*)address = value; }
	catch (...) { return; }
}

// InitImGui: Sets up ImGui for rendering and input handling.
// - Creates a new ImGui context.
// - Configures ImGui settings (prevents mouse cursor changes).
// - Initializes ImGui for Win32 input and DirectX 11 rendering.
void InitImGui()
{
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags = ImGuiConfigFlags_NoMouseCursorChange;
	ImGui_ImplWin32_Init(window);
	ImGui_ImplDX11_Init(pDevice, pContext);
}

// WndProc: Custom window procedure for handling Windows messages.
// - Passes messages to ImGui's Win32 handler first to process UI input.
// - If ImGui handles the message, returns true to prevent further processing.
// - Otherwise, forwards the message to the original window procedure.
LRESULT __stdcall WndProc(const HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {

	if (true && ImGui_ImplWin32_WndProcHandler(hWnd, uMsg, wParam, lParam))
		return true;

	return CallWindowProc(oWndProc, hWnd, uMsg, wParam, lParam);
}


// Update: Per-frame update function, called every frame.
// Checks if the game is initialized and if the game window is in the foreground.
// Early exits if initialization hasn't completed or the window is not active.
void Update(void) {

	if (game_is_initialized == false) return;
	if (GetForegroundWindow() != FindWindowA(NULL, "Tom Clancy's The Division")) return;
	
	// Use this for anything u need to run on next frame / Update.
	g_mainHandle->GetCameraManager()->Update();
}


// hkPresent: Custom DirectX 11 Present hook for rendering ImGui menus.
// - Initializes DirectX device, context, backbuffer, and ImGui on first call.
// - Handles menu toggle with a hotkey and renders the ImGui interface.
// - Calls the original Present function at the end to display frames.
HRESULT __stdcall hkPresent(IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags)
{
	if (!init)
	{

		if (SUCCEEDED(pSwapChain->GetDevice(__uuidof(ID3D11Device), (void**)&pDevice)))
		{
			pDevice->GetImmediateContext(&pContext);
			DXGI_SWAP_CHAIN_DESC sd;
			pSwapChain->GetDesc(&sd);
			window = sd.OutputWindow;
			ID3D11Texture2D* pBackBuffer;
			pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&pBackBuffer);
			pDevice->CreateRenderTargetView(pBackBuffer, NULL, &mainRenderTargetView);
			pBackBuffer->Release();
			oWndProc = (WNDPROC)SetWindowLongPtr(window, GWLP_WNDPROC, (LONG_PTR)WndProc);
			InitImGui();

			g_mainHandle = new Main();
			g_mainHandle->Initialize();
			std::cout << "running g_MainHandle Initialize!\n";
			init = true;
		}

		else
			return oPresent(pSwapChain, SyncInterval, Flags);

	}


	if (GetAsyncKeyState(VK_HOME) < 0 && !menu_key_pressed) {
		show_menu = !show_menu;
		menu_key_pressed = true;

	}
	if (GetAsyncKeyState(VK_HOME) == 0 && menu_key_pressed) {
		menu_key_pressed = false;
		TD::ShowMouse(false);
	}


	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	if (show_menu) {

		ImGui::Begin(xor ("ssh's QOL Tools"), nullptr);

		ImGui::Columns(2);
		ImGui::SetColumnOffset(1, GUI_COLUMN_OFFSET);
		static int tab = 0;


		ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 204, 51, 255));
		char* text = (char*) xor ("Game is Initialized");
		ImGui::SetCursorPosX(((float)GUI_COLUMN_OFFSET - ImGui::CalcTextSize(text).x) * 0.5f);
		ImGui::Text(text);
		ImGui::PopStyleColor();

		ImGui::Spacing();
		if (ImGui::Button(xor ("Camera"), ImVec2(GUI_COLUMN_OFFSET - 10, 20)))
		{
			tab = 1;
		}
		if (ImGui::Button(xor ("World"), ImVec2(GUI_COLUMN_OFFSET - 10, 20)))
		{
			tab = 2;
		}
		if (ImGui::Button(xor ("Settings"), ImVec2(GUI_COLUMN_OFFSET - 10, 20)))
		{
			tab = 999;
		}



		// float res_x = , res_y = ImGui::GetIO().DisplaySize.y;
		// ImGui::Text("rpm %x, recoil %x, spread %x", g_indices[RPM], g_indices[RecoilBase], g_indices[SpreadMax]);

		ImGui::NextColumn();
		{
			if (game_is_initialized) {
				switch (tab) {
				case 1:
					g_mainHandle->GetCameraManager()->DrawUI();
				break;
				case 2:
					g_mainHandle->GetVisualManager()->DrawUI();
				break;
				case 999:
					g_mainHandle->GetConfigManager()->DrawUI();
				break;
				default:
					ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 204, 51, 255));
					char* text = (char*)"Click the buttons on the left for the menus. Happy Playing! :)";
					ImGui::SetCursorPosX((ImGui::GetWindowSize().x - ImGui::CalcTextSize(text).x + GUI_COLUMN_OFFSET) * 0.5f);
					ImGui::Text(text);
					ImGui::PopStyleColor();
					break;
				}
			}
		}

		TD::ShowMouse(true);
		ImGui::End();
	}

	try {
		Update();
	}
	catch (...) {}

	ImGui::Render();
	pContext->OMSetRenderTargets(1, &mainRenderTargetView, NULL);
	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

	return oPresent(pSwapChain, SyncInterval, Flags);
}

Main::Main()
{

}

Main::~Main()
{

}

void Main::Initialize()
{
	// Open the process.
	g_pBase = (uintptr_t)GetModuleHandle(0);
	// Get the base pointer for the module.

	// Load Config
	LoadSettings();

	//Load Hooks
	util::hooks::Init();

	res_x = ImGui::GetIO().DisplaySize.x;
	res_y = ImGui::GetIO().DisplaySize.y;
	game_is_initialized = true;

	m_pCameraManager = std::make_unique<CameraManager>();
	std::cout << "Created the m_pCameraManager!\n";
	m_pVisualManager = std::make_unique<VisualManager>();
	std::cout << "Created the m_pVisualManager!\n";
}


// MainThread: Initializes a DirectX 11 hook using the kiero library.
// Repeatedly attempts to initialize kiero until successful.
// Once initialized, binds the custom Present function (hkPresent) to the DirectX 11 Present call.
// Returns TRUE when the hook is successfully set.
DWORD WINAPI MainThread(LPVOID lpReserved)
{
	AllocConsole();
	FILE* f;
	freopen_s(&f, "CONOUT$", "w", stdout);
	std::cout << "Debug information starts:\n";

	SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);


	bool init_hook = false;
	do
	{
		if (kiero::init(kiero::RenderType::D3D11) == kiero::Status::Success)
		{
			kiero::bind(8, (void**)&oPresent, hkPresent);
			init_hook = true;
		}
	} while (!init_hook);
	return TRUE;
}


// DllMain: Entry point for the DLL.
// Handles DLL load/unload events.
// - On load (DLL_PROCESS_ATTACH): disables thread notifications and starts MainThread.
// - On unload (DLL_PROCESS_DETACH): shuts down kiero library.
BOOL WINAPI DllMain(HMODULE hMod, DWORD dwReason, LPVOID lpReserved)
{
	switch (dwReason)
	{
	case DLL_PROCESS_ATTACH:
		DisableThreadLibraryCalls(hMod);
		CreateThread(nullptr, 0, MainThread, hMod, 0, nullptr);
		break;
	case DLL_PROCESS_DETACH:
		kiero::shutdown();
		break;
	}
	return TRUE;
}

