#pragma once
#include <cstdint>
#include <string>
#include <vector>

// CamoManager — texture-override / skin-item editor.
//
// Covers the two verified camo primitives (storage mapped by reversing
// sub_15A36D0 / AppearanceManager_BindItemToSlot + the EquipInstance
// layout in 06-inventory-equip-pipeline.md):
//
//   Path A (engine-native armor recolor) — writes .dds paths into
//   AppearanceManager.m_PathStrings1[0..2] (the BASE slots — what the
//   renderer samples) and flips m_DirtyFlag so the consume pipeline
//   picks them up. This is exactly what the camo branch of sub_15A36D0
//   does when the engine processes a camo wrapper.
//
//   Path B (live SkinItem edit) — walks the player's PlayerInventory
//   equipped-holder table (PI+0x80..+0x200) for a wrapper whose inner
//   item has the SkinItem vtable (g_pBase + 0x2CBA028 — set by the
//   shared 768-byte ctor sub_E39140), then live-tweaks the camo
//   descriptor fields at +688 / +736..+760 (texture path, rotU/V,
//   scaleX/Y, color1/2/3).
//
// Camo items DO NOT appear in m_AssetRecords. The engine's camo branch
// only updates m_PathStrings1 + dirty flag; it never pushes an asset
// record. So enumeration scans the equipped-holder table instead.

class CamoManager
{
public:
    // Curated armor-recolor preset. The three .dds paths drop into
    // m_PathStrings1[3..5] (override diffuse/normal/mask).
    struct ArmorCamoPreset
    {
        const char* displayName;
        const char* diffusePath;
        const char* normalPath;
        const char* maskPath;
    };

    CamoManager();
    ~CamoManager();

    void Update();
    void DrawUI();

    // ── Armor texture override (engine-native recolor) ──────────────
    // Writes the three .dds paths directly into m_PathStrings1[0..2]
    // (BASE slots at am+0x480/+0x490/+0x4A0) and sets m_DirtyFlag = 1.
    // This mirrors sub_15A36D0's camo branch exactly — no sub_1542C60
    // step needed for runtime apply. Pass nullptr/"" to leave a slot
    // at its current value.
    static bool ApplyArmorCamo(const char* diffuseDds,
                               const char* normalDds,
                               const char* maskDds,
                               std::string* errOut = nullptr);

    // Read the player AM's currently-active base-slot camo paths so
    // the UI can show "what's painted on the character right now".
    static void ReadActiveArmorCamo(char outDiffuse[260],
                                    char outNormal[260],
                                    char outMask[260]);

    // Live-mutate a SkinItem (WeaponSkinItem / BackpackSkinItem /
    // PatchSkinItem) located in the player's PlayerInventory wrapper
    // table. holderOffset is the PI offset (+0x80, +0x88, …, +0x1F8)
    // returned in SkinItemSnapshot::recordIndex; pass -1 to target
    // the first SkinItem found. Fields with sentinel values are
    // skipped:
    //   newTexturePath == nullptr / ""  → no texture write
    //   color* == 0xFFFFFFFE             → no color write
    //   rot* / scale* == NaN             → no float write
    static bool ApplySkinItemEdit(int holderOffset,
                                  const char* newTexturePath,
                                  std::uint32_t color1,
                                  std::uint32_t color2,
                                  std::uint32_t color3,
                                  float rotU, float rotV,
                                  float scaleX, float scaleY,
                                  std::string* errOut = nullptr);

    // POD descriptor for one detected SkinItem in PlayerInventory.
    // Returned in bulk via ListSkinItems so the UI can show a picker.
    struct SkinItemSnapshot
    {
        int            recordIndex;            // PI holder offset (0x80, 0x88, …)
        void*          itemPtr;                // inner Item* (per-instance)
        char           texturePath[260];       // +688
        float          rotU;                   // +736
        float          rotV;                   // +740
        float          scaleX;                 // +744
        float          scaleY;                 // +748
        std::uint32_t  color1;                 // +752 ARGB
        std::uint32_t  color2;                 // +756
        std::uint32_t  color3;                 // +760
    };
    // Scans PlayerInventory holders and returns one entry per
    // wrapper whose inner item has the SkinItem vtable
    // (g_pBase + 0x2CBA028).
    static void ListSkinItems(std::vector<SkinItemSnapshot>& out);

