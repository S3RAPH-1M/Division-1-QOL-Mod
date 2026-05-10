#include "SkinnedMeshManager.h"
#include "Snowdrop.h"
#include "Main.h"
#include "imgui/imgui.h"

#include <Windows.h>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <iostream>

// ─── curated model lists (display name + asset path) ─────────────────────────
// Backpack list seeded from the user's prior table; extend others as you
// catalog more assets. The UI also exposes a free-text custom path field per
// slot, so the list is just convenience.

static const SkinnedMeshManager::ModelSwapEntry s_backpackModels_[] =
{
    { "Ninjabike Messenger Bag",         "rogue/graph objects/gear/ca_cm_b_sv_set01.mgraphobject" },
    { "Striker's Battlegear",            "rogue/graph objects/gear/CA_CM_B_T7_R_DLC1.mgraphobject" },
    { "Striker's Battlegear Classified", "rogue/graph objects/gear/ca_cm_b_mm_st.mgraphobject" },
    { "Predator's Mark Classified",      "rogue/graph objects/gear/ca_cm_b_pa_pr.mgraphobject" },
    { "Hunters Faith Classified",        "rogue/graph objects/gear/ca_cm_b_rt_hf.mgraphobject" },
    { "Nomad Classified",                "rogue/graph objects/gear/ca_cm_b_uc_pn.mgraphobject" },
    { "D3-FNC Classified",               "rogue/graph objects/gear/ca_cm_b_pa_d3.mgraphobject" },
    { "Lone Star Classified",            "rogue/graph objects/gear/ca_cm_b_tt_ls.mgraphobject" },
    { "Lone Star",                       "rogue/graph objects/gear/CA_CM_B_Set03_BG.mgraphobject" },
    { "Banshee Classified",              "rogue/graph objects/gear/ca_cm_b_pa_ba.mgraphobject" },
    { "Banshee",                         "rogue/graph objects/gear/ca_cm_b_uw_dar.mgraphobject" },
    { "DeadEYE Classified",              "rogue/graph objects/gear/ca_cm_b_tt_de.mgraphobject" },
    { "Sentry Call Classified",          "rogue/graph objects/gear/ca_cm_b_as_sc.mgraphobject" },
    { "Alphabridge Classified",          "rogue/graph objects/gear/ca_cm_b_rt_ab.mgraphobject" },
    { "Reclaimer Classified",            "rogue/graph objects/gear/ca_cm_b_tt_rc.mgraphobject" },
    { "FireCrest Classified",            "rogue/graph objects/gear/ca_cm_b_rt_fc.mgraphobject" },
    { "FireCrest",                       "rogue/graph objects/gear/CA_CM_B_GS_UW.mgraphobject" },
    { "Spec-ops pack",                   "rogue/graph objects/gear/CA_CM_B_T7_L.mgraphobject" },
    { "Urban assault pack",              "rogue/graph objects/gear/CA_CM_B_T7_E.mgraphobject" },
    { "Security pack",                   "rogue/graph objects/gear/CA_CM_B_T1_R.mgraphobject" },
    { "Safety bag",                      "rogue/graph objects/gear/CA_CM_B_T1_U.mgraphobject" },
};

// Stub lists — populate the arrays below as you catalog paths per slot.
// The UI also exposes a free-text custom-path field per slot, so an empty list
// just means "no curated entries" — the swap still works via the textbox.
//
// To add entries for, say, hats:
//   static const SkinnedMeshManager::ModelSwapEntry s_hatEntries[] = {
//       { "Bandana",  "rogue/graph objects/gear/ca_cm_h_..." },
//       ...
//   };
//   then change s_hatModels to point at s_hatEntries and set its count.

struct ModelList
{
    const SkinnedMeshManager::ModelSwapEntry* entries;
    int count;
};

static const ModelList s_backpackModels   = { s_backpackModels_, (int)(sizeof(s_backpackModels_) / sizeof(s_backpackModels_[0])) };
static const ModelList s_hatModels        = { nullptr, 0 };
static const ModelList s_glovesModels     = { nullptr, 0 };
static const ModelList s_jacketModels     = { nullptr, 0 };
static const ModelList s_shirtModels      = { nullptr, 0 };
static const ModelList s_chestplateModels = { nullptr, 0 };
static const ModelList s_pantsModels      = { nullptr, 0 };
static const ModelList s_thighModels      = { nullptr, 0 };
static const ModelList s_feetModels       = { nullptr, 0 };
static const ModelList s_scarfModels      = { nullptr, 0 };
static const ModelList s_kneepadsModels   = { nullptr, 0 };
static const ModelList s_gasMaskModels    = { nullptr, 0 };

// ─── lifecycle ────────────────────────────────────────────────────────────────

SkinnedMeshManager::SkinnedMeshManager() = default;
SkinnedMeshManager::~SkinnedMeshManager() = default;

// ─── per-slot mod tracking (for soft-replacement / auto-revert) ──────────────

namespace
{
    // Soft-replacement tracking. Apply mutates engine state (writes path,
    // inserts a bucket, pokes m_DirtyFlag). After consumption settles a frame
    // or two later, we snapshot m_AttachHashmap_Count and m_AssetRecords_Count
    // as the "stable baseline." Any subsequent change to either count means
    // the engine has done something on its own (user equipped / modified
    // anything), so we proactively undo our mutations: remove our injected
    // AttachBucket via the engine's own hashmap_remove, and walk m_AssetRecords
    // to drop any Item* whose asset paths still reference our mod path.
    struct SlotModState
    {
        bool         active           = false;
        std::string  modPath;
        int          settleFrames     = 0;     // wait a few frames for engine consumption
        int          baselineHashCt   = -1;
        int          baselineRecordCt = -1;
    };
    static SlotModState s_modState[27];

