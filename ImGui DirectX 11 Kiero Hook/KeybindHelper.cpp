#include "KeybindHelper.h"
#include <Windows.h>
#include <algorithm>
#include "imgui/imgui.h"
#include "IniWriter.h"
#include "IniReader.h"
#include <sstream>

namespace KeybindHelper
{
    bool AnyKeyDown()
    {
        for (int key = 1; key <= 254; key++)
            if (GetAsyncKeyState(key) & 0x8000)
                return true;

        const int mouseButtons[] = { VK_LBUTTON, VK_RBUTTON, VK_MBUTTON, VK_XBUTTON1, VK_XBUTTON2 };
        for (int mouse : mouseButtons)
            if (GetAsyncKeyState(mouse) & 0x8000)
                return true;

        return false;
    }

    std::string KeyToString(int vk)
    {
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
            case VK_LBUTTON: return "Mouse L";
            case VK_RBUTTON: return "Mouse R";
            case VK_MBUTTON: return "Mouse M";
            case VK_XBUTTON1: return "Mouse 4";
            case VK_XBUTTON2: return "Mouse 5";
            default:
            {
                char name[128] = {};
                UINT scancode = MapVirtualKeyA(vk, MAPVK_VK_TO_VSC);

                switch (vk)
                {
                case VK_PRIOR:
                case VK_NEXT:
                case VK_END:
                case VK_HOME:
                case VK_INSERT:
                case VK_DELETE:
                case VK_RCONTROL:
                case VK_RMENU:
                case VK_NUMLOCK:
                case VK_DIVIDE:
                    scancode |= 0x100;
                    break;
                }

                if (GetKeyNameTextA(scancode << 16, name, 127))
                    return std::string(name);

                return "Unknown";
            }
        }
    }

    void SaveKeyBind(const char* section, const char* keyName, const KeyBind& bind, CIniWriter& writer)
    {
        std::ostringstream ss;
        for (size_t i = 0; i < bind.keys.size(); i++)
        {
            ss << bind.keys[i];
            if (i + 1 < bind.keys.size())
                ss << ",";
        }

        std::string keyString = ss.str();
        writer.WriteString((char*)section, (char*)keyName, (char*)keyString.c_str());
    }

    void LoadKeyBind(const char* section, const char* keyName, KeyBind& bind, CIniReader& reader)
    {
        char* keyString = reader.ReadString((char*)section, (char*)keyName, "");
        bind.keys.clear();

        if (keyString && strlen(keyString) > 0)
        {
            std::stringstream ss(keyString);
            std::string token;
            while (std::getline(ss, token, ','))
            {
                bind.keys.push_back(std::stoi(token));
            }
        }

        bind.waitingForInput = false;
        delete[] keyString;
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

                    // Skip Generics
                    if (key == VK_CONTROL || key == VK_SHIFT || key == VK_MENU)
                        continue;

                    if (std::find(bind.keys.begin(), bind.keys.end(), key) == bind.keys.end())
                        bind.keys.push_back(key);
                }
            }

            const int mouseButtons[] = { VK_LBUTTON, VK_RBUTTON, VK_MBUTTON, VK_XBUTTON1, VK_XBUTTON2 };
            for (int mouse : mouseButtons)
            {
                if (GetAsyncKeyState(mouse) & 0x8000)
                {
                    if (std::find(bind.keys.begin(), bind.keys.end(), mouse) == bind.keys.end())
                        bind.keys.push_back(mouse);
                }
            }

            if (!AnyKeyDown() && !bind.keys.empty())
                bind.waitingForInput = false;
        }

        ImGui::SameLine();
        ImGui::Text("%s", label);
    }
}
