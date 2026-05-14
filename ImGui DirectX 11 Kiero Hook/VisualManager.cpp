#include "VisualManager.h"
#include "Snowdrop.h"
#include "Util.h"
#include "imgui/imgui.h"
#include <string>

namespace SnowdropDebugUI
{
    // sub_1B72FF0: generic juice-driven UI-graph list loader.
    // Reads qword_45026D0 (juice store) → "myUILoadList" → list named by `listName`,
    // and instantiates+registers each entry into the live UI system.
    constexpr std::uintptr_t kLoadUIGraphListRVA = 0x1B72FF0;
    using LoadUIGraphList_t = std::int64_t (__fastcall *)(const char* listName);

    static LoadUIGraphList_t Get()
    {
        return reinterpret_cast<LoadUIGraphList_t>(g_pBase + kLoadUIGraphListRVA);
    }

    static const char* g_lastResult = nullptr;
    static std::int32_t g_countBefore = -1;
    static std::int32_t g_countAfter  = -1;
    static std::int32_t g_mgrCountBefore = -1;
    static std::int32_t g_mgrCountAfter  = -1;
    static std::uint8_t g_gateByteBefore = 0xFF;

    // Two candidate counters — we read both to see which actually moves.
    //   kHotReloadCountRVA : qword_3EC4F30 LO  -- sub_A64540 watch list (likely 0 in retail)
    //   kUIMgrCountOff     : qword_480ECE0 + 0x28 LO  -- count field of the live UI graph
    //                          manager. Initial read showed 0x4D (77 active graphs).
    constexpr std::uintptr_t kHotReloadCountRVA = 0x3EC4F30;
    constexpr std::uintptr_t kUIGraphMgrRVA     = 0x480ECE0;
    constexpr std::size_t    kUIMgrCountOff     = 0x28;

    static std::int32_t ReadGraphCount()
    {
        __try {
            return *reinterpret_cast<const std::int32_t*>(g_pBase + kHotReloadCountRVA);
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            return -1;
        }
    }

    static std::int32_t ReadUIMgrCount()
    {
        __try {
            void* mgr = *reinterpret_cast<void**>(g_pBase + kUIGraphMgrRVA);
            if (!mgr) return -1;
            return *reinterpret_cast<const std::int32_t*>(
                reinterpret_cast<std::uint8_t*>(mgr) + kUIMgrCountOff);
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            return -1;
        }
    }

    // POD-only SEH helper. Resolves [Client+0x90] → inner manager, returns
    // pointer to its +0x2B1 byte (the "graph-load enabled" gate that
    // sub_1B327E0 flips to 1 before calling sub_1B72FF0 at startup).
    // Returns nullptr if any step is null.
    static std::uint8_t* ResolveGateByte()
    {
        __try {
            TD::RogueClient* rc = TD::RogueClient::Singleton();
            if (!rc) return nullptr;
            TD::Client* client = rc->m_pClient;
            if (!client) return nullptr;
            void* innerMgr = *reinterpret_cast<void**>(
                reinterpret_cast<std::uint8_t*>(client) + 0x90);
            if (!innerMgr) return nullptr;
            return reinterpret_cast<std::uint8_t*>(innerMgr) + 0x2B1;
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            return nullptr;
        }
    }

    static void Load(const char* listName)
    {
        __try {
            Get()(listName);
            g_lastResult = "ok";
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            g_lastResult = "exception (juice key probably missing)";
        }
    }

    // Replicates the `-enabledebugui` startup behavior at runtime:
    //   1. snapshot the count
    //   2. flip [Client+0x90][+0x2B1] = 1 (the gate byte)
    //   3. sub_1B72FF0("myDebugGraphs")
    //   4. restore the gate byte to its previous value
    //   5. read back the count to confirm graphs actually registered
    static void ForceEnableDebugUI()
    {
        std::uint8_t* gate = ResolveGateByte();
        if (!gate) {
            g_lastResult = "ResolveGateByte failed (Client/manager not ready)";
            return;
        }

        g_countBefore    = ReadGraphCount();
        g_mgrCountBefore = ReadUIMgrCount();
        g_gateByteBefore = *gate;

        *gate = 1;
        Load("myDebugGraphs");
        *gate = g_gateByteBefore;

        g_countAfter    = ReadGraphCount();
        g_mgrCountAfter = ReadUIMgrCount();
    }
}

