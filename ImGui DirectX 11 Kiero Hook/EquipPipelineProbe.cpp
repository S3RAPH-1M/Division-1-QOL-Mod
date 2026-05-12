#include "EquipPipelineProbe.h"
#include "ItemDescriptorCache.h"
#include "Snowdrop.h"

#include <Windows.h>
#include <cstdint>
#include <cstdio>
#include <cstring>

// 16-byte item-list header that mirrors the engine's vector<Item*> ABI
// (matches AppearanceManager::m_AssetRecords layout — see Snowdrop.h).
//
//   +0x00  ptr        Item** to first entry (8 bytes)
//   +0x08  count      number of entries
//   +0x0C  cap_flags  capacity, high bit = inline-mode flag.
//                     Pass capacity == count, no inline bit, so the engine's
//                     vector_void_assign treats us as a heap-array source.
namespace
{
    struct EngineItemList
    {
        void**        ptr;
        std::int32_t  count;
        std::uint32_t cap_flags;
    };
    static_assert(sizeof(EngineItemList) == 16, "list header must match engine layout");

    // POD container for the SEH-guarded call site. Anything we want to
    // read about the exception has to come out of here — we cannot host
    // SEH in any function that has a C++ object with a destructor live
    // (MSVC C2712).
    struct CallOutcome
    {
        bool          returned;
        unsigned long exceptionCode;
        void*         exceptionAddress;
        void*         faultAddress;
        int           faultType;
    };

    // Thin SEH wrapper around the engine call. Lives in this anonymous
    // namespace so the C++-aware caller (RunEquipTest) doesn't have to
    // host __try itself.
    CallOutcome CallSetEquippedItemsGuarded(void* am, void* listHeader)
    {
        typedef void (__fastcall *PFN)(void* am, void* list);
        PFN fn = (PFN)(g_pBase + 0x162DB80);

        CallOutcome out{};
        out.returned         = false;
        out.exceptionCode    = 0;
        out.exceptionAddress = nullptr;
        out.faultAddress     = nullptr;
        out.faultType        = -1;

        EXCEPTION_RECORD rec{};
        __try
        {
            fn(am, listHeader);
            out.returned = true;
        }
        __except (rec = *GetExceptionInformation()->ExceptionRecord, EXCEPTION_EXECUTE_HANDLER)
        {
            out.exceptionCode    = rec.ExceptionCode;
            out.exceptionAddress = rec.ExceptionAddress;
            // For EXCEPTION_ACCESS_VIOLATION (0xC0000005), ExceptionInformation has
            //   [0] = access type (0=read, 1=write, 8=DEP)
            //   [1] = faulting address
            if (rec.NumberParameters >= 2)
            {
                out.faultType    = (int)rec.ExceptionInformation[0];
                out.faultAddress = (void*)rec.ExceptionInformation[1];
            }
        }
        return out;
    }

    // POD reader for the AppearanceManager fields we want to snapshot.
    // SEH-guarded so a despawning player doesn't take us with it.
    struct AmSnapshot
    {
        bool          ok;
        int           assetRecordsCount;
        void*         slotPSlot;
        std::uint8_t  dirtyFlag;
        std::uint8_t  listUpdated;
        std::uint8_t  needsResync;
    };

