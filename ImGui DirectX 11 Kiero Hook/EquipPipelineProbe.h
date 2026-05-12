#pragma once
#include <cstdint>

// Empirical test of the engine's high-level equip pipeline
// (sub_162DB80 / AppearanceManager_SetEquippedItems).
//
// Why this exists: the deep RE pass on 2026-05-12 surfaced a structural
// blocker — InventoryConfig templates may not be valid list entries for
// sub_162DB80 because BindItemToSlot reads slot id at
// (entry.first_qword)+0x40, which on templates points into the static
// vtable's inline string data (not an int). Reading more disasm cannot
// verify this hypothesis. Calling sub_162DB80 with a one-template list
// and inspecting the result will. See
// .claude/docs/06-inventory-equip-pipeline.md
// "Structural blocker discovered 2026-05-12 — Templates ≠ EquipInstances"
// for the full background.
//
// The probe does ONE attempted call wrapped in SEH. It does not attempt
// rollback if the call partially succeeds — the caller (UI) should warn
// the user that running the probe may corrupt outfit state until next
// engine refresh / character reload.
namespace EquipPipelineProbe
{
    // Captures everything we need to diagnose what happens when we call
    // sub_162DB80(am, &list) with a single-entry list pointing at the
    // template Item* returned by ItemDescriptorCache::LookupByName.
    //
    // POD only — no destructors, no std::string. The probe runs under SEH
    // so it must stay POD.
    struct Result
    {
        // ── Pre-flight: did we find an item to test with? ───────────────
        bool          itemFound;            // LookupByName returned non-null
        void*         itemPtr;              // the template Item*
        int           templateSlotField40;  // *(int*)(itemPtr + 0x40)
        int           templateTypeFieldA8;  // *(int*)(itemPtr + 0xA8)
        void*         templateFirstQword;   // *(void**)itemPtr  (likely vtable)

        // ── Pre-flight: is the AppearanceManager reachable? ─────────────
        bool          amFound;
        void*         amPtr;
        int           targetSlot;           // slot we expect (from template field 0x40)

        // ── Snapshot before the call ────────────────────────────────────
        int           preAssetRecordsCount;
        void*         preSlotPSlot;         // m_Clothes[targetSlot].m_pSlot
        std::uint8_t  preDirtyFlag;
        std::uint8_t  preListUpdated;
        std::uint8_t  preNeedsResync;

        // ── The call attempt ─────────────────────────────────────────────
        bool          callAttempted;
        bool          callReturned;         // true if no SEH exception
        unsigned long exceptionCode;        // 0xC0000005 = AV
        void*         exceptionAddress;     // EIP/RIP at the fault
        void*         faultAddress;         // address being read/written
        int           faultType;            // EXCEPTION_RECORD::ExceptionInformation[0]
                                            //   0=read, 1=write, 8=DEP

        // ── Snapshot after the call (only meaningful if callReturned) ───
        int           postAssetRecordsCount;
        void*         postSlotPSlot;
        std::uint8_t  postDirtyFlag;
        std::uint8_t  postListUpdated;
        std::uint8_t  postNeedsResync;

        // Diagnostic message — assembled by RunEquipTest from the fields
        // above into a single human-readable summary.
        char          summary[1024];
    };

    // Runs the probe with `itemName` (a .mitem base name, e.g.
    // "ch_pm_mask_ge3_03"). Returns true if the probe ran end to end —
    // false only on setup failures (no AppearanceManager, item not in
    // cache). An SEH exception during the engine call counts as a
    // "successful run" of the probe (the result fields tell you what
    // happened) and returns true.
    //
    // The probe can crash the game; the SEH guard catches in-thread AVs
    // on the calling thread but cannot protect against engine-internal
    // corruption that surfaces on a different thread or at the next
    // render frame. Treat each run as one-shot: if it completes without
    // visible glitching, you can run it again; if the game stutters or
    // glitches afterwards, restart the character before re-probing.
    //
    // ── V1 ──────────────────────────────────────────────────────────────
    // Pass the InventoryConfig template directly as a list entry. Verified
    // 2026-05-12 to AV at Character_ApplyClothingId because the engine
    // reads slot id via (*(QWORD*)entry)+0x40 which on a template equals
    // vtable+0x40 = inline string data, not an int.
    bool RunEquipTest(const char* itemName, Result* out);

    // ── V2 / Pattern A ──────────────────────────────────────────────────
    // Clone an existing equipped-wrapper from PlayerInventory and swap its
    // `+0x00` (inner Item*) to our target template. The wrapper layout
    // (verified 2026-05-12 live in ReClass — see "Where EquipInstances live"
    // in 06-inventory-equip-pipeline.md) puts the Item* at +0x00 and the
    // engine's `(*(QWORD*)entry) + 0x40` deref then correctly lands on the
    // template's real slot id.
    //
    // The cloned buffer is a static 1 KB store internal to this probe — no
    // heap alloc, no leak; only one Pattern A test in flight at a time.
    // The engine's `vector_void_assign` copies the pointer into
    // m_AssetRecords on success, so the buffer must survive past the call;
    // it does (static = process lifetime). Subsequent UI equips will
    // overwrite m_AssetRecords and our buffer becomes orphaned (still alive
    // but unreferenced).
    bool RunEquipTestPatternA(const char* itemName, Result* out);

    // ── V3 / Pattern A+ ─────────────────────────────────────────────────
    // Same as Pattern A, but immediately after the call returns we write 0
    // to the three change-tracking flags on the AppearanceManager:
    //   am+0x4E0  m_DirtyFlag
    //   am+0x4E1  m_ListUpdated
    //   am+0x4E2  m_NeedsResync
    //
    // Hypothesis: the engine's revert tick (which overwrote our equip after
    // one frame in the bare Pattern A test) fires on a transition of one of
    // these flags. Clearing them immediately after the call should suppress
    // the revert — at the cost of the renderer also not consuming our
    // m_DirtyFlag, which means the mesh won't appear at all if dirty=0
    // before the consume frame.
    //
    // Two delivery modes to compare:
    //   clearFlagsAfter = true   → clear all three flags right after the call
    //   clearFlagsAfter = false  → same as Pattern A (control / sanity-check)
    bool RunEquipTestPatternAPlus(const char* itemName,
                                  bool clearFlagsAfter,
                                  Result* out);

    // Per-frame maintenance: detects when a Pattern A+ injected wrapper has
    // been replaced in m_AssetRecords (e.g. the player UI-equipped a real
    // item in the same slot), and removes our orphan AttachHashmap bucket
    // so the old mesh stops rendering alongside the new one.
    //
    // Why this is needed: AppearanceManager_ModelLoadTrigger writes the
    // slot id to bucket+0x2C, but Character_ApplyClothingId (the engine's
    // bucket cleanup pass) reads bucket+0x3C. So the engine's normal
    // cleanup can't identify Pattern A+ buckets as belonging to the slot.
    // The bucket leaks and the renderer keeps rendering its mesh until
    // we remove it ourselves.
    //
    // Cheap to call every frame — most frames it's a no-op (no tracked
    // injections, or our wrapper still present). Safe to call when am is
    // null (returns immediately).
    void MaintainInjections(void* appearanceManager);

    // Remove every Pattern A+ injection's orphan bucket immediately.
    // Useful for "Cleanup" UI button. After this returns, no Pattern A+
    // injections are tracked anymore.
    void ClearAllInjections(void* appearanceManager);
}
