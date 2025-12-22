#include "KeybindHelper.h"
#include <Windows.h>
#include <algorithm>
#include "imgui/imgui.h"

namespace KeybindHelper
{
    bool AnyKeyDown()
    {
        for (int key = 1; key <= 254; key++)
            if (GetAsyncKeyState(key) & 0x8000)
                return true;
        return false;
    }

    std::string KeyToString(int vk)
    {
        char name[128] = {};
        if (GetKeyNameTextA(MapVirtualKeyA(vk, MAPVK_VK_TO_VSC) << 16, name, 127))
            return std::string(name);

        switch (vk)
        {
        case VK_LCONTROL: return "L-Ctrl";
        case VK_RCONTROL: return "R-Ctrl";
        case VK_LSHIFT: return "L-Shift";
        case VK_RSHIFT: return "R-Shift";
        case VK_LMENU: return "L-Alt";
        case VK_RMENU: return "R-Alt";
        case VK_LWIN: return "L-Win";
        case VK_RWIN: return "R-Win";
        case VK_SPACE: return "Space";
        case VK_RETURN: return "Enter";
        case VK_ESCAPE: return "Escape";
        default: return "Unknown";
        }
    }

    bool IsKeyBindPressed(const KeyBind& bind)
    {
        if (bind.keys.empty()) return false;
        for (int key : bind.keys)
            if (!(GetAsyncKeyState(key) & 0x8000))
                return false;
        return true;
    }

    void DrawKeyBindButton(const char* label, KeyBind& bind)
    {
        std::string buttonText;
        if (bind.waitingForInput)
            buttonText = "Press keys...";
        else if (bind.keys.empty())
            buttonText = "Unbound";
        else
        {
            for (size_t i = 0; i < bind.keys.size(); i++)
            {
                buttonText += KeyToString(bind.keys[i]);
                if (i + 1 < bind.keys.size())
                    buttonText += " + ";
            }
        }

        if (ImGui::Button(buttonText.c_str()))
        {
            bind.keys.clear();
            bind.waitingForInput = true;
        }

        if (bind.waitingForInput)
        {
            bool hasCtrl = false, hasShift = false, hasAlt = false;

            for (int key = 1; key <= 254; key++)
            {
                if (GetAsyncKeyState(key) & 0x8000)
                {
                    if (key == VK_ESCAPE)
                    {
                        bind.keys.clear();
                        bind.waitingForInput = false;
                        break;
                    }

                    if (key == VK_LCONTROL || key == VK_RCONTROL)
                    {
                        if (hasCtrl) continue;
                        key = VK_CONTROL;
                        hasCtrl = true;
                    }
                    else if (key == VK_LSHIFT || key == VK_RSHIFT)
                    {
                        if (hasShift) continue;
                        key = VK_SHIFT;
                        hasShift = true;
                    }
                    else if (key == VK_LMENU || key == VK_RMENU) // ALT
                    {
                        if (hasAlt) continue;
                        key = VK_MENU;
                        hasAlt = true;
                    }

                    if (std::find(bind.keys.begin(), bind.keys.end(), key) == bind.keys.end())
                        bind.keys.push_back(key);
                }
            }

            if (!AnyKeyDown() && !bind.keys.empty())
                bind.waitingForInput = false;
        }

        ImGui::SameLine();
        ImGui::Text("%s", label);
    }
}