    constexpr int kSettleFramesBeforeBaseline = 4;

    // Auto-reapply state. g_autoReapply gates the whole feature; s_lastApplied
    // stores the most-recently-applied mod path per slot so we can re-do the
    // Apply if the engine reverts our mutation. Throttled to ~0.5s with a
    // single global timer (one check sweeps all slots).
    static bool        g_autoReapply       = false;
    static std::string s_lastApplied[27];
    static double      s_lastReapplyCheck  = 0.0;
}

void SkinnedMeshManager::Update()
{
    ScanLiveSlots();
    SoftRevertOnEngineActivity();
    AutoReapplyOnDrift();
}

// SoftRevertOnEngineActivity is defined after GetPlayerAppearance and the
// POD-helper namespace below — it depends on both, so the body has to live
// further down in the translation unit.

// ─── path classification ──────────────────────────────────────────────────────

static bool ContainsCI(const char* hay, const char* needle)
{
    if (!hay || !needle) return false;
    for (; *hay; ++hay)
    {
        const char* h = hay;
        const char* n = needle;
        while (*n && *h && std::tolower((unsigned char)*h) == std::tolower((unsigned char)*n))
            ++h, ++n;
        if (!*n) return true;
    }
    return false;
}

SkinnedMeshManager::GearType SkinnedMeshManager::ClassifyPath(const char* path)
{
    if (!path || !*path) return GearType::Unknown;

    // Special-case prefixes that don't follow ca_<g>_<type> pattern.
    if (ContainsCI(path, "ch_pm_mask")) return GearType::GasMask;
    if (ContainsCI(path, "/ca_hg_") || ContainsCI(path, "/cp_hg_")) return GearType::GasMask;

    // Need at least "ca_<g>_<type>" — extract everything after the
    // gender token. The prefix from ExtractGearPrefix is e.g. "ca",
    // so we need the next-next token.
    const char* gear = std::strstr(path, "gear/");
    const char* base = gear ? gear + 5 : path;

    // tokens[0] = "ca", tokens[1] = "cm"/"cf", tokens[2] = body type letter(s)
    char tok[3][16] = {};
    int t = 0;
    int i = 0;
    while (*base && t < 3)
    {
        if (*base == '_' || *base == '.')
        {
            tok[t][i] = '\0';
            ++t;
            i = 0;
            if (*base == '.') break;
        }
        else if (i + 1 < (int)sizeof(tok[t]))
        {
            tok[t][i++] = (char)std::tolower((unsigned char)*base);
        }
        ++base;
    }
    if (t < 3) return GearType::Unknown;

    const char* type = tok[2];

    if (std::strcmp(type, "b")  == 0) return GearType::Backpack;
    if (std::strcmp(type, "l1") == 0) return GearType::Shirt;
    if (std::strcmp(type, "l2") == 0) return GearType::Chestplate;
    if (std::strcmp(type, "l3") == 0) return GearType::Jacket;
    if (std::strcmp(type, "p")  == 0) return GearType::Pants;
    if (std::strcmp(type, "t")  == 0) return GearType::Thigh;
    if (std::strcmp(type, "f")  == 0) return GearType::Feet;
    if (std::strcmp(type, "s")  == 0) return GearType::Scarf;
    if (std::strcmp(type, "k")  == 0) return GearType::Kneepads;
    if (std::strcmp(type, "h")  == 0)
    {
        // Hand/glove sub-classification: paths like "ca_cm_h_gv_st" or names containing "gloves"
        if (ContainsCI(path, "_gv_") || ContainsCI(path, "gloves")) return GearType::Gloves;
        return GearType::Hat;
    }
    if (std::strcmp(type, "hg") == 0) return GearType::GasMask;

    return GearType::Unknown;
}

const char* SkinnedMeshManager::GearTypeName(GearType t)
{
    switch (t)
    {
    case GearType::Backpack:   return "Backpack";
    case GearType::Shirt:      return "Shirt (L1)";
    case GearType::Chestplate: return "Chestplate (L2)";
    case GearType::Jacket:     return "Jacket (L3)";
    case GearType::Pants:      return "Pants";
    case GearType::Thigh:      return "Thigh Holster";
    case GearType::Feet:       return "Shoes / Boots";
    case GearType::Scarf:      return "Scarf";
    case GearType::Kneepads:   return "Kneepads";
    case GearType::Hat:        return "Hat";
    case GearType::Gloves:     return "Gloves";
    case GearType::GasMask:    return "Gas Mask";
    default:                   return "Unknown";
    }
}

// ─── POD-only helpers (SEH allowed; no C++ destructors in scope) ────────────

// Forward declaration: FindPlayerAgent is defined further down at file scope,
// but the GatherDiagInfo helper inside the anonymous namespace below needs to
// see it. Static is fine — same translation unit.
static TD::Agent* FindPlayerAgent(TD::World* world, int* outFoundIdx);

namespace
{
    struct RawSlotInfo
    {
        bool          valid;
        bool          isHeap;
        std::uint32_t cap;
        char          path[260];   // copied out of game memory before C++ string handling
    };

