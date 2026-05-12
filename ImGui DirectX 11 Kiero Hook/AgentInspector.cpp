#include "AgentInspector.h"
#include "imgui/imgui.h"

#include <cstdio>
#include <cstring>
#include <Windows.h>

AgentInspector::AgentInspector() {}
AgentInspector::~AgentInspector() {}

// SnowdropString resolver: returns the underlying c-string for an SDS.
// Layout (per Snowdrop.h): heap_ptr at +0x00 (used when flag byte at +0x0F == 1);
// otherwise the 16-byte buffer holds the chars inline.
const char* AgentInspector::SdsCString(const TD::SnowdropString& s)
{
    if (s.bytes[0x0F])
        return *reinterpret_cast<const char* const*>(s.bytes);
    return reinterpret_cast<const char*>(s.bytes);
}

bool AgentInspector::IsAgentValid(TD::Agent* a)
{
    if (!a) return false;
    auto p = reinterpret_cast<uintptr_t>(a);
    if (p < 0x10000000000ULL || p > 0x2300000000000ULL) return false;
    return true;
}

// Cheap heap-range filter. Heap on this build is roughly
// 0x100_0000_0000 .. 0x2300_0000_0000. We deliberately do NOT call
// VirtualQuery here — it's a kernel syscall and gets hit several times
// per frame in the inspector's hot path, causing visible lag. The SEH
// __try/__except wrappers around the actual reads already catch any
// access violation from a stale-but-in-range pointer.
static inline bool LooksLikeHeapPtr(const void* p)
{
    auto v = reinterpret_cast<uintptr_t>(p);
    return v >= 0x10000000000ULL && v < 0x2300000000000ULL;
}

// SEH-guarded probe of an Agent's identity / status fields. Reads everything
// into a POD struct so the SEH function has no C++ objects with destructors
// (MSVC C2712 constraint per CLAUDE.md). Returns true on success.
struct AgentSnap
{
    void*     vtable;
    void*     info;
    int       entityType;
    uint32_t  entityId1;
    uint32_t  entityId2;
    uint8_t   isDead;
    uint8_t   isRogue;
    uint8_t   dzFallbackA;
    uint8_t   dzFallbackB;
    char      tag[9];
    float     posX, posY, posZ;
    float     sX, sY, sZ;
    int       compCount;
    void*     compSet;
    void*     skinnedMesh;
    void*     world;
    void*     appearance;
    void*     inventory;
    // AgentInfo fields (optional — only valid if info != nullptr)
    uint32_t  infoId;
    uint8_t   infoTypeByte;
    int       infoNameIsHeap;  // 1 if SDS in heap mode, 0 if inline
    char      infoNameInline[17];
    const char* infoNameHeap;
    // CharGuid
    int       guidIsHeap;
    char      guidInline[17];
    const char* guidHeap;
    uint64_t  guidHash0;
    uint64_t  guidHash1;
};