    AmSnapshot ReadAmGuarded(TD::AppearanceManager* am, int slotIdx)
    {
        AmSnapshot s{};
        if (!am || slotIdx < 0 || slotIdx >= 27)
            return s;
        __try
        {
            s.assetRecordsCount = am->m_AssetRecords_Count;
            s.slotPSlot         = am->m_Clothes[slotIdx].m_pSlot;
            s.dirtyFlag         = am->m_DirtyFlag;
            s.listUpdated       = am->m_ListUpdated;
            s.needsResync       = am->m_NeedsResync;
            s.ok = true;
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {}
        return s;
    }

    // POD reader for the template Item's first qword and field_0x40 /
    // field_0xA8. These are the values the engine's BindItemToSlot uses
    // to compute slot id and dispatch on item type — knowing them up
    // front tells us what to expect.
    struct TemplateProbe
    {
        bool   ok;
        void*  firstQword;
        int    field40;
        int    fieldA8;
    };

    TemplateProbe ReadTemplateGuarded(void* itemPtr)
    {
        TemplateProbe t{};
        if (!itemPtr)
            return t;
        __try
        {
            t.firstQword = *(void**)itemPtr;
            t.field40    = *(int*)((std::uint8_t*)itemPtr + 0x40);
            t.fieldA8    = *(int*)((std::uint8_t*)itemPtr + 0xA8);
            t.ok = true;
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {}
        return t;
    }

    // Mirror of GetPlayerAppearance from SkinnedMeshManager.cpp — we keep
    // a local copy so this probe can compile standalone without exposing
    // SkinnedMeshManager's internals. Returns null on any failure
    // (singleton missing, agent array empty, EntityType != 1).
    TD::AppearanceManager* GetPlayerAppearance()
    {
        auto* rc = TD::RogueClient::Singleton();
        if (!rc) return nullptr;
        auto* client = rc->m_pClient;
        if (!client) return nullptr;
        auto* world = client->m_pWorld;
        if (!world || !world->m_AgentArray || world->m_AgentCount <= 0)
            return nullptr;

        for (int i = 0; i < world->m_AgentCount; ++i)
        {
            auto* a = world->m_AgentArray[i];
            if (!a) continue;
            int type = 0;
            __try { type = *(int*)((std::uint64_t)a + 0x3A4); }
            __except (EXCEPTION_EXECUTE_HANDLER) { continue; }
            if (type == 1)
                return a->m_pAppearance;
        }
        return nullptr;
    }

    // Walks Agent[0] to its PlayerInventory at offset +0x5B0. POD-clean —
    // takes the same shape as GetPlayerAppearance but stops one step
    // earlier in the chain.
    void* GetPlayerInventory()
    {
        auto* rc = TD::RogueClient::Singleton();
        if (!rc) return nullptr;
        auto* client = rc->m_pClient;
        if (!client) return nullptr;
        auto* world = client->m_pWorld;
        if (!world || !world->m_AgentArray || world->m_AgentCount <= 0)
            return nullptr;

        for (int i = 0; i < world->m_AgentCount; ++i)
        {
            auto* a = world->m_AgentArray[i];
            if (!a) continue;
            int type = 0;
            __try { type = *(int*)((std::uint64_t)a + 0x3A4); }
            __except (EXCEPTION_EXECUTE_HANDLER) { continue; }
            if (type == 1)
            {
                void* inv = nullptr;
                __try { inv = *(void**)((std::uint64_t)a + 0x5B0); }
                __except (EXCEPTION_EXECUTE_HANDLER) {}
                return inv;
            }
        }
        return nullptr;
    }

    // Heuristic for "this pointer is in the user-mode heap range".
    // Game heap allocations on this build land between 0x100'0000'0000
    // (= ~1 TB) and 0x7FF0'0000'0000 (= ~140 TB). Module addresses are
    // higher. Anything outside is almost certainly garbage from a stale
    // read.
    bool LooksLikeHeapPtr(void* p)
    {
        std::uint64_t v = (std::uint64_t)p;
        return v >= 0x10000000000ull && v < 0x7FF000000000ull;
    }

    // Scan PlayerInventory+0x80 .. +0x200 for non-null pointers to 96-byte
    // inventory holders. For each candidate, peek holder+0x50 — that's the
    // pointer to the EquipInstance wrapper the engine actually consumes.
    // Returns the first wrapper that passes the heap-range check.
    //
    // Why this offset range: live ReClass walk on 2026-05-12 showed dense
    // holder pointers between +0x80 and +0xB0, sparser ones up to +0xF0,
    // then drops off. +0x200 is a safe upper bound that won't read past
    // the PlayerInventory struct (~0x800 bytes).
    void* FindAnyWrapperGuarded(void* playerInventory, void** outHolder)
    {
        if (outHolder) *outHolder = nullptr;
        if (!playerInventory) return nullptr;
        __try
        {
            auto* base = (std::uint8_t*)playerInventory;
            for (int off = 0x80; off < 0x200; off += 8)
            {
                void* holder = *(void**)(base + off);
                if (!LooksLikeHeapPtr(holder)) continue;
                // holder+0x50 is the wrapper pointer per the engine's
                // sub_12E1B00 read pattern (`*(QWORD*)(*(QWORD*)v9 + 80LL)`).
                void* wrapper = *(void**)((std::uint8_t*)holder + 0x50);
                if (!LooksLikeHeapPtr(wrapper)) continue;
                if (outHolder) *outHolder = holder;
                return wrapper;
            }
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {}
        return nullptr;
    }

    // SEH-guarded memcpy. Returns true on success, false if the source
    // address fell outside committed memory partway through the copy.
    bool MemcpyGuarded(void* dst, const void* src, std::size_t n)
    {
        if (!dst || !src) return false;
        __try
        {
            std::memcpy(dst, src, n);
            return true;
        }
        __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
    }

    // POD reader for additional wrapper fields we want to log. Same shape
    // as TemplateProbe but for the wrapper / its inner item.
    struct WrapperProbe
    {
        bool   ok;
        void*  innerItem;          // wrapper.first_qword (= the Item* template)
        int    innerSlot;          // (*innerItem).field_0x40
        int    innerTypeTag;       // (*innerItem).field_0xA8
        int    wrapperField_0x130; // Owner_LookupAssetHandle reads
        int    wrapperField_0x13C; // Owner_LookupAssetHandle reads
        std::uint8_t wrapperField_0x154;  // path-mode flag (bit 0x80)
    };

    WrapperProbe ReadWrapperGuarded(void* wrapper)
    {
        WrapperProbe w{};
        if (!wrapper) return w;
        __try
        {
            auto* b = (std::uint8_t*)wrapper;
            w.innerItem          = *(void**)b;
            if (w.innerItem)
            {
                w.innerSlot    = *(int*)((std::uint8_t*)w.innerItem + 0x40);
                w.innerTypeTag = *(int*)((std::uint8_t*)w.innerItem + 0xA8);
            }
            w.wrapperField_0x130 = *(int*)(b + 0x130);
            w.wrapperField_0x13C = *(int*)(b + 0x13C);
            w.wrapperField_0x154 = *(std::uint8_t*)(b + 0x154);
            w.ok = true;
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {}
        return w;
    }
}

namespace EquipPipelineProbe
{
    bool RunEquipTest(const char* itemName, Result* out)
    {
        if (!out) return false;
        std::memset(out, 0, sizeof(*out));

        // 1. Resolve the template via the existing descriptor cache.
        out->itemPtr = ItemDescriptorCache::LookupByName(itemName);
        out->itemFound = (out->itemPtr != nullptr);
        if (!out->itemFound)
        {
            std::snprintf(out->summary, sizeof(out->summary),
                          "FAIL: '%s' not in InventoryConfig (cfg captured? scan first)",
                          itemName ? itemName : "(null)");
            return false;
        }

        // 2. Snapshot the template's diagnostic fields.
        TemplateProbe t = ReadTemplateGuarded(out->itemPtr);
        if (t.ok)
        {
            out->templateFirstQword  = t.firstQword;
            out->templateSlotField40 = t.field40;
            out->templateTypeFieldA8 = t.fieldA8;
            // The slot the template *claims* it belongs to (descriptor-side
            // value at item+0x40). Useful for diagnostics — note this is
            // NOT the value BindItemToSlot reads; that one is at
            // (item.first_qword)+0x40.
            out->targetSlot = (t.field40 >= 0 && t.field40 < 27) ? t.field40 : 0;
        }

        // 3. Resolve the player AppearanceManager.
        TD::AppearanceManager* am = GetPlayerAppearance();
        out->amPtr = am;
        out->amFound = (am != nullptr);
        if (!out->amFound)
        {
            std::snprintf(out->summary, sizeof(out->summary),
                          "FAIL: no player AppearanceManager (not in-world?)");
            return false;
        }

        // 4. Pre-call snapshot.
        AmSnapshot pre = ReadAmGuarded(am, out->targetSlot);
        if (pre.ok)
        {
            out->preAssetRecordsCount = pre.assetRecordsCount;
            out->preSlotPSlot         = pre.slotPSlot;
            out->preDirtyFlag         = pre.dirtyFlag;
            out->preListUpdated       = pre.listUpdated;
            out->preNeedsResync       = pre.needsResync;
        }

        // 5. Build a single-entry list on the stack.
        // The engine's vector_void_assign reads count at +8 and capacity
        // at +12; passing cap == count with no inline bit lets it just
        // memcpy our pointer out of `entryStorage`.
        void*           entryStorage = out->itemPtr;
        EngineItemList  list{};
        list.ptr       = &entryStorage;
        list.count     = 1;
        list.cap_flags = 1;            // capacity = 1, high bit clear

        // 6. The actual call. Wrapped in SEH so an AV inside the engine
        // becomes a structured result instead of a process-wide crash.
        out->callAttempted = true;
        CallOutcome c = CallSetEquippedItemsGuarded(am, &list);
        out->callReturned     = c.returned;
        out->exceptionCode    = c.exceptionCode;
        out->exceptionAddress = c.exceptionAddress;
        out->faultAddress     = c.faultAddress;
        out->faultType        = c.faultType;

        // 7. Post-call snapshot (best effort — engine state may be
        // corrupted if the call faulted).
        AmSnapshot post = ReadAmGuarded(am, out->targetSlot);
        if (post.ok)
        {
            out->postAssetRecordsCount = post.assetRecordsCount;
            out->postSlotPSlot         = post.slotPSlot;
            out->postDirtyFlag         = post.dirtyFlag;
            out->postListUpdated       = post.listUpdated;
            out->postNeedsResync       = post.needsResync;
        }

        // 8. Build the human-readable summary.
        if (c.returned)
        {
            std::snprintf(out->summary, sizeof(out->summary),
                          "OK call returned. item=%p first_qword=%p "
                          "tmpl[+0x40]=%d tmpl[+0xA8]=%d slot=%d\n"
                          "  AssetRecords: %d -> %d\n"
                          "  m_pSlot[%d]:  %p -> %p\n"
                          "  flags: dirty %u->%u  listUpd %u->%u  resync %u->%u",
                          out->itemPtr, out->templateFirstQword,
                          out->templateSlotField40, out->templateTypeFieldA8,
                          out->targetSlot,
                          out->preAssetRecordsCount, out->postAssetRecordsCount,
                          out->targetSlot, out->preSlotPSlot, out->postSlotPSlot,
                          (unsigned)out->preDirtyFlag,   (unsigned)out->postDirtyFlag,
                          (unsigned)out->preListUpdated, (unsigned)out->postListUpdated,
                          (unsigned)out->preNeedsResync, (unsigned)out->postNeedsResync);
        }
        else
        {
            const char* faultKind = "?";
            switch (out->faultType)
            {
                case 0: faultKind = "read";  break;
                case 1: faultKind = "write"; break;
                case 8: faultKind = "DEP";   break;
                default: break;
            }
            std::snprintf(out->summary, sizeof(out->summary),
                          "EXC code=0x%08lX  ip=%p  fault(%s)=%p\n"
                          "  item=%p first_qword=%p tmpl[+0x40]=%d tmpl[+0xA8]=%d slot=%d\n"
                          "  AssetRecords (best effort): %d -> %d",
                          out->exceptionCode, out->exceptionAddress,
                          faultKind, out->faultAddress,
                          out->itemPtr, out->templateFirstQword,
                          out->templateSlotField40, out->templateTypeFieldA8,
                          out->targetSlot,
                          out->preAssetRecordsCount, out->postAssetRecordsCount);
        }
        return true;
    }

    // ── Pattern A+ injection tracking ─────────────────────────────────
    // Per slot (0..26), remember the wrapper pointer we last injected and
    // the asset path it carries. MaintainInjections compares the current
    // m_AssetRecords against this state every frame and re-applies our
    // full injection set when the engine has dropped any of them (which
    // happens on any sub_162DB80 call from in-game UI, save load, etc.).
    struct TrackedInjection
    {
        bool   active;
        void*  wrapper;            // address of our per-slot clone buffer
        char   path[260];          // resolved visual-gear path the engine
                                   // would have inserted for the bucket;
                                   // recorded for diagnostics + possible
                                   // future per-slot bucket cleanup.
    };
    static TrackedInjection s_inj[27]{};

    // Per-slot static clone buffers. Each Pattern A+ injection writes its
    // retargeted wrapper into a unique 1 KB buffer so the engine can
    // reference it indefinitely without other injections corrupting it.
    //
    // ★ Previously a single shared s_clone buffer caused: equipping slot B
    //   would overwrite the bytes slot A's tracked wrapper pointed at, so
    //   m_AssetRecords[slotA] silently aliased slot B's item template —
    //   only one injection ever rendered correctly at a time.
    static std::uint8_t s_clones[27][1024];

    // Original outfit snapshot — taken once, before our first mutation.
    // We copy the live m_AssetRecords pointer array (the wrapper pointers
    // themselves remain valid because they live in PlayerInventory-owned
    // memory, not in the vector) so a later sub_162DB80 call can replay
    // them and restore the player's outfit exactly.
    struct OutfitSnapshot
    {
        bool  captured;
        int   count;
        void* wrappers[64];          // up to 64 wrappers; live outfits ~13
    };
    static OutfitSnapshot s_originalOutfit{};

    bool TakeOriginalSnapshotGuarded(TD::AppearanceManager* am)
    {
        if (s_originalOutfit.captured) return true;
        if (!am) return false;
        __try
        {
            void** arr = am->m_AssetRecords_Ptr;
            int    n   = am->m_AssetRecords_Count;
            if (!arr || n <= 0) return false;
            int cap = (int)(sizeof(s_originalOutfit.wrappers) /
                            sizeof(s_originalOutfit.wrappers[0]));
            if (n > cap) n = cap;
            for (int i = 0; i < n; ++i)
                s_originalOutfit.wrappers[i] = arr[i];
            s_originalOutfit.count    = n;
            s_originalOutfit.captured = true;
            return true;
        }
        __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
    }

    // Engine's AttachHashmap remove (sub_1650620). Cleanly drops the
    // bucket whose model-path key equals `path`. Verified equivalent of
    // what Character_ApplyClothingId calls internally. SEH-guarded.
    bool CallHashmapRemoveGuarded(TD::AppearanceManager* am, const char* path)
    {
        if (!am || !path || !*path) return false;
        typedef __int64 (__fastcall *PFN)(void* hashmap, const char* path);
        PFN fn = (PFN)(g_pBase + 0x1650620);
        __try
        {
            fn((void*)((std::uint64_t)am + 0x18), path);
            return true;
        }
        __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
    }

    // Scan m_AssetRecords for a specific wrapper pointer. Returns true if
    // found; false if the engine has replaced it (e.g. via the player's
    // UI equip path running its own sub_162DB80).
    bool AssetRecordsContainsGuarded(TD::AppearanceManager* am, void* wrapper)
    {
        if (!am || !wrapper) return false;
        __try
        {
            void** arr = am->m_AssetRecords_Ptr;
            int    n   = am->m_AssetRecords_Count;
            if (!arr || n <= 0) return false;
            for (int i = 0; i < n; ++i)
                if (arr[i] == wrapper) return true;
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {}
        return false;
    }

    // Reads the resolved visual-gear path the engine puts in slot.m_Path
    // (heap-allocated SnowdropString at slot+0x08). After Pattern A+
    // succeeds, this is the path string the engine added to
    // m_AttachHashmap as the bucket key — we record it so we can later
    // identify and remove that exact bucket.
    void ReadSlotPathGuarded(TD::AppearanceManager* am, int slotIdx, char* out, std::size_t outSize)
    {
        if (!out || outSize == 0) return;
        out[0] = '\0';
        if (!am || slotIdx < 0 || slotIdx >= 27) return;
        __try
        {
            const BYTE* sstr = am->m_Clothes[slotIdx].m_Path.bytes;
            const char* path = nullptr;
            if (sstr[0x0F] == 0)
                path = (const char*)sstr;                  // inline mode
            else
                path = *(const char* const*)sstr;          // heap mode
            if (!path) return;
            std::size_t i = 0;
            while (i + 1 < outSize && path[i]) { out[i] = path[i]; ++i; }
            out[i] = '\0';
        }
        __except (EXCEPTION_EXECUTE_HANDLER) { out[0] = '\0'; }
    }

    // SEH-guarded byte writes to the three change-tracking flags on the
    // AppearanceManager. Used by Pattern A+ to test whether suppressing
    // these flags suppresses the engine's revert tick. Returns true if the
    // writes completed without exception.
    bool ClearAmFlagsGuarded(TD::AppearanceManager* am)
    {
        if (!am) return false;
        __try
        {
            am->m_DirtyFlag   = 0;   // +0x4E0
            am->m_ListUpdated = 0;   // +0x4E1
            am->m_NeedsResync = 0;   // +0x4E2
            return true;
        }
        __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
    }

    // ── Pattern A: clone an existing wrapper, swap inner Item* ─────────
    bool RunEquipTestPatternA(const char* itemName, Result* out)
    {
        if (!out) return false;
        std::memset(out, 0, sizeof(*out));

        // 1. Resolve the target template.
        out->itemPtr = ItemDescriptorCache::LookupByName(itemName);
        out->itemFound = (out->itemPtr != nullptr);
        if (!out->itemFound)
        {
            std::snprintf(out->summary, sizeof(out->summary),
                          "FAIL: '%s' not in InventoryConfig", itemName ? itemName : "(null)");
            return false;
        }

        // 2. Snapshot the target template's slot id and type tag —
        //    informational only; the slot id we ACT on comes from the
        //    template the engine reads, which is `template+0x40`.
        TemplateProbe t = ReadTemplateGuarded(out->itemPtr);
        if (t.ok)
        {
            out->templateFirstQword  = t.firstQword;
            out->templateSlotField40 = t.field40;
            out->templateTypeFieldA8 = t.fieldA8;
            out->targetSlot = (t.field40 >= 0 && t.field40 < 27) ? t.field40 : 0;
        }

        // 3. Resolve player AppearanceManager.
        TD::AppearanceManager* am = GetPlayerAppearance();
        out->amPtr = am;
        out->amFound = (am != nullptr);
        if (!out->amFound)
        {
            std::snprintf(out->summary, sizeof(out->summary),
                          "FAIL: no player AppearanceManager (not in-world?)");
            return false;
        }

        // 4. Pre-call snapshot.
        AmSnapshot pre = ReadAmGuarded(am, out->targetSlot);
        if (pre.ok)
        {
            out->preAssetRecordsCount = pre.assetRecordsCount;
            out->preSlotPSlot         = pre.slotPSlot;
            out->preDirtyFlag         = pre.dirtyFlag;
            out->preListUpdated       = pre.listUpdated;
            out->preNeedsResync       = pre.needsResync;
        }

        // 5. Find an existing wrapper in PlayerInventory we can clone.
        void* inv     = GetPlayerInventory();
        void* holder  = nullptr;
        void* wrapper = FindAnyWrapperGuarded(inv, &holder);
        if (!wrapper)
        {
            std::snprintf(out->summary, sizeof(out->summary),
                          "FAIL: no clonable wrapper found in PlayerInventory %p", inv);
            return false;
        }

        WrapperProbe w = ReadWrapperGuarded(wrapper);

        // 6. Clone the wrapper into THIS slot's dedicated 1 KB buffer.
        //    Each slot gets its own s_clones[slot] entry so that equipping
        //    slot B doesn't smash slot A's wrapper memory — every active
        //    injection must keep referencing valid retargeted bytes for
        //    the engine's next pass.
        std::uint8_t* s_clone = s_clones[out->targetSlot];
        if (!MemcpyGuarded(s_clone, wrapper, sizeof(s_clones[0])))
        {
            std::snprintf(out->summary, sizeof(out->summary),
                          "FAIL: memcpy of wrapper at %p crashed mid-copy", wrapper);
            return false;
        }

        // 7. Retarget: overwrite +0x00 with our chosen template Item*.
        //    The engine's (*(QWORD*)entry)+0x40 read now lands on
        //    template+0x40 = real slot id (verified earlier in step 2).
        *(void**)s_clone = out->itemPtr;

        // 8. Build the items list. sub_162DB80 is a full-list replace —
        //    any active wrapper NOT in the list gets evicted from
        //    m_AssetRecords, which makes that slot revert to whatever
        //    PlayerInventory holds for it. Because we removed the slot's
        //    vanilla AttachHashmap bucket on the original equip, the
        //    revert renders as INVISIBLE rather than a fallback model.
        //
        //    Fix: include every still-active tracked injection (except
        //    this slot, which we're replacing) so all our prior equips
        //    survive the call. The list pointers are stored in a static
        //    array so the engine can keep reading them after we return.
        static void*  s_entries[27];
        int           nEntries = 0;
        for (int slot = 0; slot < 27; ++slot)
        {
            if (slot == out->targetSlot)          continue;
            if (!s_inj[slot].active)              continue;
            if (!s_inj[slot].wrapper)             continue;
            s_entries[nEntries++] = s_inj[slot].wrapper;
        }
        s_entries[nEntries++] = s_clone;

        EngineItemList  list{};
        list.ptr       = s_entries;
        list.count     = nEntries;
        list.cap_flags = (std::uint32_t)nEntries;

        // 8a. Snapshot the slot's current path before the call. If the slot
        //     was already occupied (e.g. the player has a vanilla backpack
        //     in slot 0), the engine's AttachHashmap has a bucket for that
        //     vanilla path. After our sub_162DB80 call updates the slot
        //     to point at OUR item's path, the vanilla bucket is orphaned
        //     — the engine's own Character_ApplyClothingId pass doesn't
        //     catch it because that function reads bucket field +0x3C
        //     while ModelLoadTrigger only writes the slot id to +0x2C
        //     (different offsets — the engine never sees vanilla buckets
        //     as belonging to the slot). We have to remove the vanilla
        //     bucket ourselves once we know the new path landed.
        char preSlotPath[260] = {};
        ReadSlotPathGuarded(am, out->targetSlot, preSlotPath, sizeof(preSlotPath));

        // 9. The call.
        out->callAttempted = true;
        CallOutcome c = CallSetEquippedItemsGuarded(am, &list);
        out->callReturned     = c.returned;
        out->exceptionCode    = c.exceptionCode;
        out->exceptionAddress = c.exceptionAddress;
        out->faultAddress     = c.faultAddress;
        out->faultType        = c.faultType;

        // 9a. Cleanup + record on success. Two orphans to potentially
        //     remove from m_AttachHashmap:
        //       (1) the vanilla bucket whose path was in m_Clothes[slot]
        //           BEFORE our call (captured into preSlotPath above)
        //       (2) the previous Pattern A+ injection for this slot
        //           (recorded by an earlier call into s_inj[slot])
        //     Both are removed via the engine's hashmap_remove, keyed by
        //     the path string. Skipping the remove when the old path
        //     equals the new path avoids removing the bucket we just
        //     inserted (e.g. re-equipping the same item is a no-op).
        if (c.returned && out->targetSlot >= 0 && out->targetSlot < 27)
        {
            char postSlotPath[260] = {};
            ReadSlotPathGuarded(am, out->targetSlot, postSlotPath, sizeof(postSlotPath));

            // (1) Vanilla / pre-call bucket — fires the first time we
            //     equip over a filled slot.
            if (preSlotPath[0] && _stricmp(preSlotPath, postSlotPath) != 0)
                CallHashmapRemoveGuarded(am, preSlotPath);

            // (2) Previous Pattern A+ bucket — fires on the 2nd+ click
            //     for the same slot.
            TrackedInjection& prev = s_inj[out->targetSlot];
            if (prev.active && prev.path[0] &&
                _stricmp(prev.path, postSlotPath) != 0 &&
                _stricmp(prev.path, preSlotPath)  != 0)   // already removed in (1)
            {
                CallHashmapRemoveGuarded(am, prev.path);
            }

            // Record the new injection state for the per-frame maintainer
            // and for the next Pattern A+ call on this slot.
            prev.active  = true;
            prev.wrapper = s_clone;
            std::snprintf(prev.path, sizeof(prev.path), "%s", postSlotPath);
        }

        // 10. Post-snapshot.
        AmSnapshot post = ReadAmGuarded(am, out->targetSlot);
        if (post.ok)
        {
            out->postAssetRecordsCount = post.assetRecordsCount;
            out->postSlotPSlot         = post.slotPSlot;
            out->postDirtyFlag         = post.dirtyFlag;
            out->postListUpdated       = post.listUpdated;
            out->postNeedsResync       = post.needsResync;
        }

        // 11. Summary. Pattern A reports extra fields so we can see
        //     which donor wrapper was used and verify the inner item
        //     swap worked.
        if (c.returned)
        {
            std::snprintf(out->summary, sizeof(out->summary),
                          "OK PatternA call returned.\n"
                          "  target item=%p first_qword=%p slot=%d type=%d\n"
                          "  donor wrapper=%p (holder=%p, donor inner=%p slot=%d type=%d "
                          "f130=%d f13C=%d f154=0x%02X)\n"
                          "  AssetRecords: %d -> %d\n"
                          "  m_pSlot[%d]:  %p -> %p\n"
                          "  flags: dirty %u->%u  listUpd %u->%u  resync %u->%u",
                          out->itemPtr, out->templateFirstQword,
                          out->templateSlotField40, out->templateTypeFieldA8,
                          wrapper, holder, w.innerItem, w.innerSlot, w.innerTypeTag,
                          w.wrapperField_0x130, w.wrapperField_0x13C,
                          (unsigned)w.wrapperField_0x154,
                          out->preAssetRecordsCount, out->postAssetRecordsCount,
                          out->targetSlot, out->preSlotPSlot, out->postSlotPSlot,
                          (unsigned)out->preDirtyFlag,   (unsigned)out->postDirtyFlag,
                          (unsigned)out->preListUpdated, (unsigned)out->postListUpdated,
                          (unsigned)out->preNeedsResync, (unsigned)out->postNeedsResync);
        }
        else
        {
            const char* faultKind = "?";
            switch (out->faultType)
            {
                case 0: faultKind = "read";  break;
                case 1: faultKind = "write"; break;
                case 8: faultKind = "DEP";   break;
                default: break;
            }
            std::snprintf(out->summary, sizeof(out->summary),
                          "PatternA EXC code=0x%08lX  ip=%p  fault(%s)=%p\n"
                          "  target item=%p slot=%d  donor wrapper=%p donor inner=%p slot=%d\n"
                          "  donor f130=%d f13C=%d f154=0x%02X\n"
                          "  AssetRecords (best effort): %d -> %d",
                          out->exceptionCode, out->exceptionAddress,
                          faultKind, out->faultAddress,
                          out->itemPtr, out->targetSlot,
                          wrapper, w.innerItem, w.innerSlot,
                          w.wrapperField_0x130, w.wrapperField_0x13C,
                          (unsigned)w.wrapperField_0x154,
                          out->preAssetRecordsCount, out->postAssetRecordsCount);
        }
        return true;
    }

    // ── Pattern A+ — same as Pattern A, optionally clear AM flags right
    //                after the call to suppress the engine's revert tick.
    bool RunEquipTestPatternAPlus(const char* itemName,
                                  bool clearFlagsAfter,
                                  Result* out)
    {
        // Delegate to Pattern A for the heavy lifting. After it returns,
        // if it succeeded AND the caller asked for flag-clearing, write 0
        // to am+0x4E0/+0x4E1/+0x4E2. This is done in a separate SEH frame
        // so a fault during the writes doesn't damage Pattern A's result.
        bool ok = RunEquipTestPatternA(itemName, out);
        if (!ok || !out->callReturned)
            return ok;     // Pattern A failed or AV'd; nothing to clear.

        if (clearFlagsAfter && out->amPtr)
        {
            bool cleared = ClearAmFlagsGuarded((TD::AppearanceManager*)out->amPtr);

            // Append the action and post-clear flag state to the summary.
            // We snapshot the flags after clearing so the result shows
            // them as 0 rather than the engine's post-call values.
            AmSnapshot afterClear = ReadAmGuarded((TD::AppearanceManager*)out->amPtr,
                                                  out->targetSlot);

            char extra[256];
            std::snprintf(extra, sizeof(extra),
                          "\n  [A+] cleared flags: %s  post-clear: dirty=%u listUpd=%u resync=%u",
                          cleared ? "yes" : "FAILED (AV)",
                          (unsigned)(afterClear.ok ? afterClear.dirtyFlag   : 0xFFu),
                          (unsigned)(afterClear.ok ? afterClear.listUpdated : 0xFFu),
                          (unsigned)(afterClear.ok ? afterClear.needsResync : 0xFFu));

            // Append to the existing summary (truncated if it would overflow).
            // Using snprintf instead of strncat — MSVC deprecates strncat as
            // unsafe; snprintf into the tail of the buffer gives the same
            // result with the proper safe-CRT interface.
            std::size_t curLen = std::strlen(out->summary);
            std::size_t avail  = sizeof(out->summary) - curLen;
            if (avail > 1)
                std::snprintf(out->summary + curLen, avail, "%s", extra);
        }
        else
        {
            // Control mode — annotate the summary so the user can tell
            // which variant ran.
            std::size_t curLen = std::strlen(out->summary);
            std::size_t avail  = sizeof(out->summary) - curLen;
            if (avail > 1)
                std::snprintf(out->summary + curLen, avail,
                              "%s", "\n  [A+ control] flags left as engine set them");
        }
        return true;
    }

    // ── Per-frame maintenance ──────────────────────────────────────────
    //
    // Detects when the engine has dropped any of our wrappers from
    // m_AssetRecords — happens whenever a sub_162DB80 call originates
    // outside this module (in-game equip UI, save load, mission swaps,
    // anything that issues SetEquippedItems). On detection, re-applies
    // every active injection in a single sub_162DB80 call so the engine
    // rebuilds m_AssetRecords with our overrides included.
    //
    // The alternative — remove our orphan AttachHashmap bucket — was
    // wrong: the engine had ALREADY rewritten m_Clothes[slot].m_Path back
    // to vanilla in the same pass, so the orphan bucket wasn't actually
    // an orphan, it was the only bucket keeping the slot rendering. We
    // were nuking our own model.
    // No-op per the user's design: auto-re-injection caused visible
    // flicker when the engine fired its own SetEquippedItems passes
    // (weapon swaps, animation state changes, etc.). The user instead
    // drives re-application manually via the "Apply All" UI button,
    // which calls ReapplyAllInjections() exactly once.
    //
    // Kept as an exported entry point so SkinnedMeshManager::Update()
    // doesn't have to change.
    void MaintainInjections(void* /*appearanceManager*/) { }

    // Manually re-apply every active Pattern A+ injection in a single
    // sub_162DB80 call. Use this when the engine has clobbered our
    // wrappers (slots gone invisible) and you want everything back.
    // Returns the number of injections re-applied; 0 if nothing was
    // active or the call faulted.
    int ReapplyAllInjections()
    {
        auto* am = GetPlayerAppearance();
        if (!am) return 0;

        static void* s_entries[27];
        int nEntries = 0;
        for (int slot = 0; slot < 27; ++slot)
        {
            if (!s_inj[slot].active || !s_inj[slot].wrapper) continue;
            s_entries[nEntries++] = s_inj[slot].wrapper;
        }
        if (nEntries == 0) return 0;

        EngineItemList list{};
        list.ptr       = s_entries;
        list.count     = nEntries;
        list.cap_flags = (std::uint32_t)nEntries;

        CallOutcome c = CallSetEquippedItemsGuarded(am, &list);
        if (!c.returned) return 0;
        ClearAmFlagsGuarded(am);
        return nEntries;
    }

    // Stage one slot's clone buffer + tracked-injection state without
    // calling the engine. Caller is responsible for firing one combined
    // sub_162DB80 call after all desired slots have been staged. Returns
    // false if either the donor wrapper or the memcpy failed.
    static bool StageOneGuarded(int slotIdx, void* itemTemplate, void* playerInv)
    {
        if (slotIdx < 0 || slotIdx >= 27 || !itemTemplate) return false;

        void* holder = nullptr;
        void* donor  = FindAnyWrapperGuarded(playerInv, &holder);
        if (!donor) return false;

        if (!MemcpyGuarded(s_clones[slotIdx], donor, sizeof(s_clones[0])))
            return false;

        // Retarget +0x00 to our chosen template. Engine's slot id read
        // (*(QWORD*)entry)+0x40 now lands on the template's real slot.
        *(void**)s_clones[slotIdx] = itemTemplate;

        s_inj[slotIdx].active  = true;
        s_inj[slotIdx].wrapper = s_clones[slotIdx];
        // Path is filled in after the engine call rewrites m_Clothes[slot].m_Path.
        return true;
    }

    // Apply many slots in a single engine call. For each (slot, name) pair:
    //   - Look up the template via ItemDescriptorCache::LookupByName
    //   - Clone a donor wrapper from PlayerInventory into s_clones[slot]
    //   - Retarget +0x00 to the template and mark s_inj[slot] active
    // Then fire ONE sub_162DB80 call carrying every active tracked wrapper
    // (the batch entries plus any earlier injections still active). One
    // animation reset for the entire batch instead of N for N entries —
    // this is the path the "Apply All" UI button uses.
    //
    // Returns the number of (slot, name) entries that successfully resolved
    // + staged. Engine call success is implicit in the return: > 0 means
    // the call returned; 0 means either the call AV'd or nothing resolved.
    int RunEquipBatch(const int* slots, const char* const* names, int n,
                      char* err, std::size_t errSize)
    {
        if (err && errSize) err[0] = '\0';

        auto* am = GetPlayerAppearance();
        if (!am)
        {
            if (err) std::snprintf(err, errSize, "no player AppearanceManager");
            return 0;
        }

        void* inv = GetPlayerInventory();
        if (!inv)
        {
            if (err) std::snprintf(err, errSize, "no PlayerInventory");
            return 0;
        }

        // Snapshot the original outfit on the first call only. After
        // this point we're free to mutate m_AssetRecords; the snapshot
        // holds pointers to wrappers that live in PlayerInventory-owned
        // memory and stay valid until the character is unloaded.
        TakeOriginalSnapshotGuarded(am);

        int staged = 0;
        for (int i = 0; i < n; ++i)
        {
            void* tmpl = ItemDescriptorCache::LookupByName(names[i]);
            if (!tmpl) continue;
            if (StageOneGuarded(slots[i], tmpl, inv)) ++staged;
        }

        // Build list of every active wrapper (this batch + previously
        // active injections that the user didn't include in this call).
        static void* s_entries[27];
        int nEntries = 0;
        for (int slot = 0; slot < 27; ++slot)
        {
            if (!s_inj[slot].active || !s_inj[slot].wrapper) continue;
            s_entries[nEntries++] = s_inj[slot].wrapper;
        }
        if (nEntries == 0)
        {
            if (err) std::snprintf(err, errSize, "nothing resolved/staged");
            return 0;
        }

        EngineItemList list{};
        list.ptr       = s_entries;
        list.count     = nEntries;
        list.cap_flags = (std::uint32_t)nEntries;

        CallOutcome c = CallSetEquippedItemsGuarded(am, &list);
        if (!c.returned)
        {
            if (err)
                std::snprintf(err, errSize,
                              "engine call AV'd (code 0x%08lX ip=%p)",
                              c.exceptionCode, c.exceptionAddress);
            return 0;
        }
        ClearAmFlagsGuarded(am);

        // After the call, record each staged slot's resolved visual-gear
        // path so future Pattern A+ calls can identify our buckets.
        for (int i = 0; i < n; ++i)
        {
            int slot = slots[i];
            if (slot < 0 || slot >= 27) continue;
            if (!s_inj[slot].active)    continue;
            ReadSlotPathGuarded(am, slot, s_inj[slot].path, sizeof(s_inj[slot].path));
        }

        return staged;
    }

    // Replay the captured original outfit through sub_162DB80, then
    // forget all our tracked injections. The snapshot's wrapper pointers
    // still reference live PlayerInventory holders, so the engine
    // rebuilds m_AssetRecords + m_Clothes[].m_Path + AttachHashmap from
    // those instead of from our clone buffers. Result: the character
    // visually matches their pre-mod state.
    //
    // Returns the wrapper count replayed, or 0 if no snapshot exists /
    // the call faulted / the player AM is unreachable.
    int RestoreOriginalOutfit()
    {
        if (!s_originalOutfit.captured || s_originalOutfit.count <= 0)
            return 0;
        auto* am = GetPlayerAppearance();
        if (!am) return 0;

        EngineItemList list{};
        list.ptr       = s_originalOutfit.wrappers;
        list.count     = s_originalOutfit.count;
        list.cap_flags = (std::uint32_t)s_originalOutfit.count;

        CallOutcome c = CallSetEquippedItemsGuarded(am, &list);
        if (!c.returned) return 0;
        ClearAmFlagsGuarded(am);

        // Drop all our tracked injections — the engine just overwrote
        // every slot we'd touched, so our clone buffers are no longer
        // referenced anywhere.
        for (int slot = 0; slot < 27; ++slot)
        {
            s_inj[slot].active  = false;
            s_inj[slot].wrapper = nullptr;
            s_inj[slot].path[0] = '\0';
        }
        return s_originalOutfit.count;
    }

    bool HasOriginalSnapshot() { return s_originalOutfit.captured; }

    // Force a fresh snapshot of the current m_AssetRecords. Used by the
    // "Snapshot" UI button so the user can pin a known-good outfit
    // (e.g. after the engine has rebuilt records for a new character).
    // Returns the wrapper count captured; 0 if the AM is unreachable or
    // m_AssetRecords is empty.
    int TakeOriginalSnapshotNow()
    {
        s_originalOutfit.captured = false;
        s_originalOutfit.count    = 0;
        auto* am = GetPlayerAppearance();
        if (!am) return 0;
        if (!TakeOriginalSnapshotGuarded(am)) return 0;
        return s_originalOutfit.count;
    }

    // Force-clean every tracked injection right now. Used by the manual
    // "Cleanup" UI button. Walks all slots, removes the bucket via the
    // engine's hashmap_remove, and clears tracking unconditionally.
    void ClearAllInjections(void* appearanceManager)
    {
        auto* am = (TD::AppearanceManager*)appearanceManager;
        for (int slot = 0; slot < 27; ++slot)
        {
            TrackedInjection& inj = s_inj[slot];
            if (!inj.active) continue;
            if (am && inj.path[0])
                CallHashmapRemoveGuarded(am, inj.path);
            inj.active     = false;
            inj.wrapper    = nullptr;
            inj.path[0]    = '\0';
        }
    }
}
