#pragma once
#include <vector>
#include <string>
#include <Windows.h>
#include <algorithm>

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
}
