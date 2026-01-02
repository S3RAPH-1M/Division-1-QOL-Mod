#pragma once
#include "CameraManager.h"
#include <memory>
#include <Windows.h>
#include "VisualManager.h"
#include "ConfigManager.h"
#include "SkinnedMeshManager.h"

class Main
{
public:
	Main();
	~Main();

	void Initialize();
	void Release();

	CameraManager* GetCameraManager() { return m_pCameraManager.get(); }
	VisualManager* GetVisualManager() { return m_pVisualManager.get(); }
	SkinnedMeshManager* GetSkinnedMeshManager() { return m_pSkinnedMeshManager.get(); }
	ConfigManager* GetConfigManager() { return m_pConfigManager.get(); }

	std::unique_ptr<CameraManager> m_pCameraManager;
	std::unique_ptr<VisualManager> m_pVisualManager;
	std::unique_ptr<SkinnedMeshManager> m_pSkinnedMeshManager;
	std::unique_ptr<ConfigManager> m_pConfigManager;

	bool m_shutdown;
public:
	Main(Main const&) = delete;
	void operator=(Main const&) = delete;
};

template<typename T> T read_memory(uintptr_t address);
template<typename T> void write_memory(uintptr_t address, T value);

extern Main* g_mainHandle;
extern HINSTANCE g_dllHandle;
extern bool g_shutdown;
extern bool menu_key_pressed;
extern int FovAmount;
extern bool UseFOV;
extern bool UseFOVZoom;
extern int ZoomFovAmount;
extern int ZoomSpeed;
extern bool useFirstPerson;
extern HMODULE g_ModModule;