    // Reads all 27 m_Clothes entries into a POD array. Returns false if the read
    // hit an access violation (player likely despawning) — caller treats as scan failure.
    //
    // NOTE: A slot is considered "populated" if it has a non-empty m_Path. We do
    // NOT require slot.m_pSlot to be non-null — during in-game equipment /
    // customization menus the engine briefly clears m_pSlot but the cached path
    // stays intact, and we want the UI (and override drift detection) to keep
    // working through that window.
    bool ReadAllSlotsGuarded(TD::AppearanceManager* am, RawSlotInfo* out)
    {
        std::memset(out, 0, sizeof(RawSlotInfo) * 27);
        __try
        {
            for (int i = 0; i < 27; ++i)
            {
                const auto& slot = am->m_Clothes[i];

                const BYTE* b = slot.m_Path.bytes;
                bool isHeap = b[0x0F] != 0;
                out[i].isHeap = isHeap;

                if (isHeap)
                {
                    const char* heap = *(const char* const*)b;
                    if (!heap) continue;
                    std::uint32_t cap = *(const std::uint32_t*)(heap - 4);
                    if (cap == 0 || cap > 0x1000) continue;
                    std::size_t copy = (cap < sizeof(out[i].path) - 1) ? cap : sizeof(out[i].path) - 1;
                    std::memcpy(out[i].path, heap, copy);
                    out[i].path[copy] = '\0';
                    out[i].cap   = cap;
                    // Mark as valid/mutatable as long as we have a heap allocation,
                    // even if the engine has NUL'd the first byte (Character_ApplyClothingId
                    // does that to "clear" a slot — the allocation survives so we can
                    // write our own path back into it).
                    out[i].valid = true;
                }
                else
                {
                    std::memcpy(out[i].path, b, 15);
                    out[i].path[15] = '\0';
                    out[i].cap   = 0;
                    out[i].valid = (out[i].path[0] != '\0');
                }
            }
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            return false;
        }
        return true;
    }

    // VirtualProtect + memcpy + memset, guarded against AV. Restores page
    // protection in all paths. Returns true on a successful copy.
    bool GuardedHeapWrite(char* dst, const char* src, std::size_t srcLen, std::uint32_t capacity)
    {
        DWORD oldProt = 0;
        if (!VirtualProtect(dst, capacity, PAGE_READWRITE, &oldProt)) return false;
        bool ok = true;
        __try
        {
            std::memcpy(dst, src, srcLen + 1);
            if (srcLen + 1 < capacity)
                std::memset(dst + srcLen + 1, 0, capacity - srcLen - 1);
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            ok = false;
        }
        DWORD tmp;
        VirtualProtect(dst, capacity, oldProt, &tmp);
        return ok;
    }

    // Walks m_AttachHashmap_Buckets and rewrites m_ClothingId on every bucket
    // matching slotId, replacing it with a sentinel value that the engine's
    // per-slot processing won't match. Used to suppress the original bag's
    // bucket before we insert our own — without this both Item*s end up in
    // m_AssetRecords and both meshes render ("two bags" symptom).
    //
    // We deliberately do NOT mutate m_ModelPath or remove the bucket from the
    // hashmap. m_ClothingId is at offset 0x3C, outside the hashmap key, so the
    // entry's hash stays valid and later lookups / inserts / Character_ApplyClothingId
    // removals still operate correctly. The disqualified bucket leaks (lives on
    // until character despawn), which is acceptable.
    constexpr std::uint32_t kClothingIdSuppressed = 0xFFFFFFFFu;
    int DisqualifyBucketsForSlot(TD::AppearanceManager* am, int slotId)
    {
        int n = 0;
        __try
        {
            auto* buckets = am->m_AttachHashmap_Buckets;
            int count = am->m_AttachHashmap_Count;
            if (!buckets || count <= 0) return 0;

            for (int i = 0; i < count; ++i)
            {
                if (buckets[i].m_ClothingId == (std::uint32_t)slotId)
                {
                    buckets[i].m_ClothingId = kClothingIdSuppressed;
                    ++n;
                }
            }
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            // return whatever we got before the AV
        }
        return n;
    }

    // Reads a SnowdropString at `sstr` (16 bytes) into outPath. Heap-mode
    // dereferences the pointer at +0; inline-mode reads the bytes directly.
    bool ReadSnowdropStringAt(const BYTE* sstr, char* outPath, std::size_t outSize)
    {
        if (!sstr || !outPath || outSize == 0) return false;
        outPath[0] = '\0';
        __try
        {
            const char* path = nullptr;
            if (sstr[0x0F] == 0)
            {
                path = (const char*)sstr;
            }
            else
            {
                path = *(const char* const*)sstr;
                if (!path) return false;
            }
            std::size_t i = 0;
            while (i + 1 < outSize && path[i]) { outPath[i] = path[i]; ++i; }
            outPath[i] = '\0';
            return outPath[0] != '\0';
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            return false;
        }
    }

    // Item base class has its asset paths spread across 8 explicit
    // SnowdropStrings (+24, +128, +368, +384, +400, +416, +600, +616) plus 3
    // AgentAssetRef structs whose first SnowdropString lives at +192, +256, +320
    // (AgentAssetRef base offset + 16). Different subclass tags populate
    // different fields, so we have to scan every candidate location.
    bool ItemContainsPath(void* itemPtr, const char* targetPath)
    {
        if (!itemPtr || !targetPath) return false;
        static const int kPathOffsets[] = {
             24, 128, 368, 384, 400, 416, 600, 616,
            192, 216, 256, 280, 320, 344,
        };
        char path[260];
        for (int off : kPathOffsets)
        {
            if (!ReadSnowdropStringAt((const BYTE*)itemPtr + off, path, sizeof(path)))
                continue;
            if (_stricmp(path, targetPath) == 0) return true;
        }
        return false;
    }

