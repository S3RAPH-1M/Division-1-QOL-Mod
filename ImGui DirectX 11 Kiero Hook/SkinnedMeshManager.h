#pragma once
#include <string>
#include <vector>
#include <cstddef>

class SkinnedMeshManager
{
public:
    // Gear-type taxonomy derived from the asset path prefix
    // (verified against live AppearanceManager dumps).
    //   ca_<g>_b_*    Backpack
    //   ca_<g>_l1_*   Shirt
    //   ca_<g>_l2_*   Chestplate
    //   ca_<g>_l3_*   Jacket
    //   ca_<g>_p_*    Pants
    //   ca_<g>_t_*    Thigh
    //   ca_<g>_f_*    Feet
    //   ca_<g>_s_*    Scarf
    //   ca_<g>_k_*    Kneepads
    //   ca_<g>_h_*    Hat   (or Gloves if path contains "_gv_" / "gloves")
    //   ca_hg_* / cp_hg_* / ch_pm_mask_*  Gas Mask
    //
    // NOTE: path-prefix classification is now a fallback. Slot indices in
    // m_Clothes[27] are deterministic on this build — see SlotGearType().
    enum class GearType
    {
        Unknown = 0,
        Backpack,
        Shirt,
        Chestplate,
        Jacket,
        Pants,
        Thigh,
        Feet,
        Scarf,
        Kneepads,
        Hat,
        Gloves,
        GasMask,
        CosmeticMask,
        _Count
    };

    struct ModelSwapEntry
    {
        const char* displayName;
        const char* assetPath;
    };

    // One entry per populated slot in AppearanceManager::m_Clothes[27].
    // Refreshed by Update() each frame.
    struct LiveSlot
    {
        int         index;          // 0..26 — index in m_Clothes
        GearType    type;
        std::string currentPath;    // copy of cached path string
        std::size_t capacity;       // heap-string capacity (0 if inline / unmutatable)
        bool        canMutate;      // capacity > 0 and path is heap-allocated
    };

    SkinnedMeshManager();
    ~SkinnedMeshManager();

    void Update();                  // refresh m_slots; cheap per frame
    void DrawUI();

    // Routes a slot swap through the engine's full equip pipeline so the
    // result is 1:1 with an in-game equip (proper Item* in m_AssetRecords,
    // old Item* dropped, model rebuilt by the factory). Mirrors
    // sub_162DD60 (Character_SetClothingIdList): sets m_ListUpdated → optional
    // sync (sub_16083F0) → ApplyClothingId reset → drops the old Item* by
    // path match → injects our path → ModelLoadTrigger inserts the attach
    // bucket → sets m_NeedsResync + m_DirtyFlag last. The engine's next
    // consume frame loads the model and the item factory (sub_F48FE0)
    // creates the new Item subclass.
    // Returns true on success; writes a diagnostic into *errOut.
    //
    // ★ LEGACY PATH ★ — This path-only flow misses the descriptor side
    // effects (head/hair swap on cosmetic masks, L1/L2 covered-mesh
    // selection on jackets). Prefer ApplyEquipByName below; ApplyDirectSwap
    // remains as a fallback for items not in InventoryConfig (custom
    // paths, mods, etc.).
    static bool ApplyDirectSwap(int slotIndex, const char* newPath,
                                std::string* errOut = nullptr);

    // ★ Production equip path (2026-05-12) ★
    // Resolves `mitemName` via ItemDescriptorCache, then drives the engine's
    // full descriptor-bound equip pipeline (Pattern A+ algorithm — clone an
    // existing PlayerInventory wrapper, retarget +0x00 to our chosen Item*,
    // call sub_162DB80, clear AppearanceManager flags to suppress the
    // engine's revert tick). Per-frame orphan-bucket cleanup is wired into
    // Update() automatically.
    //
    // This is the ONLY path that produces correct visual side effects —
    // cosmetic mask head/hair swap, L1/L2/L3 covered-mesh selection, etc.
    // It also avoids the "duplicates on multi-equip" symptom because the
    // engine atomically replaces m_AssetRecords through sub_162DB80.
    //
    // Returns true on success; writes a diagnostic into *errOut.
    // Returns false if `mitemName` is not in InventoryConfig (caller
    // should fall back to ApplyDirectSwap or report to user).
    //
    // Full algorithm reference: .claude/docs/06-inventory-equip-pipeline.md
    //                           "The shipping algorithm (verified 2026-05-12)"
    static bool ApplyEquipByName(int slotIndex, const char* mitemName,
                                 std::string* errOut = nullptr);

    static GearType    ClassifyPath(const char* path);
    static const char* GearTypeName(GearType t);

    // Fixed slot → gear-type mapping. Verified live on this build: each
    // index in m_Clothes[27] consistently lands on the same body part /
    // outfit role regardless of the asset paths populating it. Indices
    // above kKnownSlotCount return GearType::Unknown.
    static constexpr int kKnownSlotCount = 13;   // slots 0..12 are named
    static GearType    SlotGearType(int slotIndex);

private:
    void ScanLiveSlots();
    void SoftRevertOnEngineActivity();
    void AutoReapplyOnDrift();

    std::vector<LiveSlot>     m_slots;
    std::string               m_scanError;     // empty if last scan was clean

public:
    SkinnedMeshManager(SkinnedMeshManager const&) = delete;
    void operator=(SkinnedMeshManager const&) = delete;
};