VisualManager::VisualManager()
{
    m_overrideTimeOfDay = false;
    m_customTimeOfDay = 1200;

    TD::EnvironmentFileSystem* pEnvFiles = TD::EnvironmentFileSystem::Singleton();
    //std::cout << std::hex << pEnvFiles << std::inline;
    m_envPresetArray = new __int64[pEnvFiles->m_handleCount];
    m_envNameArray = new const char* [pEnvFiles->m_handleCount];

    for (int i = 0; i < pEnvFiles->m_handleCount; ++i)
    {
        m_envPresetArray[i] = pEnvFiles->m_pHandles[i].pEntity;

        std::string sName(*(const char**)(pEnvFiles->m_pHandles[i].pEntity + 0x10));
        sName = sName.substr(18, sName.length() - 18 - 13);

        char* nameBuf = new char[sName.length() + 1];
        memcpy(nameBuf, sName.c_str(), sName.length() + 1);

        m_envNameArray[i] = nameBuf;
    }

    m_selectedCurrentWeather = 0;
    m_selectedNextWeather = 0;
    m_environmentCount = pEnvFiles->m_handleCount;
}

VisualManager::~VisualManager()
{

}

struct DOFStructure
{
  int enable1;
  int enable2;
  float fstop;
  float focusDistance;
  float minCoC;
  float maxCoC;
};

void VisualManager::Update()
{
    TD::EnvironmentManager* pEnvManager = TD::RogueClient::Singleton()->m_pClient->m_pWorld->m_pEnvironmentManager;

    if (m_overrideTimeOfDay)
    {
        pEnvManager->m_FreezeToD = m_overrideTimeOfDay;
        int hours = m_customTimeOfDay / 100;
        int minutes = m_customTimeOfDay - (hours * 100);

        pEnvManager->m_TimeOfDay =
            (hours * 60 + minutes) * 60 * 1000;
    }

    if (m_freezeWeatherTimer)
    {
        pEnvManager->m_RunWeatherTimer = false;
    }
}

