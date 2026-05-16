#pragma once

// The UI / Agent inspectors are debug-only tooling. They are present in the
// Debug and DebugAndCopy configurations (both build against the debug CRT,
// which defines _DEBUG) and compiled out entirely of Release / ReleaseAndCopy.
#ifndef QOL_ENABLE_INSPECTORS
  #ifdef _DEBUG
    #define QOL_ENABLE_INSPECTORS 1
  #else
    #define QOL_ENABLE_INSPECTORS 0
  #endif
#endif

#if QOL_ENABLE_INSPECTORS

#include <vector>
#include <string>
#include <cstdint>
#include "Snowdrop.h"

// Read-only inspector that walks World::m_AgentArray and lists every
// player-type agent (Agent::m_EntityType == 1). NPCs (type 7) are filtered
// out. Refresh is manual via the button — the list is rebuilt only on
// demand so we don't allocate every frame.
//
// All Agent reads are SEH-guarded against access violations because agents
// can be freed between the array walk and the field reads. Heap-range
// filter on the pointer avoids most of those cases up front.
class AgentInspector
{
public:
    AgentInspector();
    ~AgentInspector();

    void DrawUI();

    void RefreshPlayerList();

private:
    void DrawAgentDetails(TD::Agent* a);
    void DrawAgentStats(TD::Agent* a);
    void DrawAgentGear(TD::Agent* a);

    struct Row
    {
        TD::Agent*  agent;
        std::string label;     // "[i] Name (tag)"
    };

    std::vector<Row> m_rows;
    int  m_selectedIndex = -1;
    bool m_loadedOnce    = false;

public:
    AgentInspector(AgentInspector const&) = delete;
    void operator=(AgentInspector const&) = delete;
};

#endif // QOL_ENABLE_INSPECTORS
