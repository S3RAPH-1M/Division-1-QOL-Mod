#pragma once
#include <vector>
#include <string>
#include "Snowdrop.h"

// Read-only inspector tab that lists every player-type agent in the world
// and shows their stats when one is selected. Refresh is manual (button)
// so we don't allocate / rebuild the name list every frame.
class AgentInspector
{
public:
    AgentInspector();
    ~AgentInspector();

    void DrawUI();

    // Rebuild the cached list of player agents from World->m_AgentArray.
    // Filters by Agent::m_EntityType == 1 (player).
    void RefreshPlayerList();

    // One-time heap scan for the local player's PlayerSessionState struct.
    // Fingerprints by the Credits item UUID at +0x450. Caches the result;
    // subsequent calls are cheap until the pointer is invalidated.
    void FindSessionState();

private:
    // Helpers
    static const char* SdsCString(const TD::SnowdropString& s);
    static bool        IsAgentValid(TD::Agent* a);

    void DrawAgentStats(TD::Agent* a);
    void DrawConsumables();

private:
    std::vector<TD::Agent*> m_players;
    std::vector<std::string> m_displayNames; // cached display strings for the list
    int m_selectedIndex = -1;

    // All PlayerSessionState candidates found by the scan. Index 0 is
    // conventionally the local player (verified live). Additional entries —
    // if any — would indicate remote players' state is replicated client-side.
    std::vector<TD::PlayerSessionState*> m_sessionStates;
    bool m_sessionScanAttempted = false;

public:
    AgentInspector(AgentInspector const&) = delete;
    void operator=(AgentInspector const&) = delete;
};