void VisualManager::DrawUI()
{
  TD::EnvironmentManager* pEnvManager = TD::RogueClient::Singleton()->m_pClient->m_pWorld->m_pEnvironmentManager;

  ImGui::Text("Current Environment");
  ImGui::Combo("##CurrentEnvironment", &m_selectedCurrentWeather, m_envNameArray, m_environmentCount);
  ImGui::SameLine();
  if (ImGui::Button("Select Environment##1"))
    pEnvManager->SetCurrentWeather(m_envPresetArray[m_selectedCurrentWeather]);

  ImGui::Text("Blending Environment");
  ImGui::Combo("##BlendingEnvironment", &m_selectedNextWeather, m_envNameArray, m_environmentCount);
  ImGui::SameLine();
  if (ImGui::Button("Select Environment##2"))
    pEnvManager->SetNextWeather(m_envPresetArray[m_selectedNextWeather]);

  ImGui::Text("Blend factor of current and blend environments");
  ImGui::SliderFloat("##BlendFactor", &pEnvManager->m_pEnvironmentValues->m_BlendValue, 0, 1);

  ImGui::Text("Time of Day");
  if (ImGui::InputInt("##TimeOfDay", &m_customTimeOfDay, 1, 10))
  {
    int hours = m_customTimeOfDay / 100;
    int minutes = m_customTimeOfDay - (hours * 100);

    if (minutes >= 60 && minutes < 70)
    {
      hours += 1;
      minutes = 0;
    }
    else if (minutes > 70 || minutes < 0)
    {
      if (minutes < 0)
        hours = 23;
      minutes = 59;
    }

    if (hours >= 24)
      hours = 0;
    else if (hours < 0)
      hours = 23;

    m_customTimeOfDay = hours * 100 + minutes;
    if (m_overrideTimeOfDay)
      pEnvManager->m_TimeOfDay = (hours * 60 + minutes) * 60 * 1000;
  }

  ImGui::Checkbox("Override time of day", &m_overrideTimeOfDay);
  ImGui::Spacing();


  ImGui::Text("Blend Transition Time Start (ms)");
  ImGui::InputInt("##BlendTransitionTimeStart", &pEnvManager->m_WeatherTimer, 1000, 1000);
  ImGui::Text("Blend Transition Time End (ms)");
  ImGui::InputInt("##BlendTransitionTimeEnd", &pEnvManager->m_WeatherTimerMax, 1000, 1000);
  ImGui::Spacing();
  ImGui::Checkbox("Freeze Transition Timer", &m_freezeWeatherTimer);

  if(!pEnvManager->m_RunWeatherTimer)
  {
      if (ImGui::Button("Start Blend Transition"))
      {
          pEnvManager->m_RunWeatherTimer = !pEnvManager->m_RunWeatherTimer;
      }
  }
  else
  {
      if (ImGui::Button("Stop Blend Transition"))
      {
          pEnvManager->m_RunWeatherTimer = !pEnvManager->m_RunWeatherTimer;
      }
  }

  ImGui::Spacing();
  ImGui::Separator();
  if (ImGui::CollapsingHeader("Snowdrop Debug UI (experimental)"))
  {
      ImGui::TextWrapped(
          "Replicates what the engine does when launched with -enabledebugui "
          "(Uplay strips that flag, so we do it manually):\n"
          "  1. resolve RogueClient->m_pClient->[+0x90] -> inner manager\n"
          "  2. set [inner_manager + 0x2B1] = 1 (the gate byte sub_1B327E0 sets)\n"
          "  3. call sub_1B72FF0(\"myDebugGraphs\") -> sub_A64540 path\n"
          "  4. restore the gate byte\n"
          "  5. read back qword_3EC4F30 to confirm the registry grew");

      ImGui::Spacing();
      if (ImGui::Button("Force enable Snowdrop Debug UI"))
          SnowdropDebugUI::ForceEnableDebugUI();

      ImGui::Spacing();
      ImGui::Text("Hot-reload watch list (qword_3EC4F30): %d",
                  SnowdropDebugUI::ReadGraphCount());
      ImGui::Text("UI graph manager count (qword_480ECE0+0x28): %d",
                  SnowdropDebugUI::ReadUIMgrCount());
      if (SnowdropDebugUI::g_countBefore >= 0)
      {
          ImGui::Text("Last attempt:");
          ImGui::Text("  watch list: %d -> %d  (delta %+d)",
                      SnowdropDebugUI::g_countBefore,
                      SnowdropDebugUI::g_countAfter,
                      SnowdropDebugUI::g_countAfter - SnowdropDebugUI::g_countBefore);
          ImGui::Text("  UI mgr:     %d -> %d  (delta %+d)",
                      SnowdropDebugUI::g_mgrCountBefore,
                      SnowdropDebugUI::g_mgrCountAfter,
                      SnowdropDebugUI::g_mgrCountAfter - SnowdropDebugUI::g_mgrCountBefore);
          ImGui::Text("  Gate byte was: 0x%02X (forced to 1 during call)",
                      SnowdropDebugUI::g_gateByteBefore);
      }
      if (SnowdropDebugUI::g_lastResult)
          ImGui::Text("Status: %s", SnowdropDebugUI::g_lastResult);

      ImGui::Spacing();
      ImGui::Separator();
      ImGui::TextDisabled("Diagnostic (no gate-byte flip):");
      if (ImGui::Button("Load myDebugGraphs (raw)"))
          SnowdropDebugUI::Load("myDebugGraphs");
      ImGui::SameLine();
      if (ImGui::Button("Reload myUIGraphs (raw)"))
          SnowdropDebugUI::Load("myUIGraphs");
      if (ImGui::Button("Load myUIStartupGraphs (raw)"))
          SnowdropDebugUI::Load("myUIStartupGraphs");
      ImGui::SameLine();
      if (ImGui::Button("Load myPCGraphs (raw)"))
          SnowdropDebugUI::Load("myPCGraphs");
  }
}