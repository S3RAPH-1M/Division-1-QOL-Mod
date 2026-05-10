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

void SkinnedMeshManager::Update()
{
    ScanLiveSlots();
}

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
    bool ReadAllSlotsGuarded(TD::AppearanceManager* am, RawSlotInfo* out)
    {
        std::memset(out, 0, sizeof(RawSlotInfo) * 27);
        __try
        {
            for (int i = 0; i < 27; ++i)
            {
                const auto& slot = am->m_Clothes[i];
                if (!slot.m_pSlot) continue;

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
                    out[i].valid = (out[i].path[0] != '\0');
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

    struct BucketTarget
    {
        char*                              heapStr;
        std::uint32_t                      cap;
        TD::AppearanceManager::AttachBucket* bucket;   // for clearing m_Initialized post-mutation
    };

    // Walks m_AttachHashmap_Buckets and collects every bucket whose m_ClothingId
    // matches slotId. Returns both the bucket's heap path-string (for mutation)
    // and the bucket pointer (for invalidation flags).
    int FindAttachBucketsForSlot(TD::AppearanceManager* am, int slotId,
                                 BucketTarget* out, int maxOut)
    {
        int found = 0;
        __try
        {
            auto* buckets = am->m_AttachHashmap_Buckets;
            int count = am->m_AttachHashmap_Count;
            if (!buckets || count <= 0) return 0;

            for (int i = 0; i < count && found < maxOut; ++i)
            {
                auto& b = buckets[i];
                if (b.m_ClothingId != slotId) continue;

                const BYTE* sstr = b.m_ModelPath.bytes;
                if (sstr[0x0F] == 0) continue;             // inline string — skip
                char* heapStr = *(char**)sstr;
                if (!heapStr) continue;

                std::uint32_t cap = *(const std::uint32_t*)(heapStr - 4);
                if (cap == 0 || cap > 0x1000) continue;

                out[found].heapStr = heapStr;
                out[found].cap     = cap;
                out[found].bucket  = &b;
                ++found;
            }
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            // return whatever we collected before the AV
        }
        return found;
    }

    // Calls the engine's AppearanceManager_ModelLoadTrigger (sub_162FDA0).
    // Returns true if the call returned without crashing; *outResult holds the
    // engine's return value (1 = new bucket inserted, 0 = bucket already exists).
    bool CallModelLoadTriggerGuarded(TD::AppearanceManager* am,
                                     TD::SnowdropString* path,
                                     std::uint32_t slotId,
                                     __int64* outResult)
    {
        typedef __int64 (__fastcall *PFN)(TD::AppearanceManager*, TD::SnowdropString*, std::uint32_t*);
        PFN fn = (PFN)(g_pBase + 0x162FDA0);
        *outResult = -1;
        __try
        {
            *outResult = fn(am, path, &slotId);
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            return false;
        }
        return true;
    }
}

// ─── live slot scanning ──────────────────────────────────────────────────────

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

    auto* player = world->m_AgentArray[0];
    if (!player) { if (errOut) *errOut = "player agent null";        return nullptr; }
    int type = *(int*)((__int64)player + 0x3A4);
    if (type != 1) { if (errOut) *errOut = "agent[0] is not player"; return nullptr; }

    auto* am = player->m_pAppearance;
    if (!am)     { if (errOut) *errOut = "AppearanceManager null";   return nullptr; }
    return am;
}

void SkinnedMeshManager::ScanLiveSlots()
{
    m_slots.clear();
    m_scanError.clear();

    TD::AppearanceManager* am = GetPlayerAppearance(&m_scanError);
    if (!am) return;

    RawSlotInfo raw[27];
    if (!ReadAllSlotsGuarded(am, raw))
    {
        m_scanError = "exception while reading slot table (player likely despawning)";
        return;
    }

    for (int i = 0; i < 27; ++i)
    {
        if (!raw[i].valid) continue;

        LiveSlot ls;
        ls.index       = i;
        ls.type        = ClassifyPath(raw[i].path);
        ls.currentPath = raw[i].path;
        ls.capacity    = raw[i].cap;
        ls.canMutate   = raw[i].isHeap && raw[i].cap > 0;
        m_slots.push_back(std::move(ls));
    }
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
    if (!slot.m_pSlot) { if (errOut) *errOut = "slot not populated"; return false; }

    BYTE* sstr = slot.m_Path.bytes;
    bool isHeap = sstr[0x0F] != 0;
    if (!isHeap) { if (errOut) *errOut = "path is inline (heap conversion not implemented)"; return false; }

    char* heapStr = *(char**)sstr;
    if (!heapStr) { if (errOut) *errOut = "heap pointer null"; return false; }

    std::uint32_t capacity = *(std::uint32_t*)(heapStr - 4);
    if (capacity == 0 || capacity > 0x1000) { if (errOut) *errOut = "capacity sanity check failed"; return false; }

    std::size_t newLen = std::strlen(newPath);
    if (newLen + 1 > capacity)
    {
        if (errOut)
        {
            char buf[128];
            std::snprintf(buf, sizeof(buf), "new path too long: %zu+1 > %u", newLen, capacity);
            *errOut = buf;
        }
        return false;
    }

    if (!GuardedHeapWrite(heapStr, newPath, newLen, capacity))
    {
        if (errOut) *errOut = "VirtualProtect/memcpy failed";
        return false;
    }

    // ── ALSO mutate matching AttachBucket(s) m_ModelPath. ────────────────
    // The slot's m_Path is a cached/displayed copy; the engine's mesh-loader
    // reads from buckets in m_AttachHashmap. Mutating the bucket too gives
    // the engine consistent state across path lookups (and produces visible
    // texture rebinds on the next consumption pass).
    //
    // We deliberately do NOT call the engine's ModelLoadTrigger from here:
    // injecting a fresh bucket leaves a stale OLD bucket behind, which the
    // engine's normal in-game gear-change pipeline (Character_ApplyClothingId
    // → bucket-remove-by-clothing-id) gets confused by, sometimes deleting
    // the slot entirely. Until we have a safe bucket-remove path, stick with
    // pure mutation. Visual mesh swap will require the user to re-equip in
    // the in-game UI for the engine to fully reload — that's the original
    // SkinnedMeshManager workflow and it's stable.
    BucketTarget targets[8];
    int nTargets = FindAttachBucketsForSlot(am, slotIndex, targets, 8);
    int bucketsMutated = 0;
    int bucketsTooSmall = 0;
    for (int i = 0; i < nTargets; ++i)
    {
        if (newLen + 1 > targets[i].cap) { ++bucketsTooSmall; continue; }
        if (GuardedHeapWrite(targets[i].heapStr, newPath, newLen, targets[i].cap))
            ++bucketsMutated;
    }

    // Nudge the consumption flags. The engine clears m_DirtyFlag itself;
    // m_ListUpdated and m_NeedsResync persist as "changes pending" markers.
    am->m_DirtyFlag    = 1;
    am->m_ListUpdated  = 1;
    am->m_NeedsResync  = 1;

    if (errOut)
    {
        char buf[200];
        std::snprintf(buf, sizeof(buf),
                      "ok (slot path; %d/%d bucket(s) mutated%s) — re-equip in-game for full mesh swap",
                      bucketsMutated, nTargets,
                      bucketsTooSmall ? ", some bucket caps too small" : "");
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
    if (slotIndex < 0 || slotIndex >= 27) return s_states[0];
    return s_states[slotIndex];
}

void SkinnedMeshManager::DrawUI()
{
    // Refresh on every draw — cheap, only walks 27 slots.
    ScanLiveSlots();

    ImGui::TextWrapped("Skin Changer — direct in-place mutation of m_Clothes[N].m_Path "
                       "via Agent->m_pAppearance. Visual reload may require re-equip / "
                       "zone-change if setting m_DirtyFlag isn't enough on its own.");

    if (!m_scanError.empty())
    {
        ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 100, 100, 255));
        ImGui::Text("Scan error: %s", m_scanError.c_str());
        ImGui::PopStyleColor();
        return;
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

        ImGui::SameLine();
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
                if (ok)
                {
                    ui.lastResult = err.empty()
                                    ? (std::string("ok — wrote: ") + target)
                                    : (err + " — " + target);
                }
                else
                {
                    ui.lastResult = std::string("failed: ") + err;
                }
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
