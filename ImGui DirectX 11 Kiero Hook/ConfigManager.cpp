#include "ConfigManager.h"
#include "Util.h"
#include "imgui/imgui.h"
#include "XorStr.hpp"
#include "IniWriter.h"
#include "IniReader.h"
#include <string>
#include "Main.h"
#include "attributes.h"
#include "configGlobals.h"
#include "KeybindHelper.h"
#include "CameraManager.h"

ConfigManager::ConfigManager()
{
}

ConfigManager::~ConfigManager()
{

}

void LoadSettings()
{
	try {
		UseFOV = iniReader.ReadBoolean(str_settings, (char*)"usefov", false);
		UseFOVZoom = iniReader.ReadBoolean(str_settings, (char*)"usefovzoom", false);
		FovAmount = iniReader.ReadInteger(str_settings, (char*)"fovAmount", 70);
		ZoomSpeed = iniReader.ReadInteger(str_settings, (char*)"ZoomSpeed", 3);
		ZoomFovAmount = iniReader.ReadInteger(str_settings, (char*)"ZoomFovAmount", 55);
		g_gameUIDisabled = iniReader.ReadInteger(str_settings, (char*)"hideHUD", g_gameUIDisabled);
		KeybindHelper::LoadKeyBind(str_settings, "zoomKey", ZoomKey, iniReader);
		KeybindHelper::LoadKeyBind(str_settings, "menuKey", menuKey, iniReader);
	}
	catch (...) {
		UseFOV = false;
		UseFOVZoom = false;
		FovAmount = 70;
		ZoomSpeed = 3;
		ZoomFovAmount = 55;
	}
}

void SaveSettings() 
{
	try {
		iniWriter.WriteBoolean(str_settings, (char*)"useFOV", UseFOV);
		iniWriter.WriteBoolean(str_settings, (char*)"useFOVZoom", UseFOVZoom);
		iniWriter.WriteInteger(str_settings, (char*)"fovAmount", FovAmount);
		iniWriter.WriteInteger(str_settings, (char*)"zoomSpeed", ZoomSpeed);
		iniWriter.WriteInteger(str_settings, (char*)"zoomFovAmount", ZoomFovAmount);
		iniWriter.WriteInteger(str_settings, (char*)"hideHUD", g_gameUIDisabled);
		KeybindHelper::SaveKeyBind(str_settings, "zoomKey", ZoomKey, iniWriter);
		KeybindHelper::SaveKeyBind(str_settings, "menuKey", menuKey, iniWriter);
	}
	catch (...) {
		ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 0, 0, 255));
		ImGui::SameLine();
		ImGui::Text(xor ("Save settings failed"));
		ImGui::PopStyleColor();
	}
}

KeyBind menuKey;
void ConfigManager::DrawUI()
{
	ImGui::Text(xor ("Settings"));

	if (ImGui::Button(xor ("Save Settings"))) 
	{
		SaveSettings();
	}

	KeybindHelper::DrawKeyBindButton(xor ("Menu Button"), menuKey);
}