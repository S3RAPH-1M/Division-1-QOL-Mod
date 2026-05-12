#pragma once
#include <cstddef>
#include <cstdint>

// Forward decls in the TD namespace. The real class definitions live in
// Snowdrop.h. Field offsets (myEquipmentSlot at +0x40, myAttributeGenType
// at +0x44, myMaleVisualGearName at +0x170, etc.) are accessed via the
// typed helpers below so callers don't have to know the layout.
namespace TD
{
    class ItemDescriptor;
    class InventoryConfig;
}

// Resolves .mitem item names to engine ItemDescriptor* pointers by capturing
// the live InventoryConfig pointer at runtime.
//
// Why this exists: the engine's clothing-equip pipeline is descriptor-driven —
// to fully equip an item (so render-time side effects like layered-clothing
// coverage and the cosmetic-mask head/hair swap fire), we need the .mitem
// descriptor pointer, not just a .mgraphobject path. Descriptors live in a
// global InventoryConfig cache the engine builds at startup (sub_F49920).
//
// We can't easily find the InventoryConfig pointer statically — neither
// qword_45026D0 nor qword_44DD2B0 points to it directly, and the upstream
// chain via sub_CEA470 is buried behind multiple wrappers. So instead we
// hook the engine's own name-lookup function (sub_F07C40 at g_pBase +
// 0xF07C40), which the game calls constantly during normal play (UI hover,
// inventory list, item description). The first call hands us the cfg
// pointer for free. We keep the hook installed and trampoline-forward all
// subsequent calls untouched.
namespace ItemDescriptorCache
{
    // One-time init. Currently a no-op — kept for symmetry with other
    // managers. The actual InventoryConfig capture is on-demand via
    // TryCapture(), since scanning is expensive and would block startup.
    bool Init();

    // Scans the game's heap for the InventoryConfig struct signature and
    // captures the first validated pointer. Blocks for several seconds
    // (walks hundreds of MB of process memory) so call from a UI button,
    // not from a hot path. Idempotent once successful — subsequent calls
    // are no-ops.
    //
    // Returns true if cfg was captured (either by this call or a prior one).
    // Why scanning instead of hooking: ACG (Arbitrary Code Guard) blocks
    // VirtualProtect on the game's .text page (ERROR_ACCESS_DENIED), so
    // we can't install a function hook. Calling functions still works fine,
    // and InventoryConfig has a fingerprint-able layout, so we scan.
    bool TryCapture();

    // Returns the captured InventoryConfig pointer, or nullptr if the engine
    // hasn't called sub_F07C40 yet (typically captured within a few frames
    // of the player loading into the world).
    TD::InventoryConfig* GetCfg();

    // Diagnostic accessors for the probe UI.
    bool          IsHookInstalled();       // true if MH_EnableHook succeeded
    std::uint64_t GetHookCallCount();      // total hook invocations
    std::uint64_t GetHookNullCfgCount();   // calls that came in with cfg=NULL

    // Returns the last MH_STATUS as a string ("MH_OK", "MH_ERROR_*", or "unset").
    // Use after Init() to diagnose which step failed when IsHookInstalled() == false.
    const char*   GetLastStatusName();
    // The actual integer MH_STATUS value of whichever step failed last
    // (MH_CreateHook or MH_EnableHook). -1 if Init wasn't called.
    int           GetLastStatusCode();
    // Which step recorded the last status — "MH_Initialize", "MH_CreateHook",
    // "MH_EnableHook", or "unset".
    const char*   GetLastStepName();

    // The target address the hook tries to patch (g_pBase + RVA). For
    // diagnostics — confirms we're aiming at the right address.
    void*         GetHookTargetAddress();

    // Snapshot of VirtualQuery state captured during Init. All fields zero
    // if Init wasn't reached or the query itself failed.
    struct PageDiag
    {
        unsigned long long allocationBase;
        unsigned long      state;       // MEM_COMMIT etc.
        unsigned long      protect;     // PAGE_EXECUTE_READ etc.
        unsigned long      type;        // MEM_IMAGE etc.
        unsigned long      vqLastError; // GetLastError if VirtualQuery returned 0
        unsigned long      vpLastError; // GetLastError if our VirtualProtect failed
    };
    PageDiag      GetPageDiag();

    // Look up a .mitem item by its base name (e.g. "ch_pm_mask_ge3_03" or
    // "player_jacket_premade_template"). Returns the descriptor pointer or
    // nullptr if not found, cfg not yet captured, or the lookup raised an
    // access violation. Calls the engine's own sub_F07C40 internally so
    // results match what an inventory equip would see.
    TD::ItemDescriptor* LookupByName(const char* itemName);

    // ── typed accessors over ItemDescriptor fields ─────────────────────────
    // All return safe defaults (0 / nullptr / empty string) when desc is null
    // or the read AVs. Field offsets verified against sub_F2FD40 (ArmorItem
    // descriptor parser).

    // Equipment slot enum value — matches the engine's enum at 0x2CA04E0:
    // Back=0, Chest=1, Face=2, Hands=3, Knees=4, Thighs=5, Hat=6, Jacket=7,
    // Pants=8, Scarf=9, Shirt=10, Shoes=11, BalaclavaMask=12, etc.
    int GetEquipmentSlot(TD::ItemDescriptor* desc);

    // Attribute generation type (drives render side effects — e.g.
    // cosmetic masks have this set to Hat, which triggers head/hair swap).
    int GetAttributeGenType(TD::ItemDescriptor* desc);

    // Inventory category (often duplicates EquipmentSlot but not always).
    int GetInventoryCategory(TD::ItemDescriptor* desc);

    // Reads the male/female visual-gear path SnowdropStrings into outBuf.
    // Returns outBuf on success or an empty string on failure. Handles
    // both inline and heap SnowdropString modes.
    const char* GetMaleVisualGearPath(TD::ItemDescriptor* desc, char* outBuf, std::size_t outSize);
    const char* GetFemaleVisualGearPath(TD::ItemDescriptor* desc, char* outBuf, std::size_t outSize);
}
