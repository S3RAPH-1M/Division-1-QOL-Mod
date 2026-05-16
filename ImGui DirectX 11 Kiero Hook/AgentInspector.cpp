#include "AgentInspector.h"

#if QOL_ENABLE_INSPECTORS

#include "imgui/imgui.h"

#include <cstdio>
#include <cstring>
#include <cmath>
#include <Windows.h>

AgentInspector::AgentInspector() {}
AgentInspector::~AgentInspector() {}

// Heap-range filter for The Division's allocator. Same range UIInspector
// uses; the surrounding SEH already catches AVs from stale-but-in-range
// pointers, so we skip VirtualQuery (it's a syscall and we'd be paying it
// once per agent per refresh).
static inline bool LooksLikeHeapPtr(const void* p)
{
    auto v = reinterpret_cast<std::uintptr_t>(p);
    return v >= 0x10000000000ULL && v < 0x2300000000000ULL;
}

// POD snapshot of an Agent. C++ destructors can't live in a function that
// uses __try/__except (MSVC C2712) — so we read every field we want into
// this struct from inside the SEH helper, then format with std::string
// safely from a C++-aware caller.
struct AgentSnap
{
    void*       vtable;
    void*       info;
    void*       world;
    void*       skinnedMesh;
    void*       appearance;
    void*       inventory;
    void*       compSet;
    int         compCount;
    int         entityType;
    std::uint32_t entityId1;
    std::uint32_t entityId2;
    std::uint8_t  isDead;
    std::uint8_t  isRogue;
    std::uint8_t  dzFallbackA;
    std::uint8_t  dzFallbackB;
    char        tag[9];
    float       posX, posY, posZ;
    float       scaleX, scaleY, scaleZ;

    // AgentInfo (only filled if info != nullptr)
    std::uint32_t infoId;
    std::uint8_t  infoTypeByte;
    std::uint8_t  nameIsHeap;
    char          nameInline[17];
    const char*   nameHeap;

    // CharGuid (Agent+0x428 SnowdropString, flag at +0x437)
    std::uint8_t guidIsHeap;
    char         guidInline[17];
    const char*  guidHeap;
    std::uint64_t guidHash0;
    std::uint64_t guidHash1;
};

