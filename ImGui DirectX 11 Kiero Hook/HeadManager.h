#pragma once
#include "IniWriter.h"
#include "IniReader.h"

class HeadManager
{
public:
    HeadManager();
    ~HeadManager();

    void InstallHook();
    void Update();
    void DrawUI();

    bool  m_headShrinkEnabled;
    float m_headScale;

public:
    HeadManager(HeadManager const&) = delete;
    void operator=(HeadManager const&) = delete;
};

extern HeadManager* g_pHeadManager;