    // Walks m_AssetRecords looking for an Item* containing targetPath in any of
    // its candidate path slots. Returns the index, or -1 if not found.
    int FindAssetRecordByPath(TD::AppearanceManager* am, const char* targetPath)
    {
        if (!am || !targetPath) return -1;
        __try
        {
            void** arr = am->m_AssetRecords_Ptr;
            int count = am->m_AssetRecords_Count;
            if (!arr || count <= 0) return -1;

            for (int i = 0; i < count; ++i)
                if (ItemContainsPath(arr[i], targetPath)) return i;
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {}
        return -1;
    }

    // Returns the index of `itemPtr` in m_AssetRecords, or -1 if not present.
    int FindAssetRecordByPtr(TD::AppearanceManager* am, void* itemPtr)
    {
        if (!am || !itemPtr) return -1;
        __try
        {
            void** arr = am->m_AssetRecords_Ptr;
            int count = am->m_AssetRecords_Count;
            if (!arr || count <= 0) return -1;
            for (int i = 0; i < count; ++i)
                if (arr[i] == itemPtr) return i;
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {}
        return -1;
    }

    // Removes m_AssetRecords[index] by shifting subsequent entries down and
    // decrementing the count. Does NOT call the Item dtor — the Item* is left
    // unreferenced (small leak), which is far safer than guessing the engine's
    // ref-counting / virtual destructor convention from outside.
    bool RemoveAssetRecordAt(TD::AppearanceManager* am, int index)
    {
        if (!am || index < 0) return false;
        __try
        {
            void** arr = am->m_AssetRecords_Ptr;
            int count = am->m_AssetRecords_Count;
            if (!arr || index >= count) return false;

            for (int i = index; i < count - 1; ++i)
                arr[i] = arr[i + 1];
            arr[count - 1] = nullptr;
            am->m_AssetRecords_Count = count - 1;
            return true;
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            return false;
        }
    }

    // POD helper: read m_AttachHashmap_Count and m_AssetRecords_Count under
    // a single SEH guard. Lives in this anonymous namespace so the C++-aware
    // SoftRevertOnEngineActivity (which holds std::string state) can call it
    // without tripping MSVC C2712.
    struct CountSnapshot
    {
        bool ok;
        int  hashCt;
        int  recordCt;
    };

    CountSnapshot ReadCountsGuarded(TD::AppearanceManager* am)
    {
        CountSnapshot s{};
        s.ok = false;
        if (!am) return s;
        __try
        {
            s.hashCt   = am->m_AttachHashmap_Count;
            s.recordCt = am->m_AssetRecords_Count;
            s.ok       = true;
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            s.ok = false;
        }
        return s;
    }

    // Engine's AttachHashmap remove (sub_1650620). Cleanly drops the bucket
    // whose m_ModelPath equals `path`. Used in SoftRevert to undo our injected
    // bucket once we detect any engine activity. Verified via decompile of
    // sub_16679B0 — that's exactly how Character_ApplyClothingId removes
    // matching buckets internally.
    bool CallHashmapRemoveGuarded(TD::AppearanceManager* am, const char* path)
    {
        typedef __int64 (__fastcall *PFN)(void* hashmap, const char* path);
        PFN fn = (PFN)(g_pBase + 0x1650620);
        __try
        {
            fn((void*)((__int64)am + 0x18), path);
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            return false;
        }
        return true;
    }

    // Engine's per-slot reset / "apply clothing id" (sub_16679B0). Removes
    // stale AttachBuckets where m_ClothingId == id and ref-drops any Item*
    // currently bound to that slot in m_AssetRecords. Used as a clean reset
    // before we install our own bucket — without it, the original bag's
    // Item* stays in m_AssetRecords and renders alongside ours ("two bags").
    bool CallApplyClothingIdGuarded(TD::AppearanceManager* am, std::uint32_t slotId)
    {
        typedef __int64 (__fastcall *PFN)(TD::AppearanceManager*, std::uint32_t*);
        PFN fn = (PFN)(g_pBase + 0x16679B0);
        __try
        {
            fn(am, &slotId);
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            return false;
        }
        return true;
    }

    // Engine's "model load trigger" (sub_162FDA0). Properly inserts a new
    // AttachBucket into m_AttachHashmap via sub_1544E60, with m_SlotName /
    // m_ClothingId filled in. Using the engine's insert keeps the hashmap key
    // hash and the entry array consistent (in-place mutation of an existing
    // bucket's m_ModelPath corrupts the key hash and crashes the in-game UI
    // when it later tries to remove or look up the bucket).
    bool CallModelLoadTriggerGuarded(TD::AppearanceManager* am,
                                     TD::SnowdropString* path,
                                     std::uint32_t slotId)
    {
        typedef __int64 (__fastcall *PFN)(TD::AppearanceManager*, TD::SnowdropString*, std::uint32_t*);
        PFN fn = (PFN)(g_pBase + 0x162FDA0);
        __try
        {
            fn(am, path, &slotId);
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            return false;
        }
        return true;
    }

    // Engine's SnowdropString assign function (sub_116830). Reallocates the
    // string's heap buffer if the new content is longer than the existing
    // capacity, using the engine's own allocator — which is the only safe way
    // to grow a SnowdropString without a free-time allocator-mismatch crash.
    // Used as the fallback when the new path doesn't fit in the current cap.
    bool CallStringAssignGuarded(TD::SnowdropString* str, const char* newPath)
    {
        typedef void (__fastcall *PFN)(TD::SnowdropString*, const char*);
        PFN fn = (PFN)(g_pBase + 0x116830);
        __try
        {
            fn(str, newPath);
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            return false;
        }
        return true;
    }

    // POD-only diagnostic snapshot. SEH-guarded reads of agent fields live here
    // so DrawUI doesn't have to host __try (it has C++ destructors all over).
    struct DiagInfo
    {
        int                    agentCount;
        int                    playerIdx;
        TD::Agent*             player;
        int                    playerType;
        TD::AppearanceManager* am;
    };

    void GatherDiagInfo(DiagInfo* out)
    {
        out->agentCount = 0;
        out->playerIdx  = -1;
        out->player     = nullptr;
        out->playerType = -1;
        out->am         = nullptr;

        auto* rc = TD::RogueClient::Singleton();
        if (!rc) return;
        auto* client = rc->m_pClient;
        if (!client) return;
        auto* world = client->m_pWorld;
        if (!world || !world->m_AgentArray) return;

        out->agentCount = world->m_AgentCount;
        out->player     = FindPlayerAgent(world, &out->playerIdx);
        if (!out->player) return;

        __try { out->playerType = *(int*)((__int64)out->player + 0x3A4); }
        __except (EXCEPTION_EXECUTE_HANDLER) { out->playerType = -2; }

        out->am = out->player->m_pAppearance;
    }

}

// ─── live slot scanning ──────────────────────────────────────────────────────

// Locates the player Agent by walking m_AgentArray and matching EntityType==1.
// During in-game customization/equipment menus the engine can shuffle agent
// indices (or temporarily despawn the player), so we can't rely on Agent[0].
// Falls back to Agent[0] only if no EntityType==1 agent is found.
static TD::Agent* FindPlayerAgent(TD::World* world, int* outFoundIdx)
{
    if (outFoundIdx) *outFoundIdx = -1;
    if (!world || !world->m_AgentArray || world->m_AgentCount <= 0) return nullptr;

    for (int i = 0; i < world->m_AgentCount; ++i)
    {
        auto* a = world->m_AgentArray[i];
        if (!a) continue;
        int type = 0;
        __try { type = *(int*)((__int64)a + 0x3A4); }
        __except (EXCEPTION_EXECUTE_HANDLER) { continue; }
        if (type == 1) { if (outFoundIdx) *outFoundIdx = i; return a; }
    }
    // Fallback — slot 0 even if not type 1 (caller decides if they want it)
    return world->m_AgentArray[0];
}

static TD::AppearanceManager* GetPlayerAppearance(std::string* errOut)
{
    auto* rc = TD::RogueClient::Singleton();
    if (!rc)     { if (errOut) *errOut = "RogueClient null";         return nullptr; }
    auto* client = rc->m_pClient;
    if (!client) { if (errOut) *errOut = "Client null";              return nullptr; }
    auto* world  = client->m_pWorld;
    if (!world)  { if (errOut) *errOut = "World null";               return nullptr; }
    if (!world->m_AgentArray || world->m_AgentCount <= 0)
                 { if (errOut) *errOut = "agent array empty";        return nullptr; }

    int playerIdx = -1;
    TD::Agent* player = FindPlayerAgent(world, &playerIdx);
    if (!player) { if (errOut) *errOut = "no agents in array";       return nullptr; }
    int type = 0;
    __try { type = *(int*)((__int64)player + 0x3A4); }
    __except (EXCEPTION_EXECUTE_HANDLER) { type = 0; }

    if (type != 1)
    {
        if (errOut)
        {
            char buf[160];
            std::snprintf(buf, sizeof(buf),
                          "no player (type=1) found in %d agents (agent[0].type=%d)",
                          world->m_AgentCount, type);
            *errOut = buf;
        }
        return nullptr;
    }

    auto* am = player->m_pAppearance;
    if (!am)     { if (errOut) *errOut = "AppearanceManager null";   return nullptr; }
    return am;
}

void SkinnedMeshManager::SoftRevertOnEngineActivity()
{
    auto* am = GetPlayerAppearance(nullptr);
    if (!am) return;

    for (int slotIdx = 0; slotIdx < 27; ++slotIdx)
    {
        auto& st = s_modState[slotIdx];
        if (!st.active) continue;

        // Read current counts via the POD helper (SEH lives there — this
        // function holds a std::string in s_modState so it can't host __try).
        CountSnapshot snap = ReadCountsGuarded(am);
        if (!snap.ok) continue;
        int curHashCt   = snap.hashCt;
        int curRecordCt = snap.recordCt;

        // Wait kSettleFramesBeforeBaseline frames for the engine to consume
        // the m_DirtyFlag we set in Apply. Once consumption is done, the
        // hashmap and asset-records counts reflect "our mod is installed,
        // engine is at rest." Snapshot those as the baseline.
        if (st.baselineHashCt < 0)
        {
            if (++st.settleFrames < kSettleFramesBeforeBaseline) continue;
            st.baselineHashCt   = curHashCt;
            st.baselineRecordCt = curRecordCt;
            continue;
        }

        // Any change from baseline means the engine ran some pipeline (user
        // equipped, customized colors, opened/closed a menu, etc.). Soft
        // revert: drop our injected bucket and any Item* still bound to our
        // mod path. The engine's own state (whatever the user just did) is
        // already there — by removing ours, only the engine's stays.
        if (curHashCt != st.baselineHashCt || curRecordCt != st.baselineRecordCt)
        {
            // 1. Remove our bucket via engine's hashmap_remove (sub_1650620).
            //    Uses the same code path Character_ApplyClothingId uses, so
            //    hashmap integrity is preserved.
            CallHashmapRemoveGuarded(am, st.modPath.c_str());

            // 2. Drop any Item* in m_AssetRecords that contains our mod path
            //    in any of its asset slots. May find more than one if the
            //    engine pushed multiples; loop until none remain.
            for (int safety = 0; safety < 8; ++safety)
            {
                int idx = FindAssetRecordByPath(am, st.modPath.c_str());
                if (idx < 0) break;
                if (!RemoveAssetRecordAt(am, idx)) break;
            }

            st = {};   // mark inactive, clear all state
        }
    }
}

void SkinnedMeshManager::AutoReapplyOnDrift()
{
    if (!g_autoReapply) return;

    // Throttle to ~0.5s. ImGui::GetTime is just the display-frame clock, but
    // it's monotonic and cheap, so it's fine for this.
    double now = ImGui::GetTime();
    if (now - s_lastReapplyCheck < 0.5) return;
    s_lastReapplyCheck = now;

    for (int slotIdx = 0; slotIdx < 27; ++slotIdx)
    {
        const std::string& want = s_lastApplied[slotIdx];
        if (want.empty()) continue;

        // Look up current path from the live slot scan.
        const LiveSlot* live = nullptr;
        for (const auto& ls : m_slots)
            if (ls.index == slotIdx) { live = &ls; break; }
        if (!live) continue;

        // No drift → don't disturb. The whole point of this pass is to fix up
        // what the engine has reverted, not to keep poking healthy state.
        if (live->currentPath == want) continue;

        // Drifted: re-apply. ApplyDirectSwap re-installs everything (path,
        // bucket, dirty flag) and re-arms the SoftRevert tracking for the
        // next engine event.
        ApplyDirectSwap(slotIdx, want.c_str(), nullptr);
    }
}

void SkinnedMeshManager::ScanLiveSlots()
{
    // The engine's character/customization menu briefly nulls every slot's
    // m_pSlot while it swaps in a preview character. Naïve scan during that
    // window shows "no populated slots" and looks like everything got
    // destroyed. Strategy: only replace m_slots when we get a populated
    // result. If the scan comes back empty, keep the previous (now stale)
    // m_slots for up to ~5 seconds before giving up.
    //
    // Counter is a function-local static so we don't depend on whether
    // any specific class field is visible to the precompiled header.
    static int s_emptyScans = 0;
    constexpr int kMaxEmptyScansBeforeGiveUp = 300;   // ~5s @ 60 fps

    std::string thisScanError;
    TD::AppearanceManager* am = GetPlayerAppearance(&thisScanError);
    if (!am)
    {
        ++s_emptyScans;
        if (s_emptyScans > kMaxEmptyScansBeforeGiveUp)
        {
            m_slots.clear();
            m_scanError = thisScanError;
        }
        else if (!m_slots.empty())
        {
            m_scanError = "(showing last good scan — " + thisScanError + ")";
        }
        return;
    }

    RawSlotInfo raw[27];
    if (!ReadAllSlotsGuarded(am, raw))
    {
        ++s_emptyScans;
        if (s_emptyScans > kMaxEmptyScansBeforeGiveUp)
        {
            m_slots.clear();
            m_scanError = "exception while reading slot table (player despawning)";
        }
        else if (!m_slots.empty())
        {
            m_scanError = "(showing last good scan — read AV)";
        }
        return;
    }

    std::vector<LiveSlot> newSlots;
    for (int i = 0; i < 27; ++i)
    {
        if (!raw[i].valid) continue;
        LiveSlot ls;
        ls.index       = i;
        ls.type        = ClassifyPath(raw[i].path);
        ls.currentPath = raw[i].path;
        ls.capacity    = raw[i].cap;
        ls.canMutate   = raw[i].isHeap && raw[i].cap > 0;
        newSlots.push_back(std::move(ls));
    }

    if (newSlots.empty())
    {
        // Transient: engine is mid-equip / in customization preview.
        ++s_emptyScans;
        if (s_emptyScans > kMaxEmptyScansBeforeGiveUp)
        {
            m_slots.clear();
            m_scanError = "no populated slots";
        }
        else if (!m_slots.empty())
        {
            m_scanError = "(showing last good scan — engine mid-update)";
        }
        else
        {
            m_scanError = "no populated slots";
        }
        return;
    }

    // Got a fresh populated scan — replace cache, clear transient counter.
    m_slots      = std::move(newSlots);
    m_scanError.clear();
    s_emptyScans = 0;
}

// ─── direct in-place mutation ────────────────────────────────────────────────

bool SkinnedMeshManager::ApplyDirectSwap(int slotIndex, const char* newPath,
                                        std::string* errOut)
{
    if (slotIndex < 0 || slotIndex >= 27) { if (errOut) *errOut = "slot index out of range"; return false; }
    if (!newPath || !*newPath)            { if (errOut) *errOut = "empty target path";       return false; }

    TD::AppearanceManager* am = GetPlayerAppearance(errOut);
    if (!am) return false;

    auto& slot = am->m_Clothes[slotIndex];

    // 1. Reset the slot via the engine's own per-slot apply. This removes any
    //    AttachBucket whose m_ClothingId == slotIndex (including a bucket we
    //    inserted on a previous Apply) AND ref-drops the slot's existing Item*
    //    in m_AssetRecords — without this, the original bag stays bound and
    //    renders on top of ours. ApplyClothingId will overwrite m_Clothes[N].
    //    m_Path with the engine's cached path; we re-write our own afterwards.
    bool slotReset = CallApplyClothingIdGuarded(am, (std::uint32_t)slotIndex);

    // 2. Suppress any stragglers (e.g. buckets ApplyClothingId didn't catch
    //    because of the build's NULL +0x28 hashmap quirk). Cheap belt-and-
    //    braces; sets m_ClothingId to a sentinel so per-slot processing
    //    ignores them while leaving the hashmap key/hash intact.
    int disqualified = DisqualifyBucketsForSlot(am, slotIndex);

    // 3. Write our path. Re-read sstr because the engine reset above may have
    //    reallocated the heap buffer (its "writes cached path" step).
    BYTE* sstr = slot.m_Path.bytes;
    std::size_t newLen = std::strlen(newPath);

    // Two-path write strategy:
    //   a) If the existing heap allocation has enough capacity, do a fast
    //      VirtualProtect + memcpy (no engine-side state changes).
    //   b) Otherwise, hand off to the engine's own SnowdropString::assign
    //      (sub_116830). That function reallocates with the engine's
    //      allocator, so any later free by the engine is safe.
    bool wroteOk = false;
    bool usedEngineAssign = false;

    if (sstr[0x0F] != 0)   // heap-mode SnowdropString
    {
        char* heapStr = *(char**)sstr;
        if (heapStr)
        {
            std::uint32_t capacity = *(std::uint32_t*)(heapStr - 4);
            if (capacity > 0 && capacity <= 0x1000 && newLen + 1 <= capacity)
            {
                wroteOk = GuardedHeapWrite(heapStr, newPath, newLen, capacity);
            }
        }
    }

    if (!wroteOk)
    {
        // Fallback: let the engine grow / re-allocate the SnowdropString.
        // Works for both heap-mode (cap-too-small) and inline-mode strings,
        // and is the only allocator-correct way to grow the buffer.
        usedEngineAssign = CallStringAssignGuarded(&slot.m_Path, newPath);
        wroteOk = usedEngineAssign;
    }

    if (!wroteOk)
    {
        if (errOut) *errOut = "path write failed (both fast-path and engine assign)";
        return false;
    }

    // 4. Insert a properly-keyed AttachBucket for our new path via the engine's
    //    own model-load trigger. The engine's insert routes through sub_1544E60
    //    (hashmap_insert), which keeps the hashmap key hash and entry array
    //    consistent — unlike in-place m_ModelPath mutation.
    bool bucketInserted = CallModelLoadTriggerGuarded(am, &slot.m_Path,
                                                      (std::uint32_t)slotIndex);

    // Trigger a visible re-render via the auto-clearing dirty flag only.
    // Deliberately NOT touching m_ListUpdated / m_NeedsResync — those are the
    // engine's stable change-pending flags and leaving them at 1 makes the next
    // consumption pass re-enter against m_ClothingIdList (which doesn't contain
    // our mod) and trashes the visual on the next outfit change.
    am->m_DirtyFlag = 1;

    // Register / reset per-slot mod tracking. On the next few Update() ticks,
    // SoftRevertOnEngineActivity will snapshot the engine's count baselines
    // and then watch for any change (equip/customize) — when that happens our
    // bucket and Item*s are removed and the mod reverts.
    //
    // Also remember newPath in s_lastApplied so AutoReapplyOnDrift can re-do
    // the Apply if the engine reverts and the user has the toggle on.
    {
        auto& st = s_modState[slotIndex];
        st = {};
        st.active  = true;
        st.modPath = newPath;
        s_lastApplied[slotIndex] = newPath;
    }

    if (errOut)
    {
        char buf[220];
        std::snprintf(buf, sizeof(buf),
                      "ok (reset:%s, %s, %d disq, %s) — instant swap; reverts on next engine change",
                      slotReset       ? "y" : "n",
                      usedEngineAssign ? "engine assign" : "fast memcpy",
                      disqualified,
                      bucketInserted  ? "bucket+" : "bucket-FAIL");
        *errOut = buf;
    }
    return true;
}

// ─── UI ──────────────────────────────────────────────────────────────────────

static const SkinnedMeshManager::ModelSwapEntry*
GetModelList(SkinnedMeshManager::GearType t, int& outCount)
{
    using GT = SkinnedMeshManager::GearType;
    const ModelList* lst = nullptr;
    switch (t)
    {
    case GT::Backpack:   lst = &s_backpackModels;   break;
    case GT::Shirt:      lst = &s_shirtModels;      break;
    case GT::Chestplate: lst = &s_chestplateModels; break;
    case GT::Jacket:     lst = &s_jacketModels;     break;
    case GT::Pants:      lst = &s_pantsModels;      break;
    case GT::Thigh:      lst = &s_thighModels;      break;
    case GT::Feet:       lst = &s_feetModels;       break;
    case GT::Scarf:      lst = &s_scarfModels;      break;
    case GT::Kneepads:   lst = &s_kneepadsModels;   break;
    case GT::Hat:        lst = &s_hatModels;        break;
    case GT::Gloves:     lst = &s_glovesModels;     break;
    case GT::GasMask:    lst = &s_gasMaskModels;    break;
    default:             outCount = 0; return nullptr;
    }
    outCount = lst->count;
    return lst->entries;
}

// Per-slot UI state (one row per LiveSlot).
struct SlotUIState
{
    int  pickedIndex = -1;          // -1 = none / "current"; >=0 = entry in model list
    char custom[256] = {};          // free-text path (overrides pickedIndex if non-empty)
    std::string lastResult;         // result message of the most recent Apply
    bool        lastOk = false;
};

static SlotUIState& UIStateForSlot(int slotIndex)
{
    static SlotUIState s_states[27];
    int idx = (slotIndex < 0 || slotIndex >= 27) ? 0 : slotIndex;
    return s_states[idx];
}

void SkinnedMeshManager::DrawUI()
{
    // Refresh on every draw — cheap, only walks 27 slots.
    ScanLiveSlots();

    ImGui::TextWrapped("Skin Changer — direct in-place mutation of m_Clothes[N].m_Path "
                       "via Agent->m_pAppearance. Visual reload may require re-equip / "
                       "zone-change if setting m_DirtyFlag isn't enough on its own.");

    // Live diagnostic — shows what the singleton chain is actually returning so
    // we can tell the difference between "engine has no player" and "scan logic
    // bug" when slots vanish. Reads happen in a POD-only helper (DiagInfo) so
    // the SEH guard around the agent-type read doesn't conflict with C++ object
    // unwinding in this function.
    {
        DiagInfo di;
        GatherDiagInfo(&di);
        ImGui::TextDisabled(
            "[diag] agents=%d  player_idx=%d  player=0x%p  type=%d  AM=0x%p",
            di.agentCount, di.playerIdx, (void*)di.player, di.playerType, (void*)di.am);
    }

    // Auto re-apply controls. Drift-only: if the slot's current path already
    // matches what we last applied, this pass leaves it alone — only the
    // slots the engine has reverted get re-Applied.
    ImGui::Checkbox("Auto re-apply every 0.5s on drift", &g_autoReapply);
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip(
            "Polls each slot every ~0.5s. If a slot's path no longer matches\n"
            "the last mod you applied (e.g. the engine just reverted it after\n"
            "you equipped or customized something), re-runs Apply for that slot.\n"
            "Slots that are already matching are left untouched.");
    }
    ImGui::SameLine();
    if (ImGui::Button("Forget all"))
    {
        for (int i = 0; i < 27; ++i) s_lastApplied[i].clear();
    }
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Clears the saved last-applied paths so auto re-apply has nothing to restore.");

    // Show error/transient banner if any, but don't bail out — the slot list
    // below is rendered from m_slots which may be stale-but-valid during the
    // engine's customization-menu preview window.
    if (!m_scanError.empty())
    {
        bool isTransient = m_scanError.find("last good scan") != std::string::npos;
        ImVec4 col = isTransient ? ImVec4(1.0f, 0.85f, 0.2f, 1.0f)
                                 : ImVec4(1.0f, 0.40f, 0.40f, 1.0f);
        ImGui::PushStyleColor(ImGuiCol_Text, col);
        ImGui::Text("%s", m_scanError.c_str());
        ImGui::PopStyleColor();
    }

    if (m_slots.empty())
    {
        ImGui::TextDisabled("(no populated slots — load into the world first)");
        return;
    }

    ImGui::Separator();

    for (const auto& ls : m_slots)
    {
        ImGui::PushID(ls.index);

        // Header: "Slot 7 — Jacket (L3)"
        ImGui::TextColored(ImVec4(0.6f, 0.85f, 1.0f, 1.0f),
                           "Slot %d — %s", ls.index, GearTypeName(ls.type));
        ImGui::SameLine();
        ImGui::TextDisabled("[cap %u, %s]",
                            (unsigned)ls.capacity,
                            ls.canMutate ? "mutable" : "INLINE/locked");

        if (ls.currentPath.empty())
            ImGui::TextDisabled("Current: (engine cleared — heap allocation reserved, can still write)");
        else
            ImGui::TextWrapped("Current: %s", ls.currentPath.c_str());

        if (!ls.canMutate)
        {
            ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.2f, 1.0f),
                               "  cannot mutate (path is inline — heap conversion not implemented)");
            ImGui::Separator();
            ImGui::PopID();
            continue;
        }

