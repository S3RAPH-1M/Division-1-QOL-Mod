#include "VisualManager.h"
#include "Snowdrop.h"
#include "Util.h"
#include "imgui/imgui.h"
#include <string>

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

void VisualManager::DrawUI()
{
  TD::EnvironmentManager* pEnvManager = TD::RogueClient::Singleton()->m_pClient->m_pWorld->m_pEnvironmentManager;
  ImGui::Dummy(ImVec2(0, 10));
  ImGui::Columns(4, "VisualColumns", false);
  ImGui::NextColumn();
  ImGui::SetColumnOffset(-1, 12);
  ImGui::PushItemWidth(200);

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
  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 10));
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

  if (ImGui::Checkbox("Override time of day", &m_overrideTimeOfDay))
  {
    pEnvManager->m_FreezeToD = m_overrideTimeOfDay;
    if (m_overrideTimeOfDay)
    {
      int hours = m_customTimeOfDay / 100;
      int minutes = m_customTimeOfDay - hours;
      pEnvManager->m_TimeOfDay = (hours * 60 + minutes) * 60 * 1000;
    }
  }
  ImGui::PopStyleVar();

  ImGui::NextColumn();
  ImGui::SetColumnOffset(-1, 552);
  ImGui::PushItemWidth(200);

  ImGui::Text("Blend Transition Time Start (ms)");
  ImGui::InputInt("##BlendTransitionTimeStart", &pEnvManager->m_WeatherTimer, 1000, 1000);
  ImGui::Text("Blend Transition Time End (ms)");
  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 10));
  ImGui::InputInt("##BlendTransitionTimeEnd", &pEnvManager->m_WeatherTimerMax, 1000, 1000);

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

  ImGui::PopStyleVar();
}