#pragma once
#include "IniWriter.h"
#include "IniReader.h"

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

void LoadSettings();
void SaveSettings();