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

    void Update();                  // refresh m_slots; cheap to call every frame
    void DrawUI();

    // Direct in-place mutation of m_Clothes[slotIndex].m_Path.
    //   slotIndex   : 0..26 (must be a populated slot per ScanLiveSlots)
    //   newPath     : null-terminated UTF-8 model path
    //   errOut      : optional human-readable failure reason
    // Returns true on success. After success, sets m_DirtyFlag / m_ListUpdated /
    // m_NeedsResync on the AppearanceManager. Visual reload may still require
    // re-equip or zone-change depending on the engine's consumption pipeline.
    static bool ApplyDirectSwap(int slotIndex, const char* newPath,
                                std::string* errOut = nullptr);

    static GearType    ClassifyPath(const char* path);
    static const char* GearTypeName(GearType t);

private:
    void ScanLiveSlots();
    void DrawSlotGroup(GearType type, const ModelSwapEntry* models, int modelCount);

    std::vector<LiveSlot> m_slots;
    std::string           m_scanError;     // empty if last scan was clean

public:
    SkinnedMeshManager(SkinnedMeshManager const&) = delete;
    void operator=(SkinnedMeshManager const&) = delete;
};