        int        modelCount = 0;
        const auto* models = GetModelList(ls.type, modelCount);

        SlotUIState& ui = UIStateForSlot(ls.index);

        // Dropdown of curated models for this gear type
        const char* preview = (ui.pickedIndex >= 0 && ui.pickedIndex < modelCount)
                              ? models[ui.pickedIndex].displayName
                              : "(pick a model or type a custom path)";
        ImGui::PushItemWidth(360.0f);
        if (ImGui::BeginCombo("##picker", preview))
        {
            for (int i = 0; i < modelCount; ++i)
            {
                bool selected = (ui.pickedIndex == i);
                if (ImGui::Selectable(models[i].displayName, selected))
                    ui.pickedIndex = i;
                if (selected) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        ImGui::PopItemWidth();

        // Free-text custom path (overrides dropdown when non-empty)
        ImGui::PushItemWidth(360.0f);
        ImGui::InputText("custom##path", ui.custom, sizeof(ui.custom));
        ImGui::PopItemWidth();

        if (ImGui::Button("Apply"))
        {
            const char* target = nullptr;
            if (ui.custom[0] != '\0')
                target = ui.custom;
            else if (ui.pickedIndex >= 0 && ui.pickedIndex < modelCount)
                target = models[ui.pickedIndex].assetPath;

            if (!target || !*target)
            {
                ui.lastOk = false;
                ui.lastResult = "no target selected";
            }
            else
            {
                std::string err;
                bool ok = ApplyDirectSwap(ls.index, target, &err);
                ui.lastOk = ok;
                ui.lastResult = ok
                    ? (err.empty() ? (std::string("ok — wrote: ") + target)
                                   : (err + " — " + target))
                    : (std::string("failed: ") + err);
            }
        }

        if (!ui.lastResult.empty())
        {
            ImVec4 col = ui.lastOk ? ImVec4(0.5f, 1.0f, 0.5f, 1.0f)
                                   : ImVec4(1.0f, 0.5f, 0.5f, 1.0f);
            ImGui::TextColored(col, "  %s", ui.lastResult.c_str());
        }

        ImGui::Separator();
        ImGui::PopID();
    }
}