static bool SafeReadAgent(TD::Agent* a, AgentSnap* out)
{
    __try
    {
        BYTE* p = reinterpret_cast<BYTE*>(a);
        out->vtable      = *(void**)(p + 0x000);
        out->info        = *(void**)(p + 0x028);
        out->world       = *(void**)(p + 0x038);
        out->posX        = *(float*)(p + 0x070);
        out->posY        = *(float*)(p + 0x074);
        out->posZ        = *(float*)(p + 0x078);
        out->isDead      = *(std::uint8_t*)(p + 0x1C0);
        out->skinnedMesh = *(void**)(p + 0x1D0);

        std::memcpy(out->tag, p + 0x1D8, 8);
        out->tag[8] = 0;
        for (int b = 0; b < 8; ++b)
            if (out->tag[b] < 0x20 || out->tag[b] > 0x7E) out->tag[b] = 0;

        out->dzFallbackA = *(std::uint8_t*)(p + 0x255);
        out->dzFallbackB = *(std::uint8_t*)(p + 0x257);
        out->entityType  = *(int*)(p + 0x3A4);
        out->entityId1   = *(std::uint32_t*)(p + 0x3A8);
        out->entityId2   = *(std::uint32_t*)(p + 0x3AC);
        out->guidHash0   = *(std::uint64_t*)(p + 0x418);
        out->guidHash1   = *(std::uint64_t*)(p + 0x420);
        out->guidIsHeap  = *(std::uint8_t*)(p + 0x437) ? 1 : 0;
        if (out->guidIsHeap)
        {
            out->guidHeap = *(const char**)(p + 0x428);
            out->guidInline[0] = 0;
        }
        else
        {
            std::memcpy(out->guidInline, p + 0x428, 16);
            out->guidInline[16] = 0;
            out->guidHeap = nullptr;
        }
        out->compCount  = *(int*)(p + 0x4B0);
        out->compSet    = *(void**)(p + 0x4B8);
        out->inventory  = *(void**)(p + 0x5B0);
        out->isRogue    = *(std::uint8_t*)(p + 0x75C);
        out->appearance = *(void**)(p + 0x7C8);
        out->scaleX     = *(float*)(p + 0x960);
        out->scaleY     = *(float*)(p + 0x974);
        out->scaleZ     = *(float*)(p + 0x988);

        out->infoId        = 0;
        out->infoTypeByte  = 0;
        out->nameIsHeap    = 0;
        out->nameInline[0] = 0;
        out->nameHeap      = nullptr;
        if (out->info && LooksLikeHeapPtr(out->info))
        {
            BYTE* ip = reinterpret_cast<BYTE*>(out->info);
            out->infoId       = *(std::uint32_t*)(ip + 0x08);
            out->infoTypeByte = *(std::uint8_t*)(ip + 0x0C);
            // SnowdropString at +0x10; heap-flag byte at +0x1F
            std::uint8_t flag = *(std::uint8_t*)(ip + 0x1F);
            out->nameIsHeap = flag ? 1 : 0;
            if (flag)
                out->nameHeap = *(const char**)(ip + 0x10);
            else
            {
                std::memcpy(out->nameInline, ip + 0x10, 16);
                out->nameInline[16] = 0;
            }
        }
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

// SEH-guarded c-string read for SDS heap pointers. Copies up to outSize-1
// printable chars, NUL-terminates, returns true on success.
static bool SafeReadCString(const char* src, char* out, int outSize)
{
    if (!src || !out || outSize <= 0) return false;
    if (!LooksLikeHeapPtr(src)) return false;
    __try
    {
        int i = 0;
        for (; i < outSize - 1; ++i)
        {
            unsigned char c = static_cast<unsigned char>(src[i]);
            if (c == 0) break;
            if (c < 0x20 || c > 0x7E) c = '?';
            out[i] = static_cast<char>(c);
        }
        out[i] = 0;
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        out[0] = 0;
        return false;
    }
}

void AgentInspector::RefreshPlayerList()
{
    m_rows.clear();
    m_selectedIndex = -1;

    TD::RogueClient* rc = TD::RogueClient::Singleton();
    if (!rc || !rc->m_pClient) return;
    TD::World* world = rc->m_pClient->m_pWorld;
    if (!world || !world->m_AgentArray || world->m_AgentCount <= 0) return;

    for (int i = 0; i < world->m_AgentCount; ++i)
    {
        TD::Agent* a = world->m_AgentArray[i];
        if (!LooksLikeHeapPtr(a)) continue;

        AgentSnap s{};
        if (!SafeReadAgent(a, &s)) continue;

        // Players only — entity type 1. NPCs (type 7) are filtered out per
        // the user's request. IsPlayer() in Snowdrop.h accepts both but we
        // want strictly real players here.
        if (s.entityType != 1) continue;

        char name[64] = {};
        if (s.nameIsHeap)
        {
            if (!SafeReadCString(s.nameHeap, name, sizeof(name)))
                std::snprintf(name, sizeof(name), "<bad-heap>");
        }
        else
        {
            int j = 0;
            for (; j < 16 && s.nameInline[j]; ++j) name[j] = s.nameInline[j];
            name[j] = 0;
        }
        if (!name[0]) std::snprintf(name, sizeof(name), "<unnamed>");

        char line[160];
        std::snprintf(line, sizeof(line), "[%d] %s  (%s)",
                      (int)m_rows.size(),
                      name,
                      s.tag[0] ? s.tag : "no-tag");

        Row r{};
        r.agent = a;
        r.label = line;
        m_rows.push_back(std::move(r));
    }
}

void AgentInspector::DrawAgentDetails(TD::Agent* a)
{
    if (!LooksLikeHeapPtr(a))
    {
        ImGui::TextColored(ImVec4(1, 0.4f, 0.4f, 1), "Invalid agent pointer.");
        return;
    }

    AgentSnap s{};
    if (!SafeReadAgent(a, &s))
    {
        ImGui::TextColored(ImVec4(1, 0.4f, 0.4f, 1),
            "Read failed (access violation). Agent may have been freed — refresh.");
        return;
    }

    // Resolve name + GUID into local buffers (no C++ objects in __try).
    char name[128] = {};
    if (s.nameIsHeap) SafeReadCString(s.nameHeap, name, sizeof(name));
    else { std::memcpy(name, s.nameInline, 16); name[16] = 0; }

    char guidStr[128] = {};
    if (s.guidIsHeap)
    {
        if (!SafeReadCString(s.guidHeap, guidStr, sizeof(guidStr)))
            std::snprintf(guidStr, sizeof(guidStr), "<bad ptr 0x%p>", s.guidHeap);
    }
    else { std::memcpy(guidStr, s.guidInline, 16); guidStr[16] = 0; }

    // Distance from local player (Agent[0]) for quick "who's near me".
    float distance = -1.0f;
    bool  isLocal  = false;
    if (auto* rc = TD::RogueClient::Singleton())
    {
        if (rc->m_pClient && rc->m_pClient->m_pWorld
            && rc->m_pClient->m_pWorld->m_AgentCount > 0
            && rc->m_pClient->m_pWorld->m_AgentArray)
        {
            TD::Agent* local = rc->m_pClient->m_pWorld->m_AgentArray[0];
            if (local == a) isLocal = true;
            AgentSnap ls{};
            if (LooksLikeHeapPtr(local) && SafeReadAgent(local, &ls))
            {
                float dx = ls.posX - s.posX;
                float dy = ls.posY - s.posY;
                float dz = ls.posZ - s.posZ;
                distance = std::sqrt(dx * dx + dy * dy + dz * dz);
            }
        }
    }

    ImGui::TextColored(ImVec4(1, 0.8f, 0.2f, 1), "Identity %s",
                       isLocal ? "(LOCAL PLAYER)" : "");
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
    ImGui::Text("Entity type:   %d  (1=player)", s.entityType);
    ImGui::Text("Entity IDs:    %u / %u", s.entityId1, s.entityId2);
    ImGui::Text("CharGUID:      %s", guidStr);
    ImGui::Text("CharGUID hash: 0x%016llX %016llX",
                (unsigned long long)s.guidHash0,
                (unsigned long long)s.guidHash1);

    ImGui::Spacing();
    ImGui::TextColored(ImVec4(1, 0.8f, 0.2f, 1), "Status");
    ImGui::Separator();
    ImGui::Text("Dead:          %s", s.isDead  ? "YES" : "no");
    ImGui::Text("Rogue:         %s", s.isRogue ? "YES" : "no");
    ImGui::Text("DZ fallback:   A=%u  B=%u",
                (unsigned)s.dzFallbackA, (unsigned)s.dzFallbackB);

    ImGui::Spacing();
    ImGui::TextColored(ImVec4(1, 0.8f, 0.2f, 1), "Transform");
    ImGui::Separator();
    ImGui::Text("Position:      (%.2f, %.2f, %.2f)", s.posX, s.posY, s.posZ);
    ImGui::Text("Scale:         (%.3f, %.3f, %.3f)", s.scaleX, s.scaleY, s.scaleZ);
    if (!isLocal && distance >= 0)
        ImGui::Text("Distance:      %.2f m  (from local player)", distance);

    ImGui::Spacing();
    ImGui::TextColored(ImVec4(1, 0.8f, 0.2f, 1), "Components / Sub-objects");
    ImGui::Separator();
    ImGui::Text("ComponentSet*: 0x%p  (count %d)", s.compSet, s.compCount);
    ImGui::Text("SkinnedMesh*:  0x%p", s.skinnedMesh);

    ImGui::Spacing();
    ImGui::TextColored(ImVec4(1, 0.8f, 0.2f, 1), "Pointers");
    ImGui::Separator();
    ImGui::Text("World*:           0x%p", s.world);
    ImGui::Text("AppearanceMgr*:   0x%p", s.appearance);
    ImGui::Text("PlayerInventory*: 0x%p", s.inventory);

    ImGui::Spacing();
    DrawAgentStats(a);

    ImGui::Spacing();
    DrawAgentGear(a);
}

// ─── Equipped-gear enumeration ──────────────────────────────────────────────
// AppearanceManager (Agent+0x7C8) holds the per-slot rendered model paths.
// The clothing-slot region around AM+0x40..AM+0x400 contains qwords pointing
// to heap-allocated C-strings like "rogue/graph objects/gear/<name>.mgraphobject".
//
// Snowdrop.h documents a 0x28-stride ClothingSlot, but the actual live layout
// has paths at irregular qword offsets (some at slot+0x00, some at slot+0x08,
// and some slots reference shared "default" config blocks instead). Rather
// than fight the layout, we scan the AM region as raw qwords and accept any
// pointer whose target begins with "rogue/" — the canonical gear-path prefix.
// This is robust to layout drift and works for both local and remote agents
// (cosmetic model paths ARE network-replicated; that's how other clients
// know what to render).
namespace GearScan
{
    constexpr std::uintptr_t kAgent_AppearanceOff = 0x7C8;
    // Bounded scan range. AM is ~0x990 bytes; the clothing slots live in the
    // first ~0x400. Going further is cheap (SEH'd) but risks pulling in
    // non-gear paths (loaded shader bundles, mounted asset paths, etc.).
    constexpr std::size_t    kScanStart           = 0x40;
    constexpr std::size_t    kScanEnd             = 0x400;
    // Cap on distinct paths shown — defends against duplicates if the
    // same path is referenced multiple times in different slots.
    constexpr int            kMaxPaths            = 32;
}

// POD-only SEH check: does the target memory start with the literal "rogue/"?
static bool SafePeekRoguePrefix(std::uintptr_t target)
{
    __try
    {
        const std::uint8_t* p = reinterpret_cast<const std::uint8_t*>(target);
        return p[0] == 'r' && p[1] == 'o' && p[2] == 'g' && p[3] == 'u'
            && p[4] == 'e' && p[5] == '/';
    }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

// POD-only SEH walker: read AM, scan kScanStart..kScanEnd as qwords, and for
// each heap-looking qword whose target starts with "rogue/", copy up to
// 192 bytes of the path into the output buffer. Returns the number of
// distinct paths found (deduplicated by pointer value).
struct GearPath
{
    std::uintptr_t pathPtr;
    std::size_t    amOffset;
    char           text[192];
};

static int SafeScanGearPaths(TD::Agent* a, GearPath* out, int outCap)
{
    if (!a) return 0;
    __try
    {
        std::uintptr_t am = *(std::uintptr_t*)(
            reinterpret_cast<BYTE*>(a) + GearScan::kAgent_AppearanceOff);
        if (!LooksLikeHeapPtr(reinterpret_cast<void*>(am))) return 0;

        int n = 0;
        for (std::size_t off = GearScan::kScanStart;
             off < GearScan::kScanEnd && n < outCap;
             off += sizeof(std::uintptr_t))
        {
            std::uintptr_t ptr = *(std::uintptr_t*)(am + off);
            if (!LooksLikeHeapPtr(reinterpret_cast<void*>(ptr))) continue;
            if (!SafePeekRoguePrefix(ptr)) continue;

            // Dedup against entries already collected (same pointer can
            // appear in multiple slot fields).
            bool dup = false;
            for (int i = 0; i < n; ++i)
                if (out[i].pathPtr == ptr) { dup = true; break; }
            if (dup) continue;

            out[n].pathPtr  = ptr;
            out[n].amOffset = off;
            const char* s   = reinterpret_cast<const char*>(ptr);
            int i = 0;
            for (; i < (int)sizeof(out[n].text) - 1; ++i)
            {
                unsigned char c = static_cast<unsigned char>(s[i]);
                if (c == 0) break;
                if (c < 0x20 || c > 0x7E) c = '?';
                out[n].text[i] = static_cast<char>(c);
            }
            out[n].text[i] = 0;
            ++n;
        }
        return n;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
}

void AgentInspector::DrawAgentGear(TD::Agent* a)
{
    ImGui::TextColored(ImVec4(1, 0.8f, 0.2f, 1), "Equipped gear (cosmetic models)");
    ImGui::Separator();

    static std::uintptr_t s_scannedAgent = 0;
    static int            s_count        = 0;
    static GearPath       s_paths[GearScan::kMaxPaths] = {};

    if (ImGui::Button("Scan gear paths") ||
        s_scannedAgent != reinterpret_cast<std::uintptr_t>(a))
    {
        s_count = SafeScanGearPaths(a, s_paths, GearScan::kMaxPaths);
        s_scannedAgent = reinterpret_cast<std::uintptr_t>(a);
    }
    ImGui::SameLine();
    ImGui::Text("(%d path%s)", s_count, s_count == 1 ? "" : "s");

    if (s_count == 0)
    {
        ImGui::TextDisabled(
            "No gear paths found. AppearanceManager may not be populated yet\n"
            "for this agent (remote players stream in gradually).");
        return;
    }

    ImGui::BeginChild("##gear", ImVec2(0, 220), true);
    ImGui::Columns(2, "##gearcols", false);
    ImGui::SetColumnWidth(0, 80);
    ImGui::Text("AM+off");  ImGui::NextColumn();
    ImGui::Text("Path");    ImGui::NextColumn();
    ImGui::Separator();
    for (int i = 0; i < s_count; ++i)
    {
        ImGui::Text("0x%03X", (unsigned)s_paths[i].amOffset);
        ImGui::NextColumn();
        ImGui::TextWrapped("%s", s_paths[i].text);
        ImGui::NextColumn();
    }
    ImGui::Columns(1);
    ImGui::EndChild();
}

// ─── Stats chain ────────────────────────────────────────────────────────────
// Pointer-walk verified live 2026-05-14. Both engine functions sub_55C890
// (lazy cached getter) and sub_5A7670 (invalidator) resolve a stat slot via:
//
//   slot = *(QWORD)(owner + 0x78) + (stat_id << 6);
//
// The owner is reached from Agent+0x4E0 → +0x130. The slot is 0x40 bytes;
// the current value lives at slot+0x04 (and is mirrored to slot+0x2C).
//
// Known stat_id: 657 = Stamina (verified on both a remote agent and the
// local player). Firearms / Electronics are NOT in this stat system — the
// string "Firearms" doesn't exist anywhere in TheDivision.exe, and CE
// pointer-scans for F/E values come up empty for remote players. F/E live
// in a separate (local-only) loadout/character-progression struct that
// hasn't been mapped yet. The "Stamina" string is registered in
// sub_1601F60 alongside movement-state fields (Position/Stance/Aim/Speed),
// so this stat track is an agent-state Stamina that the gear-stat feeds
// into. The populated-slot scan below lists any other populated slots so
// they can be mapped empirically by changing gear and watching which moves.
namespace StatChain
{
    constexpr std::uintptr_t kAgent_StatsMgrOff    = 0x4E0;   // Agent → StatsManager*
    constexpr std::uintptr_t kStatsMgr_OwnerOff    = 0x130;   // StatsManager → StatTrackOwner*
    constexpr std::uintptr_t kOwner_ArrayPtrOff    = 0x078;   // Owner → stat_array_base
    constexpr std::size_t    kSlotStride           = 0x40;
    constexpr int            kStatId_Stamina       = 657;

    // We cap the populated-slot scan at this many entries. The arrays we've
    // observed extend well past id=657, but every slot past the last populated
    // one is a default-init template (flag=1, value=FLT_MAX) so we don't
    // gain anything by scanning further. 1024 is a safety ceiling.
    constexpr int            kMaxSlotScan          = 1024;
}

struct StatSlot
{
    int   id;
    float value;
};

// POD-only SEH walker: Agent → (Agent+0x4E0) → (+0x130) → (+0x78). Returns
// the array base or 0 on any deref failure or non-heap-looking pointer.
// Output owner_out lets the UI display the resolved struct address.
static std::uintptr_t SafeResolveStatArray(TD::Agent* a, std::uintptr_t* owner_out)
{
    *owner_out = 0;
    if (!a) return 0;
    __try
    {
        BYTE* p = reinterpret_cast<BYTE*>(a);
        std::uintptr_t sm = *(std::uintptr_t*)(p + StatChain::kAgent_StatsMgrOff);
        if (!LooksLikeHeapPtr(reinterpret_cast<void*>(sm))) return 0;
        std::uintptr_t owner = *(std::uintptr_t*)(sm + StatChain::kStatsMgr_OwnerOff);
        if (!LooksLikeHeapPtr(reinterpret_cast<void*>(owner))) return 0;
        std::uintptr_t base  = *(std::uintptr_t*)(owner + StatChain::kOwner_ArrayPtrOff);
        if (!LooksLikeHeapPtr(reinterpret_cast<void*>(base))) return 0;
        *owner_out = owner;
        return base;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
}

// POD-only SEH read of a single slot's value field. Returns true on success.
// A slot is considered "populated" if its +0x00 flag is 0 (the engine sets
// it to 1 for default-init slots; the value at +0x04 is FLT_MAX in that
// case). The flag-0 check is cheaper than a NaN/FLT_MAX test on the float.
static bool SafeReadSlot(std::uintptr_t arrayBase, int id,
                         float* outValue, bool* outPopulated)
{
    *outValue = 0.0f;
    *outPopulated = false;
    __try
    {
        std::uintptr_t slot = arrayBase + static_cast<std::uintptr_t>(id) * StatChain::kSlotStride;
        std::uint8_t flag  = *(std::uint8_t*)(slot + 0x00);
        float        val   = *(float*)(slot + 0x04);
        *outValue     = val;
        *outPopulated = (flag == 0);   // flag==1 is default-init
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

// POD-only bulk scan. Walks id 0..kMaxSlotScan and copies up to kMax
// populated entries into out_buf. Returns the number written. Stops early
// if a read throws (treats it as "end of valid array").
static int SafeScanPopulated(std::uintptr_t arrayBase,
                             StatSlot* out_buf, int outCap)
{
    int written = 0;
    __try
    {
        BYTE* base = reinterpret_cast<BYTE*>(arrayBase);
        for (int id = 0; id < StatChain::kMaxSlotScan && written < outCap; ++id)
        {
            BYTE* slot = base + static_cast<std::size_t>(id) * StatChain::kSlotStride;
            std::uint8_t flag = *(std::uint8_t*)(slot + 0x00);
            float        val  = *(float*)(slot + 0x04);
            if (flag == 0)
            {
                out_buf[written].id    = id;
                out_buf[written].value = val;
                ++written;
            }
        }
        return written;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) { return written; }
}

void AgentInspector::DrawAgentStats(TD::Agent* a)
{
    ImGui::TextColored(ImVec4(1, 0.8f, 0.2f, 1), "Stats");
    ImGui::Separator();

    std::uintptr_t owner = 0;
    std::uintptr_t base  = SafeResolveStatArray(a, &owner);
    if (!base)
    {
        ImGui::TextDisabled(
            "Stat chain unresolved (Agent+0x4E0 / +0x130 / +0x78 not valid).\n"
            "Remote agents that haven't fully streamed in can show this.");
        return;
    }

    ImGui::Text("Owner:        0x%p", reinterpret_cast<void*>(owner));
    ImGui::Text("Array base:   0x%p", reinterpret_cast<void*>(base));

    // Known: stat_id 657 = Stamina (verified live).
    float staminaVal = 0.0f;
    bool  staminaOk  = false;
    SafeReadSlot(base, StatChain::kStatId_Stamina, &staminaVal, &staminaOk);
    ImGui::Spacing();
    if (staminaOk)
        ImGui::Text("Stamina [657]:  %.2f", staminaVal);
    else
        ImGui::TextDisabled("Stamina [657]:  <not populated>");

    // Full populated-slot list — lets the user identify firearms / electronics
    // (and any other interesting slot) by ID empirically. Static cache so we
    // don't re-scan every frame; refresh via the button.
    static std::uintptr_t s_scannedAgent = 0;
    static int            s_count        = 0;
    static StatSlot       s_slots[64]    = {};

    if (ImGui::Button("Scan populated slots") ||
        s_scannedAgent != reinterpret_cast<std::uintptr_t>(a))
    {
        s_count = SafeScanPopulated(base, s_slots,
                                    sizeof(s_slots) / sizeof(s_slots[0]));
        s_scannedAgent = reinterpret_cast<std::uintptr_t>(a);
    }
    ImGui::SameLine();
    ImGui::Text("(%d populated)", s_count);

    if (s_count > 0)
    {
        ImGui::BeginChild("##stats", ImVec2(0, 180), true);
        ImGui::Columns(3, "##statcols", false);
        ImGui::SetColumnWidth(0, 70);
        ImGui::SetColumnWidth(1, 120);
        ImGui::Text("ID");        ImGui::NextColumn();
        ImGui::Text("Value");     ImGui::NextColumn();
        ImGui::Text("Label");     ImGui::NextColumn();
        ImGui::Separator();
        for (int i = 0; i < s_count; ++i)
        {
            ImGui::Text("%d", s_slots[i].id);                  ImGui::NextColumn();
            ImGui::Text("%.3f", s_slots[i].value);             ImGui::NextColumn();
            if (s_slots[i].id == StatChain::kStatId_Stamina)
                ImGui::TextColored(ImVec4(0.6f, 1.0f, 0.6f, 1), "Stamina");
            else
                ImGui::TextDisabled("(unmapped)");
            ImGui::NextColumn();
        }
        ImGui::Columns(1);
        ImGui::EndChild();
    }
}

void AgentInspector::DrawUI()
{
    if (!m_loadedOnce)
    {
        RefreshPlayerList();
        m_loadedOnce = true;
    }

    if (ImGui::Button("Refresh"))
        RefreshPlayerList();
    ImGui::SameLine();
    ImGui::Text("%zu player(s) found", m_rows.size());

    ImGui::Separator();

    ImGui::BeginChild("##agentlist", ImVec2(340, 380), true);
    for (int i = 0; i < (int)m_rows.size(); ++i)
    {
        const Row& r = m_rows[i];
        if (ImGui::Selectable(r.label.c_str(), m_selectedIndex == i))
            m_selectedIndex = i;
    }
    if (m_rows.empty())
        ImGui::TextDisabled("(no players — click Refresh after entering the world)");
    ImGui::EndChild();

    ImGui::SameLine();

    ImGui::BeginChild("##agentdetail", ImVec2(0, 380), true);
    if (m_selectedIndex >= 0 && m_selectedIndex < (int)m_rows.size())
        DrawAgentDetails(m_rows[m_selectedIndex].agent);
    else
        ImGui::TextDisabled("Select a player to inspect.");
    ImGui::EndChild();
}

#endif // QOL_ENABLE_INSPECTORS
