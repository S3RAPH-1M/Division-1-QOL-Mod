#pragma once
#include "IniWriter.h"
#include "IniReader.h"
#include "KeybindHelper.h"

class ConfigManager
{

public:
    ConfigManager();
    ~ConfigManager();

    void DrawUI();
public:
    ConfigManager(ConfigManager const&) = delete;
    void operator=(ConfigManager const&) = delete;
};

extern KeyBind menuKey;


void LoadSettings();
void SaveSettings();