static bool SafeReadAgent(TD::Agent* a, AgentSnap* out)
{
    __try
    {
        out->vtable      = *(void**)((BYTE*)a + 0x000);
        out->info        = *(void**)((BYTE*)a + 0x028);
        out->world       = *(void**)((BYTE*)a + 0x038);
        out->isDead      = *(uint8_t*)((BYTE*)a + 0x1C0);
        out->skinnedMesh = *(void**)((BYTE*)a + 0x1D0);
        std::memcpy(out->tag, (BYTE*)a + 0x1D8, 8);
        out->tag[8] = 0;
        for (int b = 0; b < 8; ++b)
            if (out->tag[b] < 0x20 || out->tag[b] > 0x7E) out->tag[b] = 0;
        out->dzFallbackA = *(uint8_t*)((BYTE*)a + 0x255);
        out->dzFallbackB = *(uint8_t*)((BYTE*)a + 0x257);
        out->entityType  = *(int*)((BYTE*)a + 0x3A4);
        out->entityId1   = *(uint32_t*)((BYTE*)a + 0x3A8);
        out->entityId2   = *(uint32_t*)((BYTE*)a + 0x3AC);
        out->guidHash0   = *(uint64_t*)((BYTE*)a + 0x418);
        out->guidHash1   = *(uint64_t*)((BYTE*)a + 0x420);
        out->guidIsHeap  = *(uint8_t*)((BYTE*)a + 0x437) ? 1 : 0;
        if (out->guidIsHeap)
            out->guidHeap = *(const char**)((BYTE*)a + 0x428);
        else
        {
            std::memcpy(out->guidInline, (BYTE*)a + 0x428, 16);
            out->guidInline[16] = 0;
            out->guidHeap = nullptr;
        }
        out->compCount   = *(int*)((BYTE*)a + 0x4B0);
        out->compSet     = *(void**)((BYTE*)a + 0x4B8);
        out->inventory   = *(void**)((BYTE*)a + 0x5B0);
        out->isRogue     = *(uint8_t*)((BYTE*)a + 0x75C);
        out->appearance  = *(void**)((BYTE*)a + 0x7C8);

        out->posX = ((float*)((BYTE*)a + 0x70))[0];
        out->posY = ((float*)((BYTE*)a + 0x70))[1];
        out->posZ = ((float*)((BYTE*)a + 0x70))[2];
        out->sX   = *(float*)((BYTE*)a + 0x960);
        out->sY   = *(float*)((BYTE*)a + 0x974);
        out->sZ   = *(float*)((BYTE*)a + 0x988);

        // AgentInfo — only probe if pointer looks valid
        out->infoId          = 0;
        out->infoTypeByte    = 0;
        out->infoNameIsHeap  = 0;
        out->infoNameInline[0] = 0;
        out->infoNameHeap    = nullptr;
        if (out->info && (uintptr_t)out->info > 0x10000000000ULL)
        {
            out->infoId       = *(uint32_t*)((BYTE*)out->info + 0x08);
            out->infoTypeByte = *(uint8_t*)((BYTE*)out->info + 0x0C);
            uint8_t flag      = *(uint8_t*)((BYTE*)out->info + 0x1F); // SDS at +0x10
            out->infoNameIsHeap = flag ? 1 : 0;
            if (flag)
                out->infoNameHeap = *(const char**)((BYTE*)out->info + 0x10);
            else
            {
                std::memcpy(out->infoNameInline, (BYTE*)out->info + 0x10, 16);
                out->infoNameInline[16] = 0;
            }
        }
        return true;
    }
    __except(EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

// SEH-guarded readback of a c-string at a heap pointer — protects against
// dereferencing a stale or invalid SDS heap_ptr. Copies up to outSize-1
// chars into out and null-terminates. Returns true on success.
static bool SafeReadCString(const char* heapPtr, char* out, size_t outSize)
{
    if (!heapPtr || !out || outSize == 0) return false;
    if (!LooksLikeHeapPtr(heapPtr)) return false;
    __try
    {
        size_t i = 0;
        for (; i < outSize - 1; ++i)
        {
            char c = heapPtr[i];
            if (c == 0) break;
            if ((unsigned char)c < 0x20 || (unsigned char)c > 0x7E) { c = '?'; }
            out[i] = c;
        }
        out[i] = 0;
        return true;
    }
    __except(EXCEPTION_EXECUTE_HANDLER)
    {
        out[0] = 0;
        return false;
    }
}

void AgentInspector::RefreshPlayerList()
{
    m_players.clear();
    m_displayNames.clear();

    TD::RogueClient* rc = TD::RogueClient::Singleton();
    if (!rc || !rc->m_pClient) return;
    TD::World* world = rc->m_pClient->m_pWorld;
    if (!world || !world->m_AgentArray || world->m_AgentCount <= 0) return;

    for (int i = 0; i < world->m_AgentCount; ++i)
    {
        TD::Agent* a = world->m_AgentArray[i];
        if (!IsAgentValid(a)) continue;

        AgentSnap snap{};
        if (!SafeReadAgent(a, &snap)) continue;
        if (snap.entityType != 1) continue; // players only

        m_players.push_back(a);

        // Build display name from the AgentInfo SnowdropString
        char name[64] = "?";
        if (snap.infoNameIsHeap)
        {
            SafeReadCString(snap.infoNameHeap, name, sizeof(name));
            if (!name[0]) std::snprintf(name, sizeof(name), "<bad-heap-name>");
        }
        else
        {
            const size_t cap = sizeof(name) - 1;
            size_t i = 0;
            for (; i < cap && snap.infoNameInline[i]; ++i)
                name[i] = snap.infoNameInline[i];
            name[i] = 0;
        }

        char line[160];
        std::snprintf(line, sizeof(line), "[%d] %s  (%s)",
                      (int)m_players.size() - 1,
                      name[0] ? name : "<unnamed>",
                      snap.tag[0] ? snap.tag : "no-tag");
        m_displayNames.emplace_back(line);
    }

    if (m_selectedIndex >= (int)m_players.size()) m_selectedIndex = -1;
}

void AgentInspector::DrawAgentStats(TD::Agent* a)
{
    if (!IsAgentValid(a))
    {
        ImGui::TextColored(ImVec4(1, 0.4f, 0.4f, 1), "Invalid agent pointer.");
        return;
    }

    AgentSnap s{};
    if (!SafeReadAgent(a, &s))
    {
        ImGui::TextColored(ImVec4(1, 0.4f, 0.4f, 1),
            "Read failed (access violation). Agent may have been freed.");
        return;
    }

    // Bounded inline-buffer copy (avoids deprecated strncpy).
    auto copyInline = [](char* dst, size_t dstSize, const char* src, size_t srcMax)
    {
        if (!dstSize) return;
        const size_t cap = dstSize - 1;
        const size_t lim = (srcMax < cap) ? srcMax : cap;
        size_t i = 0;
        for (; i < lim && src[i]; ++i) dst[i] = src[i];
        dst[i] = 0;
    };

    // Resolve display strings safely
    char name[128] = "?";
    if (s.infoNameIsHeap) SafeReadCString(s.infoNameHeap, name, sizeof(name));
    else                  copyInline(name, sizeof(name), s.infoNameInline, 16);

    char guidStr[128] = "?";
    if (s.guidIsHeap)
    {
        if (!SafeReadCString(s.guidHeap, guidStr, sizeof(guidStr)))
            std::snprintf(guidStr, sizeof(guidStr), "<bad ptr 0x%p>", s.guidHeap);
    }
    else
        copyInline(guidStr, sizeof(guidStr), s.guidInline, 16);

    ImGui::TextColored(ImVec4(1, 0.8f, 0.2f, 1), "Identity");
    ImGui::Separator();
    ImGui::Text("Agent*:        0x%p", (void*)a);
    ImGui::Text("vtable:        0x%p", s.vtable);
    ImGui::Text("AgentInfo*:    0x%p", s.info);
    ImGui::Text("Name:          %s", name[0] ? name : "<empty>");
    if (s.info)
    {
        ImGui::Text("AgentInfo.Id:  0x%08X", s.infoId);
        ImGui::Text("Type byte:     %u", (unsigned)s.infoTypeByte);
    }
    ImGui::Text("Entity tag:    %s", s.tag[0] ? s.tag : "<empty>");
    ImGui::Text("Entity type:   %d  (1=player, 7=NPC)", s.entityType);
    ImGui::Text("Entity IDs:    %u / %u", s.entityId1, s.entityId2);
    ImGui::Text("CharGUID:      %s", guidStr);
    ImGui::Text("CharGUID hash: 0x%016llX %016llX",
                (unsigned long long)s.guidHash0,
                (unsigned long long)s.guidHash1);

    ImGui::Spacing();
    ImGui::TextColored(ImVec4(1, 0.8f, 0.2f, 1), "Status");
    ImGui::Separator();
    ImGui::Text("Is dead:       %s", s.isDead ? "YES" : "no");
    ImGui::Text("Is rogue:      %s", s.isRogue ? "YES" : "no");
    ImGui::Text("DZ fallback A: %u", (unsigned)s.dzFallbackA);
    ImGui::Text("DZ fallback B: %u", (unsigned)s.dzFallbackB);

    ImGui::Spacing();
    ImGui::TextColored(ImVec4(1, 0.8f, 0.2f, 1), "Transform");
    ImGui::Separator();
    ImGui::Text("Position:      (%.2f, %.2f, %.2f)", s.posX, s.posY, s.posZ);
    ImGui::Text("Scale (XYZ):   %.3f / %.3f / %.3f", s.sX, s.sY, s.sZ);

    ImGui::Spacing();
    ImGui::TextColored(ImVec4(1, 0.8f, 0.2f, 1), "Components");
    ImGui::Separator();
    ImGui::Text("ComponentSet*: 0x%p (count %d)", s.compSet, s.compCount);
    ImGui::Text("SkinnedMesh*:  0x%p", s.skinnedMesh);

    ImGui::Spacing();
    ImGui::TextColored(ImVec4(1, 0.8f, 0.2f, 1), "Pointers");
    ImGui::Separator();
    ImGui::Text("World*:           0x%p", s.world);
    ImGui::Text("AppearanceMgr*:   0x%p", s.appearance);
    ImGui::Text("PlayerInventory*: 0x%p", s.inventory);

    ImGui::Spacing();
    DrawAgentWallet(a);
}

// SEH-guarded validation of a candidate PlayerSessionState pointer.
// Verifies the Credits UUID is at +0x450 (the fingerprint) and that the
// grenade/medkit counts at +0x000 and +0x040 look like plausible small
// integers. Returns true if the candidate looks valid.
static bool VerifySessionState(TD::PlayerSessionState* ps)
{
    if (!ps) return false;
    if (!LooksLikeHeapPtr(ps)) return false;
    __try
    {
        // Re-check the Credits UUID is at +0x450 (defensive — we just
        // scanned for it, but the page could have been freed between).
        const uint8_t* uid = (const uint8_t*)ps + 0x450;
        for (int b = 0; b < 16; ++b)
            if (uid[b] != TD::CurrencyUid::Credits[b]) return false;
        // Sanity: grenade/medkit caps are small (typically <= ~20). Reject
        // anything > 1000 as a false positive.
        uint32_t g = ps->Grenades();
        uint32_t m = ps->Medkits();
        if (g > 1000 || m > 1000) return false;
        return true;
    }
    __except(EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

// Scan all committed private (= heap) memory regions for the 16-byte
// Credits item UUID. PlayerSessionState* = (match_address - 0x450).
// Accumulates ALL verified candidates into 'out' so we can detect whether
// remote players' session state is replicated client-side.
static void ScanAllSessionStates(std::vector<TD::PlayerSessionState*>& out)
{
    out.clear();

    SYSTEM_INFO si{};
    GetSystemInfo(&si);

    BYTE* p   = (BYTE*)si.lpMinimumApplicationAddress;
    BYTE* end = (BYTE*)si.lpMaximumApplicationAddress;

    while (p < end)
    {
        MEMORY_BASIC_INFORMATION mbi{};
        if (!VirtualQuery(p, &mbi, sizeof(mbi))) break;

        const DWORD readableMask =
            PAGE_READONLY | PAGE_READWRITE | PAGE_WRITECOPY |
            PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY;
        const bool readable =
            mbi.State == MEM_COMMIT &&
            (mbi.Protect & readableMask) &&
            !(mbi.Protect & (PAGE_GUARD | PAGE_NOACCESS)) &&
            mbi.Type == MEM_PRIVATE;   // heap, not module image

        if (readable && mbi.RegionSize >= 0x500)
        {
            __try
            {
                BYTE* base = (BYTE*)mbi.BaseAddress;
                const size_t n = mbi.RegionSize;
                for (size_t i = 0; i + 16 + 0x500 <= n; i += 4)
                {
                    if (*(uint32_t*)(base + i) !=
                        *(uint32_t*)TD::CurrencyUid::Credits) continue;
                    if (std::memcmp(base + i, TD::CurrencyUid::Credits, 16) != 0)
                        continue;
                    if (i < 0x450) continue;
                    TD::PlayerSessionState* candidate =
                        (TD::PlayerSessionState*)(base + i - 0x450);
                    if (VerifySessionState(candidate))
                        out.push_back(candidate);
                }
            }
            __except(EXCEPTION_EXECUTE_HANDLER) { /* skip page */ }
        }

        p = (BYTE*)mbi.BaseAddress + mbi.RegionSize;
    }
}

void AgentInspector::FindSessionState()
{
    m_sessionScanAttempted = true;
    ScanAllSessionStates(m_sessionStates);
}

// SEH-guarded read of all the resource fields we care about from one
// PlayerSessionState candidate. POD-only locals (no C++ destructors).
struct SessionFields
{
    bool     ok;
    uint32_t grenades;
    uint32_t medkits;
    uint32_t credits;
    uint32_t dzFund;
    uint32_t phoenix;
    uint32_t targetIntel;
    uint32_t dzKeys;
    uint32_t premiumCreds;
    uint32_t geCredits;
};

static SessionFields ReadSessionFields(TD::PlayerSessionState* ps)
{
    SessionFields f{};
    if (!ps) return f;
    __try
    {
        f.grenades     = ps->Grenades();
        f.medkits      = ps->Medkits();
        f.credits      = ps->Credits();
        f.dzFund       = ps->DZFund();
        f.phoenix      = ps->PhoenixCredits();
        f.targetIntel  = ps->TargetIntel();
        f.dzKeys       = ps->DZKeys();
        f.premiumCreds = ps->PremiumCredits();
        f.geCredits    = ps->GECredits();
        f.ok = true;
    }
    __except(EXCEPTION_EXECUTE_HANDLER)
    {
        f.ok = false;
    }
    return f;
}

// Format a uint32 with thousands separators into a fixed buffer (POD only,
// no std::string — keeps callers SEH-safe and avoids per-frame allocations).
// E.g. 5069574 -> "5,069,574".
static void FormatThousands(uint32_t v, char* out, size_t outSize)
{
    if (!out || outSize == 0) return;
    char raw[16];
    int n = std::snprintf(raw, sizeof(raw), "%u", v);
    if (n <= 0) { out[0] = 0; return; }

    int commaInserts = (n - 1) / 3;
    int total = n + commaInserts;
    if ((size_t)total + 1 > outSize) { std::snprintf(out, outSize, "%u", v); return; }

    int srcEnd = n;
    int dstEnd = total;
    out[dstEnd--] = 0;
    int groupCount = 0;
    while (srcEnd > 0)
    {
        if (groupCount == 3) { out[dstEnd--] = ','; groupCount = 0; }
        out[dstEnd--] = raw[--srcEnd];
        ++groupCount;
    }
}

// Is the given agent the LOCAL player (Agent[0] of the World's agent array)?
static bool IsLocalAgent(TD::Agent* a)
{
    TD::RogueClient* rc = TD::RogueClient::Singleton();
    if (!rc || !rc->m_pClient || !rc->m_pClient->m_pWorld) return false;
    TD::World* w = rc->m_pClient->m_pWorld;
    if (!w->m_AgentArray || w->m_AgentCount <= 0) return false;
    return w->m_AgentArray[0] == a;
}

void AgentInspector::DrawAgentWallet(TD::Agent* a)
{
    ImGui::TextColored(ImVec4(1, 0.8f, 0.2f, 1), "Wallet");
    ImGui::Separator();

    // Auto-scan once so the panel populates without a button click.
    if (!m_sessionScanAttempted)
        FindSessionState();

    // Drop any cached candidates that have gone stale (heap can reallocate).
    for (auto it = m_sessionStates.begin(); it != m_sessionStates.end(); )
    {
        if (!VerifySessionState(*it))
            it = m_sessionStates.erase(it);
        else
            ++it;
    }

    if (ImGui::SmallButton("Re-scan session state"))
    {
        m_sessionStates.clear();
        FindSessionState();
        return;
    }

    if (m_sessionStates.empty())
    {
        ImGui::TextColored(ImVec4(1, 0.6f, 0.2f, 1),
            "No PlayerSessionState located by heap scan.");
        return;
    }

    // Pick which session state to display for this agent.
    //   Local player  -> candidate 0 (verified to be the local wallet).
    //   Remote player -> if exactly 2 candidates exist, use candidate 1
    //                    (best-effort; remote replication is unverified).
    //                    Otherwise note the limitation rather than guess.
    const bool local = IsLocalAgent(a);
    TD::PlayerSessionState* ps = nullptr;

    if (local)
    {
        ps = m_sessionStates[0];
    }
    else if (m_sessionStates.size() == 2)
    {
        ps = m_sessionStates[1];
        ImGui::TextColored(ImVec4(1, 0.7f, 0.2f, 1),
            "Note: remote-wallet attribution is best-effort — using the\n"
            "non-local PlayerSessionState candidate. Verify before trusting.");
    }
    else
    {
        ImGui::TextDisabled(
            "Remote player selected, but %d session-state candidates found.\n"
            "Agent* -> PlayerSessionState* mapping is not yet implemented,\n"
            "so we can't attribute a specific wallet to this agent.",
            (int)m_sessionStates.size());
        return;
    }

    SessionFields f = ReadSessionFields(ps);
    if (!f.ok)
    {
        ImGui::TextColored(ImVec4(1, 0.4f, 0.4f, 1),
            "Read failed — pointer became invalid.");
        return;
    }

    ImGui::TextDisabled("SessionState* 0x%p", (void*)ps);
    ImGui::Spacing();

    // Inline "Label: value" rows. Currencies routinely run into the millions
    // so we format with thousands separators for readability.
    auto walletLine = [](const char* label, uint32_t value)
    {
        char buf[32];
        FormatThousands(value, buf, sizeof(buf));
        ImGui::Text("%-18s %s", label, buf);
    };

    ImGui::Text("Currencies");
    ImGui::Indent();
    walletLine("Credits:",         f.credits);
    walletLine("Premium Credits:", f.premiumCreds);
    walletLine("Phoenix Credits:", f.phoenix);
    walletLine("DZ Fund:",         f.dzFund);
    walletLine("Dark Zone Keys:",  f.dzKeys);
    walletLine("Target Intel:",    f.targetIntel);
    walletLine("GE Credits:",      f.geCredits);
    ImGui::Unindent();

    ImGui::Spacing();
    ImGui::Text("Consumables");
    ImGui::Indent();
    walletLine("Grenades:", f.grenades);
    walletLine("Medkits:",  f.medkits);
    ImGui::Unindent();
}

void AgentInspector::DrawUI()
{
    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 204, 51, 255));
    ImGui::Text("Player / Agent Inspector");
    ImGui::PopStyleColor();
    ImGui::Separator();

    if (ImGui::Button("Refresh Player List"))
        RefreshPlayerList();
    ImGui::SameLine();
    ImGui::TextDisabled("(%d players cached)", (int)m_players.size());

    ImGui::Spacing();

    // We're already inside the right-half of the parent window's outer
    // 2-column layout. Use BeginChild with EXPLICIT widths for both panels
    // so neither escapes the parent column. Stacked vertically when the
    // column is narrow (< 500px), side-by-side when there's room.
    const float avail_w = ImGui::GetContentRegionAvail().x;
    const float avail_h = ImGui::GetContentRegionAvail().y;
    const float gap     = 6.0f;
    const bool  stacked = (avail_w < 500.0f);

    if (stacked)
    {
        // Narrow column: list on top (1/3 of height), detail below.
        const float list_h = avail_h * 0.35f;
        ImGui::BeginChild("##agent_list_v", ImVec2(avail_w, list_h),
                          true, ImGuiWindowFlags_HorizontalScrollbar);
        {
            if (m_players.empty())
                ImGui::TextWrapped(
                    "No players cached. Click \"Refresh Player List\" while in-world.");
            else
            {
                for (int i = 0; i < (int)m_players.size(); ++i)
                {
                    const bool selected = (m_selectedIndex == i);
                    if (ImGui::Selectable(m_displayNames[i].c_str(), selected))
                        m_selectedIndex = i;
                }
            }
        }
        ImGui::EndChild();

        ImGui::BeginChild("##agent_detail_v", ImVec2(avail_w, avail_h - list_h - gap),
                          true, ImGuiWindowFlags_HorizontalScrollbar);
        {
            if (m_selectedIndex < 0 || m_selectedIndex >= (int)m_players.size())
                ImGui::TextDisabled("Select a player above to inspect.");
            else
                DrawAgentStats(m_players[m_selectedIndex]);
        }
        ImGui::EndChild();
    }
    else
    {
        // Wide column: list left (fixed 240), detail right (remaining).
        const float list_w   = 240.0f;
        const float detail_w = avail_w - list_w - gap;

        ImGui::BeginChild("##agent_list_h", ImVec2(list_w, avail_h),
                          true, ImGuiWindowFlags_HorizontalScrollbar);
        {
            if (m_players.empty())
                ImGui::TextWrapped(
                    "No players cached.\nClick \"Refresh Player List\" while in-world.");
            else
            {
                for (int i = 0; i < (int)m_players.size(); ++i)
                {
                    const bool selected = (m_selectedIndex == i);
                    if (ImGui::Selectable(m_displayNames[i].c_str(), selected))
                        m_selectedIndex = i;
                }
            }
        }
        ImGui::EndChild();

        ImGui::SameLine(0.0f, gap);

        ImGui::BeginChild("##agent_detail_h", ImVec2(detail_w, avail_h),
                          true, ImGuiWindowFlags_HorizontalScrollbar);
        {
            if (m_selectedIndex < 0 || m_selectedIndex >= (int)m_players.size())
                ImGui::TextDisabled("Select a player on the left to inspect.");
            else
                DrawAgentStats(m_players[m_selectedIndex]);
        }
        ImGui::EndChild();
    }
}