    // POD diagnostic — one entry per inner item found in
    // PlayerInventory+0x80..+0x600. Used by the UI "Dump PI" button
    // so we can see what's actually in the equipped table when
    // Refresh comes back empty.
    struct PiEntryRaw
    {
        int           holderOffset;       // +0x80, +0x88, …
        void*         inner;              // wrapper.first_qword
        std::uint64_t vtableRVA;          // inner.vtable - g_pBase  (so it's stable across loads)
        int           slotId;             // inner.+0x40
        int           assetTypeTag;       // inner.+168
        char          name[160];          // inner.+24 SnowdropString (item base name)
    };
    static void DumpPlayerInventory(std::vector<PiEntryRaw>& out);

    // ── Path C: edit a camo template by .mitem name ────────────────
    // Uses ItemDescriptorCache::LookupByName to resolve the template
    // pointer (works for any camo: "b_camo_zebra_white",
    // "w_camo_solid", patterns, etc.), then live-edits its descriptor
    // fields. The engine re-reads these fields each render frame for
    // shader binding, so changes apply immediately — no re-equip
    // needed when the camo is already on. For un-equipped camos, the
    // user just equips it in-game afterward and our edits ship with it.
    static bool LookupCamoTemplate(const char* mitemName,
                                   SkinItemSnapshot* out,
                                   std::string* errOut = nullptr);

    static bool EditCamoTemplate(const char* mitemName,
                                 const char* newTexturePath,
                                 std::uint32_t color1,
                                 std::uint32_t color2,
                                 std::uint32_t color3,
                                 float rotU, float rotV,
                                 float scaleX, float scaleY,
                                 std::string* errOut = nullptr);

private:
    // Persistent UI state
    int  m_selectedPreset      = -1;
    char m_customDiffuse[260]  = {};
    char m_customNormal[260]   = {};
    char m_customMask[260]     = {};

    // SkinItem editor state — sticky across frames so the user can
    // tweak without the auto-refresh blowing away their typing.
    int           m_skinSelectedIdx = -1;       // -1 = none picked
    char          m_skinTextureBuf[260] = {};
    float         m_skinColor1Rgba[4]   = {1,1,1,1};
    float         m_skinColor2Rgba[4]   = {1,1,1,1};
    float         m_skinColor3Rgba[4]   = {1,1,1,1};
    float         m_skinRotU  = 0.0f;
    float         m_skinRotV  = 0.0f;
    float         m_skinScaleX = 1.0f;
    float         m_skinScaleY = 1.0f;
    bool          m_skinFieldsInitialized = false;

    std::string m_armorResult;
    bool        m_armorOk = false;
    std::string m_skinResult;
    bool        m_skinOk = false;

    std::vector<SkinItemSnapshot> m_skinCache;

    // Path C state — edit camo template by name.
    char          m_templateName[128]   = {};
    bool          m_templateLoaded       = false;
    SkinItemSnapshot m_templateSeed{};
    char          m_templateTextureBuf[260] = {};
    float         m_templateColor1Rgba[4]   = {1,1,1,1};
    float         m_templateColor2Rgba[4]   = {1,1,1,1};
    float         m_templateColor3Rgba[4]   = {1,1,1,1};
    float         m_templateRotU  = 0.0f;
    float         m_templateRotV  = 0.0f;
    float         m_templateScaleX = 1.0f;
    float         m_templateScaleY = 1.0f;
    std::string   m_templateResult;
    bool          m_templateOk = false;

    // Diagnostic — populated by the "Dump PI" debug button.
    std::vector<PiEntryRaw> m_piDump;

public:
    CamoManager(CamoManager const&) = delete;
    void operator=(CamoManager const&) = delete;
};
