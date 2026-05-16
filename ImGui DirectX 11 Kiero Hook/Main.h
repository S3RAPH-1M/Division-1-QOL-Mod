#pragma once
#include "CameraManager.h"
#include <memory>
#include <Windows.h>
#include "VisualManager.h"
#include "ConfigManager.h"
#include "SkinnedMeshManager.h"
#include "CamoManager.h"
#include "HeadManager.h"
#include "UIInspector.h"
#include "AgentInspector.h"

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
	CamoManager* GetCamoManager() { return m_pCamoManager.get(); }
	ConfigManager* GetConfigManager() { return m_pConfigManager.get(); }
	HeadManager* GetHeadManager() { return m_pHeadManager.get(); }
#if QOL_ENABLE_INSPECTORS
	UIInspector* GetUIInspector() { return m_pUIInspector.get(); }
	AgentInspector* GetAgentInspector() { return m_pAgentInspector.get(); }
#endif

	std::unique_ptr<CameraManager> m_pCameraManager;
	std::unique_ptr<VisualManager> m_pVisualManager;
	std::unique_ptr<SkinnedMeshManager> m_pSkinnedMeshManager;
	std::unique_ptr<CamoManager> m_pCamoManager;
	std::unique_ptr<ConfigManager> m_pConfigManager;
	std::unique_ptr<HeadManager> m_pHeadManager;
#if QOL_ENABLE_INSPECTORS
	std::unique_ptr<UIInspector> m_pUIInspector;
	std::unique_ptr<AgentInspector> m_pAgentInspector;
#endif

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
extern bool g_InDarkZone;
extern bool g_IsPlayerRogue;
extern bool g_IsPlayerDead;
extern bool g_ForceRogueVisual;

// Pre-render rogue-visual enforcer. Defined in main.cpp, invoked from the
// camera-update vtable hook (post-original, pre-scene-render) so our write
// is the last one before the watch/antenna props are built each frame.
void ApplyForceRogueVisualTick();
