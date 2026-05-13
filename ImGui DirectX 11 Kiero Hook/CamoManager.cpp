#include "CamoManager.h"
#include "Snowdrop.h"
#include "Main.h"
#include "ItemDescriptorCache.h"
#include "imgui/imgui.h"

#include <Windows.h>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>

// All engine-call wrappers are SEH-guarded raw-memory helpers. They live in
// this anonymous namespace and host __try/__except in POD-only functions so
// the public methods above can hold std::string / std::vector without
// tripping MSVC C2712. Mirrors the SkinnedMeshManager pattern.
namespace
{
    // ── curated armor-recolor presets ────────────────────────────────────
    // The "base" texture set on the player at character load is the arm
    // patch (CA_SHD_Armpatch_D/N/M.dds). Anything else dropped into
    // m_PathStrings1[3..5] will repaint the same surface on the next
    // consume pass — which is the engine-side primitive for an outfit
    // recolor. These presets re-use stock Division .dds assets the engine
    // is already known to stream.
    const CamoManager::ArmorCamoPreset kPresets[] =
    {
        { "Black-Out",
          "rogue/baked/art/[gear]/agent/textures/CA_SHD_Armpatch_Black_D.dds",
          "rogue/baked/art/[gear]/agent/textures/ca_shd_armpatch_n.dds",
          "rogue/baked/art/[gear]/agent/textures/CA_SHD_Armpatch_M.dds" },
        { "Stock Restore (vanilla arm patch)",
          "rogue/baked/art/[gear]/agent/textures/CA_SHD_Armpatch_D.dds",
          "rogue/baked/art/[gear]/agent/textures/ca_shd_armpatch_n.dds",
          "rogue/baked/art/[gear]/agent/textures/CA_SHD_Armpatch_M.dds" },
        { "Zebra (weapon camo dds)",
          "rogue/baked/art/[Weapons]/Attachments/camo/w_camo_zebra.dds",
          "rogue/baked/art/[gear]/agent/textures/ca_shd_armpatch_n.dds",
          "rogue/baked/art/[gear]/agent/textures/CA_SHD_Armpatch_Empty_M.dds" },
        { "Stripes (weapon camo dds)",
          "rogue/baked/art/[Weapons]/Attachments/Camo/W_Camo_Stripes_01.dds",
          "rogue/baked/art/[gear]/agent/textures/ca_shd_armpatch_n.dds",
          "rogue/baked/art/[gear]/agent/textures/CA_SHD_Armpatch_Empty_M.dds" },
        { "Solid Pink (weapon camo dds)",
          "rogue/baked/art/[Weapons]/Attachments/Camo/W_Camo_Solid.dds",
          "rogue/baked/art/[gear]/agent/textures/ca_shd_armpatch_n.dds",
          "rogue/baked/art/[gear]/agent/textures/CA_SHD_Armpatch_Empty_M.dds" },
    };
    const int kPresetCount = (int)(sizeof(kPresets) / sizeof(kPresets[0]));

    // ── Engine entry points (RVAs into TheDivision.exe). All from
    // .claude/docs/05-items-and-camos.md / Snowdrop.h. ───────────────────
    constexpr std::uintptr_t kRVA_StringAssign       = 0x116830;  // SnowdropString::assign(this, c_str)

    // SkinItem vtable (verified live read against b_camo_solid_pink in 05-items-and-camos.md).
    constexpr std::uintptr_t kRVA_SkinItemVtable     = 0x2CBA028;

    // SkinItem field offsets (verified — see 05-items-and-camos.md
    // "Verified SkinItem layout").
    constexpr int kOff_SkinTexture = 688;   // SnowdropString
    constexpr int kOff_RotU        = 736;   // float
    constexpr int kOff_RotV        = 740;   // float
    constexpr int kOff_ScaleX      = 744;   // float
    constexpr int kOff_ScaleY      = 748;   // float
    constexpr int kOff_Color1      = 752;   // ARGB DWORD
    constexpr int kOff_Color2      = 756;
    constexpr int kOff_Color3      = 760;

    // SEH-guarded SnowdropString assign through the engine's allocator.
    // Same pattern SkinnedMeshManager uses.
    bool CallStringAssignGuarded(TD::SnowdropString* str, const char* newPath)
    {
        if (!str || !newPath) return false;
        typedef void (__fastcall *PFN)(TD::SnowdropString*, const char*);
        PFN fn = (PFN)(g_pBase + kRVA_StringAssign);
        __try { fn(str, newPath); }
        __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
        return true;
    }

    // Find the player AppearanceManager via the singleton chain. Returns
    // nullptr on any link missing — the UI shows a "no AM" status in that
    // case. POD-only so it can host __try.
    TD::AppearanceManager* FindPlayerAM_POD()
    {
        TD::AppearanceManager* am = nullptr;
        __try
        {
            auto* rc = TD::RogueClient::Singleton();
            if (!rc) return nullptr;
            auto* client = rc->m_pClient;
            if (!client) return nullptr;
            auto* world = client->m_pWorld;
            if (!world || !world->m_AgentArray) return nullptr;

            for (int i = 0; i < world->m_AgentCount; ++i)
            {
                auto* a = world->m_AgentArray[i];
                if (!a) continue;
                int type = *(int*)((__int64)a + 0x3A4);
                if (type == 1) { am = a->m_pAppearance; break; }
            }
            if (!am && world->m_AgentCount > 0 && world->m_AgentArray[0])
                am = world->m_AgentArray[0]->m_pAppearance;
        }
        __except (EXCEPTION_EXECUTE_HANDLER) { return nullptr; }
        return am;
    }

