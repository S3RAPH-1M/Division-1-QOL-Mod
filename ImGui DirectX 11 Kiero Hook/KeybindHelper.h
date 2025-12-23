#pragma once
#include <vector>
#include <string>
#include <Windows.h>
#include <algorithm>
#include "IniWriter.h"
#include "IniReader.h"

struct KeyBind
{
    std::vector<int> keys;
    bool waitingForInput = false;
};

namespace KeybindHelper
{
    bool AnyKeyDown();
    std::string KeyToString(int vk);
    bool IsKeyBindPressed(const KeyBind& bind);
    void DrawKeyBindButton(const char* label, KeyBind& bind);
    void SaveKeyBind(const char* section, const KeyBind& bind, CIniWriter& writer);
    void LoadKeyBind(const char* section, KeyBind& bind, CIniReader& reader);
}
