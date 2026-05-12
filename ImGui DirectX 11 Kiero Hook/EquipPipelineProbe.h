#pragma once
#include <cstdint>
#include <cstddef>

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

    // No-op kept for source compatibility — auto re-injection caused
    // visible flicker on engine-driven SetEquippedItems passes
    // (weapon swaps, animation state changes), so re-application is
    // now user-driven via ReapplyAllInjections() / the "Apply All" UI
    // button. Safe to call every frame; does nothing.
    void MaintainInjections(void* appearanceManager);

    // Re-apply every currently tracked Pattern A+ injection in a single
    // sub_162DB80 call. Use this when the engine has clobbered our
    // wrappers (slots gone invisible after an in-game equip) and the
    // user wants everything visible again. Returns the number of
    // injections re-applied; 0 if nothing was active or the call faulted.
    int ReapplyAllInjections();

    // Replay the original outfit captured by the first RunEquipBatch call,
    // then clear all tracked injections. Returns the number of original
    // wrappers replayed (also implies success); 0 if no snapshot exists
    // or the engine call faulted.
    int  RestoreOriginalOutfit();
    bool HasOriginalSnapshot();

    // Force-capture a fresh snapshot of the live m_AssetRecords NOW. The
    // first RunEquipBatch call takes one automatically; this lets the
    // user re-pin if they want to overwrite the auto-capture (e.g. after
    // a character reload that rebuilt the wrapper set). Returns the
    // wrapper count captured, or 0 on failure.
    int  TakeOriginalSnapshotNow();

    // Batch-apply many slots in one engine call. For each (slot, name)
    // pair: resolve the .mitem name via ItemDescriptorCache, stage a
    // retargeted clone, then fire ONE sub_162DB80 call carrying every
    // staged wrapper PLUS any earlier injections still tracked. Single
    // animation reset for the whole batch — the "Apply All" UI button's
    // backend. Returns the count of entries successfully staged; 0 if
    // the engine call AV'd or nothing resolved. On failure, `err` (if
    // non-null) receives a short diagnostic.
    int RunEquipBatch(const int* slots, const char* const* names, int n,
                      char* err, std::size_t errSize);

    // Remove every Pattern A+ injection's orphan bucket immediately.
    // Useful for "Cleanup" UI button. After this returns, no Pattern A+
    // injections are tracked anymore.
    void ClearAllInjections(void* appearanceManager);
}