    // POD copy of a SnowdropString into a fixed-size char buf. Same shape
    // as SkinnedMeshManager::ReadSnowdropStringAt — duplicated to keep
    // this TU independent.
    bool ReadSnowdropStringAt(const BYTE* sstr, char* outPath, std::size_t outSize)
    {
        if (!sstr || !outPath || outSize == 0) return false;
        outPath[0] = '\0';
        __try
        {
            const char* path = nullptr;
            if (sstr[0x0F] == 0)
                path = (const char*)sstr;
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
        __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
    }

    // Pre-flight read of one candidate Item* (from PlayerInventory).
    // Falls back to safe defaults on any SEH; caller looks at vtableOk
    // to decide whether to surface the entry.
    struct SkinItemRaw
    {
        bool          ok;
        bool          vtableOk;
        void*         itemPtr;
        char          texturePath[260];
        float         rotU, rotV, scaleX, scaleY;
        std::uint32_t color1, color2, color3;
    };

    void ReadSkinItemRaw_POD(void* itemPtr, SkinItemRaw* out)
    {
        out->ok = false;
        out->vtableOk = false;
        out->itemPtr = itemPtr;
        out->texturePath[0] = '\0';
        out->rotU = out->rotV = 0.0f;
        out->scaleX = out->scaleY = 1.0f;
        out->color1 = out->color2 = out->color3 = 0;
        if (!itemPtr) return;

        __try
        {
            std::uintptr_t vtable = *(std::uintptr_t*)itemPtr;
            std::uintptr_t expected = (std::uintptr_t)(g_pBase + kRVA_SkinItemVtable);
            out->vtableOk = (vtable == expected);
            if (!out->vtableOk) { out->ok = true; return; }

            ReadSnowdropStringAt((const BYTE*)itemPtr + kOff_SkinTexture,
                                 out->texturePath, sizeof(out->texturePath));
            out->rotU   = *(float*)((BYTE*)itemPtr + kOff_RotU);
            out->rotV   = *(float*)((BYTE*)itemPtr + kOff_RotV);
            out->scaleX = *(float*)((BYTE*)itemPtr + kOff_ScaleX);
            out->scaleY = *(float*)((BYTE*)itemPtr + kOff_ScaleY);
            out->color1 = *(std::uint32_t*)((BYTE*)itemPtr + kOff_Color1);
            out->color2 = *(std::uint32_t*)((BYTE*)itemPtr + kOff_Color2);
            out->color3 = *(std::uint32_t*)((BYTE*)itemPtr + kOff_Color3);
            out->ok = true;
        }
        __except (EXCEPTION_EXECUTE_HANDLER) { out->ok = false; }
    }

    // Apply mutations to a single SkinItem in place. POD-only — std::string
    // would have made the SEH guards illegal under C2712.
    struct SkinItemEditPlan
    {
        bool          writeTexture;
        const char*   texturePath;
        bool          writeColor1, writeColor2, writeColor3;
        std::uint32_t color1, color2, color3;
        bool          writeRotU, writeRotV, writeScaleX, writeScaleY;
        float         rotU, rotV, scaleX, scaleY;
    };

    struct SkinItemEditResult
    {
        bool wroteTexture;
        bool wroteColors;
        bool wroteFloats;
        bool callFailed;
    };

    void ApplySkinItemEdit_POD(void* itemPtr,
                               const SkinItemEditPlan* plan,
                               SkinItemEditResult* out)
    {
        out->wroteTexture = false;
        out->wroteColors  = false;
        out->wroteFloats  = false;
        out->callFailed   = false;
        if (!itemPtr || !plan) { out->callFailed = true; return; }

        if (plan->writeTexture && plan->texturePath && plan->texturePath[0])
        {
            TD::SnowdropString* dst = (TD::SnowdropString*)((BYTE*)itemPtr + kOff_SkinTexture);
            if (CallStringAssignGuarded(dst, plan->texturePath))
                out->wroteTexture = true;
            else
                out->callFailed = true;
        }

        __try
        {
            if (plan->writeColor1) { *(std::uint32_t*)((BYTE*)itemPtr + kOff_Color1) = plan->color1; out->wroteColors = true; }
            if (plan->writeColor2) { *(std::uint32_t*)((BYTE*)itemPtr + kOff_Color2) = plan->color2; out->wroteColors = true; }
            if (plan->writeColor3) { *(std::uint32_t*)((BYTE*)itemPtr + kOff_Color3) = plan->color3; out->wroteColors = true; }
            if (plan->writeRotU)   { *(float*)((BYTE*)itemPtr + kOff_RotU)   = plan->rotU;   out->wroteFloats = true; }
            if (plan->writeRotV)   { *(float*)((BYTE*)itemPtr + kOff_RotV)   = plan->rotV;   out->wroteFloats = true; }
            if (plan->writeScaleX) { *(float*)((BYTE*)itemPtr + kOff_ScaleX) = plan->scaleX; out->wroteFloats = true; }
            if (plan->writeScaleY) { *(float*)((BYTE*)itemPtr + kOff_ScaleY) = plan->scaleY; out->wroteFloats = true; }
        }
        __except (EXCEPTION_EXECUTE_HANDLER) { out->callFailed = true; }
    }

    // Walk the player's PlayerInventory equipped-holder table
    // (PI+0x80..+0x200) and call back with each (holder, wrapper, inner)
    // triple where every link is heap-valid. holder lives at
    // PI+0x80+i*8; holder+0x50 = wrapper; wrapper+0x00 = inner Item*.
    //
    // This is where camos actually live on the player — they're separate
    // wrapper slots in PlayerInventory, NOT entries in m_AssetRecords.
    // Verified live via the equip-pipeline RE pass (see
    // 06-inventory-equip-pipeline.md "Where EquipInstances live" + IDA
    // decompile of sub_15A36D0 / Item_IsCamoVanityJacket).
    //
    // POD-only because of __try.
    bool LooksLikeHeapPtr_POD(void* p)
    {
        std::uint64_t v = (std::uint64_t)p;
        if (v < 0x10000ULL) return false;
        if (v & 7ULL) return false;          // 8-byte aligned
        if (v >= 0x7FFFFFFFFFFFULL) return false;
        return true;
    }

    struct InvInner
    {
        bool   ok;
        int    holderOffset;   // +0x80, +0x88, ..., +0x1F8
        void*  holder;
        void*  wrapper;
        void*  inner;          // wrapper.first_qword (= inner Item*)
    };

    int ScanPlayerInventoryInners_POD(InvInner* out, int outCap)
    {
        int n = 0;
        if (!out || outCap <= 0) return 0;

        TD::Agent* player = nullptr;
        __try
        {
            auto* rc = TD::RogueClient::Singleton();
            if (!rc) return 0;
            auto* client = rc->m_pClient;
            if (!client) return 0;
            auto* world = client->m_pWorld;
            if (!world || !world->m_AgentArray) return 0;
            for (int i = 0; i < world->m_AgentCount; ++i)
            {
                auto* a = world->m_AgentArray[i];
                if (!a) continue;
                int type = *(int*)((__int64)a + 0x3A4);
                if (type == 1) { player = a; break; }
            }
        }
        __except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
        if (!player) return 0;

        __try
        {
            void* inv = *(void**)((BYTE*)player + 0x5B0);
            if (!LooksLikeHeapPtr_POD(inv)) return 0;

            auto* base = (BYTE*)inv;
            for (int off = 0x80; off < 0x200 && n < outCap; off += 8)
            {
                void* holder = *(void**)(base + off);
                if (!LooksLikeHeapPtr_POD(holder)) continue;
                void* wrapper = *(void**)((BYTE*)holder + 0x50);
                if (!LooksLikeHeapPtr_POD(wrapper)) continue;
                void* inner = *(void**)wrapper;
                if (!LooksLikeHeapPtr_POD(inner)) continue;

                out[n].ok           = true;
                out[n].holderOffset = off;
                out[n].holder       = holder;
                out[n].wrapper      = wrapper;
                out[n].inner        = inner;
                ++n;
            }
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {}
        return n;
    }

    // 0xFFFFFFFE sentinel = "don't write this color"
    constexpr std::uint32_t kColorSentinel = 0xFFFFFFFE;
}

CamoManager::CamoManager()  = default;
CamoManager::~CamoManager() = default;

void CamoManager::Update()
{
    // No per-frame engine state to maintain — armor recolor and skin-item
    // edits are commit-and-forget. The "refresh skin item list" pass runs
    // inside DrawUI so we only pay for it when the user is looking at
    // this panel.
}

bool CamoManager::ApplyArmorCamo(const char* diffuseDds,
                                 const char* normalDds,
                                 const char* maskDds,
                                 std::string* errOut)
{
    auto* am = FindPlayerAM_POD();
    if (!am) { if (errOut) *errOut = "no player AppearanceManager"; return false; }

    // Engine-native camo apply path (verified by IDA decompile of
    // sub_15A36D0 / AppearanceManager_BindItemToSlot, camo branch):
    // writes 3 SnowdropStrings into m_PathStrings1[0..2] (BASE slots
    // at am+0x480/+0x490/+0x4A0) and sets m_DirtyFlag = 1. NO call to
    // sub_1542C60 — that finalize helper is only used by the init code
    // (sub_15A39F0) to seed the slot when it's empty.
    //
    // The override slots [3..5] are written-to-base by sub_1542C60 in a
    // different code path (clothing init), but for runtime camo apply
    // the engine bypasses them entirely.
    TD::SnowdropString* baseSlots = &am->m_PathStrings1[0];

    int wrote = 0;
    const char* paths[3] = { diffuseDds, normalDds, maskDds };
    for (int i = 0; i < 3; ++i)
    {
        const char* p = paths[i];
        if (!p || !*p) continue;
        if (CallStringAssignGuarded(&baseSlots[i], p))
            ++wrote;
    }

    if (wrote == 0)
    {
        if (errOut) *errOut = "nothing to apply (all paths empty)";
        return false;
    }

    // Trigger consumption. m_DirtyFlag auto-clears once the consume pass
    // runs, so this write doesn't leave a permanent flag set.
    __try { am->m_DirtyFlag = 1; }
    __except (EXCEPTION_EXECUTE_HANDLER) {}

    if (errOut)
    {
        char buf[160];
        std::snprintf(buf, sizeof(buf), "ok (%d/3 base slots written, dirty fired)", wrote);
        *errOut = buf;
    }
    return true;
}

// Read the current base-slot camo paths the engine is using for the
// player. The renderer samples these every frame; treat them as the
// "currently active armor camo" readout.
void CamoManager::ReadActiveArmorCamo(char outDiffuse[260],
                                      char outNormal[260],
                                      char outMask[260])
{
    if (outDiffuse) outDiffuse[0] = 0;
    if (outNormal)  outNormal[0]  = 0;
    if (outMask)    outMask[0]    = 0;

    auto* am = FindPlayerAM_POD();
    if (!am) return;

    if (outDiffuse)
        ReadSnowdropStringAt((const BYTE*)&am->m_PathStrings1[0], outDiffuse, 260);
    if (outNormal)
        ReadSnowdropStringAt((const BYTE*)&am->m_PathStrings1[1], outNormal,  260);
    if (outMask)
        ReadSnowdropStringAt((const BYTE*)&am->m_PathStrings1[2], outMask,    260);
}

void CamoManager::ListSkinItems(std::vector<SkinItemSnapshot>& out)
{
    out.clear();

    // PlayerInventory holders at PI+0x80..+0x200. Each holder+0x50 is a
    // wrapper, wrapper.first_qword is the inner Item* template. Camo
    // items (WeaponSkinItem / BackpackSkinItem / PatchSkinItem) all use
    // the SkinItem vtable g_pBase + 0x2CBA028 — set by sub_E39140 (the
    // shared 768-byte ctor). Walk every inner item and pick the ones
    // whose vtable matches.
    //
    // We DO NOT scan m_AssetRecords because camo wrappers don't go in
    // there — sub_15A36D0's camo branch only writes m_PathStrings1[0..2]
    // and never pushes an asset record. The bag/jacket ArmorItem is what
    // ends up in m_AssetRecords.
    InvInner inners[48] = {};
    int nFound = ScanPlayerInventoryInners_POD(inners, 48);

    for (int i = 0; i < nFound; ++i)
    {
        SkinItemRaw raw{};
        ReadSkinItemRaw_POD(inners[i].inner, &raw);
        if (!raw.ok || !raw.vtableOk) continue;

        SkinItemSnapshot s{};
        s.recordIndex = inners[i].holderOffset;  // use holder slot offset as a stable id
        s.itemPtr     = inners[i].inner;
        std::memcpy(s.texturePath, raw.texturePath, sizeof(s.texturePath));
        s.rotU = raw.rotU;   s.rotV = raw.rotV;
        s.scaleX = raw.scaleX; s.scaleY = raw.scaleY;
        s.color1 = raw.color1; s.color2 = raw.color2; s.color3 = raw.color3;
        out.push_back(s);
    }
}

bool CamoManager::ApplySkinItemEdit(int holderOffset,
                                    const char* newTexturePath,
                                    std::uint32_t color1,
                                    std::uint32_t color2,
                                    std::uint32_t color3,
                                    float rotU, float rotV,
                                    float scaleX, float scaleY,
                                    std::string* errOut)
{
    auto* am = FindPlayerAM_POD();
    if (!am) { if (errOut) *errOut = "no player AppearanceManager"; return false; }

    // Scan PI wrappers for SkinItems. If holderOffset >= 0, take the
    // wrapper at that PI offset; otherwise take the first SkinItem
    // we find.
    InvInner inners[48] = {};
    int nFound = ScanPlayerInventoryInners_POD(inners, 48);

    void* target = nullptr;
    int   matchedOff = -1;
    for (int i = 0; i < nFound; ++i)
    {
        if (holderOffset >= 0 && inners[i].holderOffset != holderOffset) continue;
        SkinItemRaw raw{};
        ReadSkinItemRaw_POD(inners[i].inner, &raw);
        if (raw.ok && raw.vtableOk)
        {
            target = inners[i].inner;
            matchedOff = inners[i].holderOffset;
            break;
        }
    }
    if (!target)
    {
        if (errOut) *errOut = "no SkinItem found in PlayerInventory (equip a camo first)";
        return false;
    }

    SkinItemEditPlan plan{};
    plan.writeTexture = (newTexturePath && newTexturePath[0]);
    plan.texturePath  = newTexturePath;
    plan.writeColor1 = (color1 != kColorSentinel);
    plan.writeColor2 = (color2 != kColorSentinel);
    plan.writeColor3 = (color3 != kColorSentinel);
    plan.color1 = color1; plan.color2 = color2; plan.color3 = color3;
    plan.writeRotU   = !std::isnan(rotU);   plan.rotU   = rotU;
    plan.writeRotV   = !std::isnan(rotV);   plan.rotV   = rotV;
    plan.writeScaleX = !std::isnan(scaleX); plan.scaleX = scaleX;
    plan.writeScaleY = !std::isnan(scaleY); plan.scaleY = scaleY;

    SkinItemEditResult res{};
    ApplySkinItemEdit_POD(target, &plan, &res);

    // After mutating the SkinItem fields, ping m_DirtyFlag so the
    // renderer re-samples the descriptor on the next consume pass.
    // Cheap insurance; the consume path will no-op if nothing changed.
    __try { am->m_DirtyFlag = 1; }
    __except (EXCEPTION_EXECUTE_HANDLER) {}

    if (errOut)
    {
        char buf[220];
        std::snprintf(buf, sizeof(buf),
                      "ok (PI offset 0x%X, tex:%s colors:%s floats:%s)",
                      matchedOff,
                      res.wroteTexture ? "y" : "skip",
                      res.wroteColors  ? "y" : "skip",
                      res.wroteFloats  ? "y" : "skip");
        *errOut = buf;
    }
    return !res.callFailed;
}

// ── Diagnostic: dump all inner items in PlayerInventory ─────────────────────

namespace
{
    // POD descriptor read of one inner item. Pulls vtable, slot id,
    // asset-type tag, and the base name (SnowdropString at +24).
    void ReadInnerDiag_POD(void* inner,
                           std::uint64_t* outVtableRVA,
                           int*           outSlot,
                           int*           outAssetTag,
                           char*          outName,
                           std::size_t    nameCap)
    {
        *outVtableRVA = 0;
        *outSlot      = -1;
        *outAssetTag  = -1;
        if (outName && nameCap > 0) outName[0] = '\0';
        if (!inner) return;
        __try
        {
            std::uintptr_t vt = *(std::uintptr_t*)inner;
            if (vt > (std::uintptr_t)g_pBase)
                *outVtableRVA = (std::uint64_t)(vt - (std::uintptr_t)g_pBase);
            *outSlot     = *(int*)((BYTE*)inner + 0x40);
            *outAssetTag = *(int*)((BYTE*)inner + 168);
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {}
        // Name SnowdropString at +24
        if (outName && nameCap > 0)
            ReadSnowdropStringAt((const BYTE*)inner + 24, outName, nameCap);
    }
}

void CamoManager::DumpPlayerInventory(std::vector<PiEntryRaw>& out)
{
    out.clear();

    // Sweep a generous range — earlier scans capped at +0x200 (the
    // clothing-relevant chunk), but camo wrappers might live past
    // that. SEH-guarded so reading off the end of the struct just
    // skips invalid offsets.
    InvInner inners[64] = {};
    int n = ScanPlayerInventoryInners_POD(inners, 64);

    for (int i = 0; i < n; ++i)
    {
        PiEntryRaw e{};
        e.holderOffset = inners[i].holderOffset;
        e.inner        = inners[i].inner;
        ReadInnerDiag_POD(inners[i].inner,
                          &e.vtableRVA, &e.slotId, &e.assetTypeTag,
                          e.name, sizeof(e.name));
        out.push_back(e);
    }
}

// ── Path C: edit a camo template by .mitem name ────────────────────────────

bool CamoManager::LookupCamoTemplate(const char* mitemName,
                                     SkinItemSnapshot* out,
                                     std::string* errOut)
{
    if (!mitemName || !*mitemName)
    {
        if (errOut) *errOut = "empty .mitem name";
        return false;
    }
    if (!out)
    {
        if (errOut) *errOut = "null out ptr";
        return false;
    }

    auto* desc = ItemDescriptorCache::LookupByName(mitemName);
    if (!desc)
    {
        if (errOut) *errOut = "name not found in InventoryConfig (cache may still be capturing)";
        return false;
    }

    SkinItemRaw raw{};
    ReadSkinItemRaw_POD((void*)desc, &raw);
    if (!raw.ok)
    {
        if (errOut) *errOut = "descriptor read faulted";
        return false;
    }
    // Non-SkinItem templates have an ArmorItem vtable etc. — still
    // return the snapshot but warn so the user knows the edit won't
    // hit the camo descriptor extension fields.
    *out = SkinItemSnapshot{};
    out->recordIndex = -1;
    out->itemPtr     = (void*)desc;
    std::memcpy(out->texturePath, raw.texturePath, sizeof(out->texturePath));
    out->rotU = raw.rotU;     out->rotV = raw.rotV;
    out->scaleX = raw.scaleX; out->scaleY = raw.scaleY;
    out->color1 = raw.color1; out->color2 = raw.color2; out->color3 = raw.color3;

    if (errOut)
    {
        char buf[200];
        std::snprintf(buf, sizeof(buf),
                      raw.vtableOk
                        ? "ok (SkinItem template %p)"
                        : "WARN: vtable is not SkinItem — edits to +688..+760 may be misaligned (%p)",
                      desc);
        *errOut = buf;
    }
    return true;
}

bool CamoManager::EditCamoTemplate(const char* mitemName,
                                   const char* newTexturePath,
                                   std::uint32_t color1,
                                   std::uint32_t color2,
                                   std::uint32_t color3,
                                   float rotU, float rotV,
                                   float scaleX, float scaleY,
                                   std::string* errOut)
{
    if (!mitemName || !*mitemName)
    {
        if (errOut) *errOut = "empty .mitem name";
        return false;
    }
    auto* desc = ItemDescriptorCache::LookupByName(mitemName);
    if (!desc)
    {
        if (errOut) *errOut = "name not found in InventoryConfig";
        return false;
    }

    // Confirm it's a SkinItem before we touch the extension fields —
    // writing color floats over an ArmorItem's SBO containers would
    // corrupt the template.
    SkinItemRaw raw{};
    ReadSkinItemRaw_POD((void*)desc, &raw);
    if (!raw.ok || !raw.vtableOk)
    {
        if (errOut) *errOut = "template is not a SkinItem (vtable mismatch); refusing to write";
        return false;
    }

    SkinItemEditPlan plan{};
    plan.writeTexture = (newTexturePath && newTexturePath[0]);
    plan.texturePath  = newTexturePath;
    plan.writeColor1 = (color1 != kColorSentinel);
    plan.writeColor2 = (color2 != kColorSentinel);
    plan.writeColor3 = (color3 != kColorSentinel);
    plan.color1 = color1; plan.color2 = color2; plan.color3 = color3;
    plan.writeRotU   = !std::isnan(rotU);   plan.rotU   = rotU;
    plan.writeRotV   = !std::isnan(rotV);   plan.rotV   = rotV;
    plan.writeScaleX = !std::isnan(scaleX); plan.scaleX = scaleX;
    plan.writeScaleY = !std::isnan(scaleY); plan.scaleY = scaleY;

    SkinItemEditResult res{};
    ApplySkinItemEdit_POD((void*)desc, &plan, &res);

    // The template is shared — engine re-reads it every frame via the
    // wrapper.first_qword deref. Pinging dirty flag is belt-and-braces
    // but cheap.
    if (auto* am = FindPlayerAM_POD())
    {
        __try { am->m_DirtyFlag = 1; }
        __except (EXCEPTION_EXECUTE_HANDLER) {}
    }

    if (errOut)
    {
        char buf[240];
        std::snprintf(buf, sizeof(buf),
                      "ok (template %p, tex:%s colors:%s floats:%s)%s",
                      (void*)desc,
                      res.wroteTexture ? "y" : "skip",
                      res.wroteColors  ? "y" : "skip",
                      res.wroteFloats  ? "y" : "skip",
                      res.callFailed   ? "  [partial fault]" : "");
        *errOut = buf;
    }
    return !res.callFailed;
}

// ── UI ──────────────────────────────────────────────────────────────────────

namespace
{
    // ImGui ARGB ↔ float-RGBA[4] conversion. SkinItem stores colors as
    // 0xAARRGGBB. ColorEdit4 takes RGBA floats in [0,1].
    void ArgbToRgbaF(std::uint32_t argb, float out[4])
    {
        out[0] = ((argb >> 16) & 0xFF) / 255.0f; // R
        out[1] = ((argb >>  8) & 0xFF) / 255.0f; // G
        out[2] = ( argb        & 0xFF) / 255.0f; // B
        out[3] = ((argb >> 24) & 0xFF) / 255.0f; // A
    }

    std::uint32_t RgbaFToArgb(const float in[4])
    {
        auto clamp8 = [](float f) -> std::uint32_t {
            int v = (int)(f * 255.0f + 0.5f);
            if (v < 0) v = 0; if (v > 255) v = 255;
            return (std::uint32_t)v;
        };
        return (clamp8(in[3]) << 24) | (clamp8(in[0]) << 16)
             | (clamp8(in[1]) <<  8) |  clamp8(in[2]);
    }
}

// Curated camo-name shortcuts for the "Edit Camo Template by Name"
// section. The user can also type any other .mitem base name; this
// list is just convenience. Names verified against the game's
// InventoryConfig taxonomy:
//   b_camo_*  → BackpackSkinItem (bag camos with ID-mask shader)
//   w_camo_*  → WeaponSkinItem
//   pa_camo_* / s_camo_* → PatchSkinItem / other surfaces
static const char* kCamoNamePresets[] =
{
    "b_camo_solid_pink",
    "b_camo_solid_black",
    "b_camo_zebra_white",
    "b_camo_zebra_black",
    "b_camo_stripes_01",
    "b_camo_woodland",
    "b_camo_urban",
    "b_camo_desert",
    "w_camo_solid",
    "w_camo_zebra",
    "w_camo_stripes_01",
};
static const int kCamoNamePresetCount =
    (int)(sizeof(kCamoNamePresets) / sizeof(kCamoNamePresets[0]));

void CamoManager::DrawUI()
{
    // ── Path C (primary): edit a camo template by .mitem name ────────
    //
    // This is the actual "use any camo & add custom camos" path. The
    // user types a .mitem base name (e.g. b_camo_solid_pink); we look
    // it up via InventoryConfig and live-edit the SkinItem descriptor's
    // texture / colors / scale / rotation. The renderer reads these
    // every frame through the wrapper's first_qword (= template ptr)
    // deref, so changes apply immediately even without re-equipping.
    if (ImGui::CollapsingHeader("Edit Camo Template by .mitem Name",
                                ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::TextWrapped(
            "Looks up a camo by its .mitem base name and live-edits its "
            "descriptor: mySkinTexture (the camo texture sampled through "
            "the model's ID mask, usually ending in _c), myColor1/2/3 (the "
            "three region tints the ID mask routes), and myScaleX/Y + "
            "myRotationU/V (UV transform). The engine re-reads on every "
            "frame so the edit shows up immediately on whatever's equipped "
            "with that camo.");

        TD::InventoryConfig* cfg = ItemDescriptorCache::GetCfg();
        ImGui::TextColored(cfg ? ImVec4(0.5f, 1.0f, 0.5f, 1.0f)
                               : ImVec4(1.0f, 0.85f, 0.2f, 1.0f),
                           cfg ? "InventoryConfig: ready"
                               : "InventoryConfig: resolving (open an inventory once)");

        // Preset shortcut combo — clicking copies the name into the
        // editable buffer.
        ImGui::PushItemWidth(280.0f);
        if (ImGui::BeginCombo("Quick pick", "(camo presets)"))
        {
            for (int i = 0; i < kCamoNamePresetCount; ++i)
            {
                if (ImGui::Selectable(kCamoNamePresets[i], false))
                {
                    std::snprintf(m_templateName, sizeof(m_templateName),
                                  "%s", kCamoNamePresets[i]);
                    m_templateLoaded = false;
                }
            }
            ImGui::EndCombo();
        }
        ImGui::PopItemWidth();

        ImGui::PushItemWidth(420.0f);
        ImGui::InputText(".mitem base name", m_templateName, sizeof(m_templateName));
        ImGui::PopItemWidth();

        if (ImGui::Button("Look Up"))
        {
            std::string err;
            m_templateOk = LookupCamoTemplate(m_templateName, &m_templateSeed, &err);
            m_templateResult = err;
            if (m_templateOk)
            {
                std::snprintf(m_templateTextureBuf, sizeof(m_templateTextureBuf),
                              "%s", m_templateSeed.texturePath);
                ArgbToRgbaF(m_templateSeed.color1, m_templateColor1Rgba);
                ArgbToRgbaF(m_templateSeed.color2, m_templateColor2Rgba);
                ArgbToRgbaF(m_templateSeed.color3, m_templateColor3Rgba);
                m_templateRotU   = m_templateSeed.rotU;
                m_templateRotV   = m_templateSeed.rotV;
                m_templateScaleX = m_templateSeed.scaleX;
                m_templateScaleY = m_templateSeed.scaleY;
                m_templateLoaded = true;
            }
        }

        if (m_templateLoaded)
        {
            ImGui::SameLine();
            ImGui::TextDisabled("template @ %p", m_templateSeed.itemPtr);

            ImGui::PushItemWidth(420.0f);
            ImGui::InputText("mySkinTexture", m_templateTextureBuf,
                             sizeof(m_templateTextureBuf));
            ImGui::PopItemWidth();

            ImGui::ColorEdit4("myColor1 (RGBA)", m_templateColor1Rgba);
            ImGui::ColorEdit4("myColor2 (RGBA)", m_templateColor2Rgba);
            ImGui::ColorEdit4("myColor3 (RGBA)", m_templateColor3Rgba);

            ImGui::PushItemWidth(150.0f);
            ImGui::DragFloat("myScaleX",    &m_templateScaleX, 0.05f, 0.01f, 100.0f, "%.2f");
            ImGui::SameLine();
            ImGui::DragFloat("myScaleY",    &m_templateScaleY, 0.05f, 0.01f, 100.0f, "%.2f");
            ImGui::DragFloat("myRotationU", &m_templateRotU,   0.01f, -10.0f, 10.0f, "%.2f");
            ImGui::SameLine();
            ImGui::DragFloat("myRotationV", &m_templateRotV,   0.01f, -10.0f, 10.0f, "%.2f");
            ImGui::PopItemWidth();

            if (ImGui::Button("Apply Template Edit"))
            {
                std::string err;
                std::uint32_t c1 = RgbaFToArgb(m_templateColor1Rgba);
                std::uint32_t c2 = RgbaFToArgb(m_templateColor2Rgba);
                std::uint32_t c3 = RgbaFToArgb(m_templateColor3Rgba);
                m_templateOk = EditCamoTemplate(
                    m_templateName,
                    m_templateTextureBuf[0] ? m_templateTextureBuf : nullptr,
                    c1, c2, c3,
                    m_templateRotU, m_templateRotV,
                    m_templateScaleX, m_templateScaleY,
                    &err);
                m_templateResult = err;
            }
            ImGui::SameLine();
            if (ImGui::Button("Reload From Template"))
            {
                std::string err;
                m_templateOk = LookupCamoTemplate(m_templateName, &m_templateSeed, &err);
                m_templateResult = err;
                if (m_templateOk)
                {
                    std::snprintf(m_templateTextureBuf, sizeof(m_templateTextureBuf),
                                  "%s", m_templateSeed.texturePath);
                    ArgbToRgbaF(m_templateSeed.color1, m_templateColor1Rgba);
                    ArgbToRgbaF(m_templateSeed.color2, m_templateColor2Rgba);
                    ArgbToRgbaF(m_templateSeed.color3, m_templateColor3Rgba);
                    m_templateRotU   = m_templateSeed.rotU;
                    m_templateRotV   = m_templateSeed.rotV;
                    m_templateScaleX = m_templateSeed.scaleX;
                    m_templateScaleY = m_templateSeed.scaleY;
                }
            }
        }

        if (!m_templateResult.empty())
        {
            ImVec4 col = m_templateOk ? ImVec4(0.5f, 1.0f, 0.5f, 1.0f)
                                      : ImVec4(1.0f, 0.5f, 0.5f, 1.0f);
            ImGui::TextColored(col, "%s", m_templateResult.c_str());
        }
    }

    ImGui::Separator();

    // ── Path A: arm-patch / overlay texture (LIMITED SCOPE) ──────────
    if (ImGui::CollapsingHeader("Arm Patch / Overlay Texture (m_PathStrings1)"))
    {
        ImGui::TextWrapped(
            "WARNING: this section targets m_PathStrings1[0..2] — the engine's "
            "single 'active overlay' texture set. On most live characters that "
            "slot is bound to the ARM PATCH (CA_SHD_Armpatch_*.dds), NOT the "
            "backpack. Per-bag camos use a different system (ID-mask + 3 colors "
            "+ texture composed in the model's shader) — use 'Edit Camo Template "
            "by Name' above for that. This section is kept for arm-patch / "
            "vanity-jacket recolors only.");

        // Live readout of the engine's currently-active camo paths. Lets the
        // user see what they're replacing before they click Apply.
        {
            char liveD[260] = {}, liveN[260] = {}, liveM[260] = {};
            ReadActiveArmorCamo(liveD, liveN, liveM);
            ImGui::TextDisabled("Active diffuse: %s", liveD[0] ? liveD : "(empty)");
            ImGui::TextDisabled("Active normal : %s", liveN[0] ? liveN : "(empty)");
            ImGui::TextDisabled("Active mask   : %s", liveM[0] ? liveM : "(empty)");
            if (ImGui::SmallButton("Copy active -> editable"))
            {
                std::snprintf(m_customDiffuse, sizeof(m_customDiffuse), "%s", liveD);
                std::snprintf(m_customNormal,  sizeof(m_customNormal),  "%s", liveN);
                std::snprintf(m_customMask,    sizeof(m_customMask),    "%s", liveM);
                m_selectedPreset = -1;
            }
        }

        // Preset picker
        const char* presetPreview = (m_selectedPreset >= 0 && m_selectedPreset < kPresetCount)
                                    ? kPresets[m_selectedPreset].displayName
                                    : "(custom)";
        ImGui::PushItemWidth(280.0f);
        if (ImGui::BeginCombo("Preset", presetPreview))
        {
            if (ImGui::Selectable("(custom)", m_selectedPreset < 0))
                m_selectedPreset = -1;
            for (int i = 0; i < kPresetCount; ++i)
            {
                bool selected = (m_selectedPreset == i);
                if (ImGui::Selectable(kPresets[i].displayName, selected))
                {
                    m_selectedPreset = i;
                    // Copy preset into the editable fields so the user can
                    // tweak from there without losing the preset baseline.
                    std::snprintf(m_customDiffuse, sizeof(m_customDiffuse),
                                  "%s", kPresets[i].diffusePath);
                    std::snprintf(m_customNormal,  sizeof(m_customNormal),
                                  "%s", kPresets[i].normalPath);
                    std::snprintf(m_customMask,    sizeof(m_customMask),
                                  "%s", kPresets[i].maskPath);
                }
                if (selected) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        ImGui::PopItemWidth();

        ImGui::PushItemWidth(420.0f);
        ImGui::InputText("Diffuse .dds", m_customDiffuse, sizeof(m_customDiffuse));
        ImGui::InputText("Normal  .dds", m_customNormal,  sizeof(m_customNormal));
        ImGui::InputText("Mask    .dds", m_customMask,    sizeof(m_customMask));
        ImGui::PopItemWidth();

        if (ImGui::Button("Apply Armor Camo"))
        {
            std::string err;
            m_armorOk = ApplyArmorCamo(m_customDiffuse, m_customNormal, m_customMask, &err);
            m_armorResult = err;
        }
        ImGui::SameLine();
        if (ImGui::Button("Clear Fields"))
        {
            m_customDiffuse[0] = 0;
            m_customNormal[0]  = 0;
            m_customMask[0]    = 0;
            m_selectedPreset   = -1;
        }

        if (!m_armorResult.empty())
        {
            ImVec4 col = m_armorOk ? ImVec4(0.5f, 1.0f, 0.5f, 1.0f)
                                   : ImVec4(1.0f, 0.5f, 0.5f, 1.0f);
            ImGui::TextColored(col, "%s", m_armorResult.c_str());
        }
    }

    ImGui::Separator();

    // ── Path B: live SkinItem editor ─────────────────────────────────
    if (ImGui::CollapsingHeader("Live Skin Item Editor (color / scale / texture)",
                                ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::TextWrapped(
            "Walks the player's PlayerInventory wrapper table "
            "(PI+0x80..+0x200) for inner items whose vtable matches the "
            "shared SkinItem ctor (g_pBase + 0x2CBA028 = WeaponSkin / "
            "BackpackSkin / PatchSkin). Equip a camo in-game first, then "
            "click Refresh. Edits hit the per-instance fields at +688 / "
            "+736..+760 and persist across our own equip operations.");

        // Refresh cache on demand — cheap (typically 0-2 SkinItems present).
        if (ImGui::Button("Refresh Skin Items"))
        {
            ListSkinItems(m_skinCache);
            m_skinFieldsInitialized = false;
            // Re-validate the selected index.
            if (m_skinSelectedIdx >= (int)m_skinCache.size())
                m_skinSelectedIdx = m_skinCache.empty() ? -1 : 0;
        }
        ImGui::SameLine();
        ImGui::Text("(%d found)", (int)m_skinCache.size());

        if (m_skinCache.empty())
        {
            ImGui::TextDisabled("(no SkinItems found in PlayerInventory by vtable match —");
            ImGui::TextDisabled(" if you have a camo equipped in-game, open Diagnostics below");
            ImGui::TextDisabled(" and check what vtables actually show up at PI+0x80..+0x600)");
        }
        else
        {
            // Picker: one row per detected SkinItem.
            if (m_skinSelectedIdx < 0 || m_skinSelectedIdx >= (int)m_skinCache.size())
                m_skinSelectedIdx = 0;

            char preview[280];
            {
                const auto& s = m_skinCache[m_skinSelectedIdx];
                std::snprintf(preview, sizeof(preview), "PI+0x%X  %s",
                              s.recordIndex,
                              s.texturePath[0] ? s.texturePath : "(empty)");
            }
            ImGui::PushItemWidth(420.0f);
            if (ImGui::BeginCombo("Target", preview))
            {
                for (int i = 0; i < (int)m_skinCache.size(); ++i)
                {
                    const auto& s = m_skinCache[i];
                    char label[300];
                    std::snprintf(label, sizeof(label), "#%d  %s",
                                  s.recordIndex,
                                  s.texturePath[0] ? s.texturePath : "(empty)");
                    bool selected = (m_skinSelectedIdx == i);
                    if (ImGui::Selectable(label, selected))
                    {
                        m_skinSelectedIdx = i;
                        m_skinFieldsInitialized = false;
                    }
                    if (selected) ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
            ImGui::PopItemWidth();

            // On first display of a target — or after a refresh — seed the
            // edit widgets from the live values. After that the user owns
            // the widgets; we don't clobber their typing on every frame.
            if (!m_skinFieldsInitialized && m_skinSelectedIdx >= 0
                && m_skinSelectedIdx < (int)m_skinCache.size())
            {
                const auto& s = m_skinCache[m_skinSelectedIdx];
                std::snprintf(m_skinTextureBuf, sizeof(m_skinTextureBuf),
                              "%s", s.texturePath);
                ArgbToRgbaF(s.color1, m_skinColor1Rgba);
                ArgbToRgbaF(s.color2, m_skinColor2Rgba);
                ArgbToRgbaF(s.color3, m_skinColor3Rgba);
                m_skinRotU   = s.rotU;
                m_skinRotV   = s.rotV;
                m_skinScaleX = s.scaleX;
                m_skinScaleY = s.scaleY;
                m_skinFieldsInitialized = true;
            }

            ImGui::PushItemWidth(420.0f);
            ImGui::InputText("mySkinTexture", m_skinTextureBuf, sizeof(m_skinTextureBuf));
            ImGui::PopItemWidth();

            ImGui::ColorEdit4("myColor1 (RGBA)", m_skinColor1Rgba);
            ImGui::ColorEdit4("myColor2 (RGBA)", m_skinColor2Rgba);
            ImGui::ColorEdit4("myColor3 (RGBA)", m_skinColor3Rgba);

            ImGui::PushItemWidth(150.0f);
            ImGui::DragFloat("myScaleX",    &m_skinScaleX, 0.05f, 0.01f, 100.0f, "%.2f");
            ImGui::SameLine();
            ImGui::DragFloat("myScaleY",    &m_skinScaleY, 0.05f, 0.01f, 100.0f, "%.2f");
            ImGui::DragFloat("myRotationU", &m_skinRotU,   0.01f, -10.0f, 10.0f, "%.2f");
            ImGui::SameLine();
            ImGui::DragFloat("myRotationV", &m_skinRotV,   0.01f, -10.0f, 10.0f, "%.2f");
            ImGui::PopItemWidth();

            if (ImGui::Button("Apply Skin Edit"))
            {
                std::string err;
                const auto& s = m_skinCache[m_skinSelectedIdx];
                std::uint32_t c1 = RgbaFToArgb(m_skinColor1Rgba);
                std::uint32_t c2 = RgbaFToArgb(m_skinColor2Rgba);
                std::uint32_t c3 = RgbaFToArgb(m_skinColor3Rgba);
                m_skinOk = ApplySkinItemEdit(
                    s.recordIndex,
                    m_skinTextureBuf[0] ? m_skinTextureBuf : nullptr,
                    c1, c2, c3,
                    m_skinRotU, m_skinRotV,
                    m_skinScaleX, m_skinScaleY,
                    &err);
                m_skinResult = err;

                // Re-pull live values so the preview combo + initial seed
                // reflect what's now in memory.
                ListSkinItems(m_skinCache);
                m_skinFieldsInitialized = false;
            }
            ImGui::SameLine();
            if (ImGui::Button("Revert Edits to Live"))
                m_skinFieldsInitialized = false;

            if (!m_skinResult.empty())
            {
                ImVec4 col = m_skinOk ? ImVec4(0.5f, 1.0f, 0.5f, 1.0f)
                                      : ImVec4(1.0f, 0.5f, 0.5f, 1.0f);
                ImGui::TextColored(col, "%s", m_skinResult.c_str());
            }
        }
    }

    ImGui::Separator();

    // ── Diagnostic: dump PlayerInventory holders so we can see what
    // vtables are actually present when Refresh comes back empty. ────
    if (ImGui::CollapsingHeader("Diagnostics: PlayerInventory dump"))
    {
        if (ImGui::Button("Dump PI inners"))
        {
            DumpPlayerInventory(m_piDump);
        }
        ImGui::SameLine();
        ImGui::Text("(%d holders found)", (int)m_piDump.size());
        ImGui::TextDisabled("SkinItem vtable RVA reference: 0x2CBA028");
        ImGui::TextDisabled("ArmorItem vtable RVA reference: 0x2CA74A0");
        ImGui::TextDisabled("Item (generic) vtable RVA: 0x2CB9E38");

        if (!m_piDump.empty())
        {
            // Plain monospaced-style dump — Tables API isn't available in
            // this fork of ImGui. Header row + one row per entry, fixed
            // column widths via space-padding so they line up.
            ImGui::TextDisabled("PI off    vtable RVA   slot   tag   name");
            for (const auto& e : m_piDump)
            {
                ImGui::Text("+0x%-6X 0x%-10llX %-6d %-5d %s",
                            e.holderOffset,
                            (unsigned long long)e.vtableRVA,
                            e.slotId,
                            e.assetTypeTag,
                            e.name[0] ? e.name : "(unnamed)");
            }
        }
    }
}
