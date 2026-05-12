#include "ItemDescriptorCache.h"
#include "Snowdrop.h"
#include <Windows.h>
#include <cstring>
#include <cstdint>
#include <atomic>

// TD::ItemDescriptor and TD::InventoryConfig are now fully defined in
// Snowdrop.h with real field layouts. We previously had empty stub structs
// here while the layouts were unknown; remove them now that Snowdrop.h
// owns the canonical definitions.

// ─── Strategy ────────────────────────────────────────────────────────────────
// Direct singleton-chain walk. We can't hook sub_F07C40 (ACG blocks
// .text patching on this process), but every caller of sub_F07C40 obtains
// the InventoryConfig pointer through the same fixed offset chain rooted
// at a single global pointer in the data section:
//
//   module + 0x4688B28   global pointer (game's item-system singleton)
//     -> +0x120          sub-system holder
//     -> +0x28           inner holder
//     -> +0x138          cfg-owner wrapper
//     -> +0xD8           InventoryConfig*
//
// Verified by decompiling sub_125E3B0 and sub_11D5B20 (both walk this
// exact chain to feed sub_CEA470, which deref-s owner+0xD8 to feed
// sub_F07C40). Five reads, SEH-guarded, no scanning.
//
// We still validate by probing sub_F07C40 with a known-good item name —
// cheap insurance against the offsets drifting on a future game update.
//
// The legacy heap-wide signature scan (LooksLikeInventoryConfig +
// ScanForCfg) remains in the file below but is dead code; it's kept as
// fallback inspiration if the chain ever stops resolving.
// ─────────────────────────────────────────────────────────────────────────────

namespace
{
    // RVA of the engine's name-lookup function (sub_F07C40).
    constexpr std::uintptr_t kLookupByNameRVA = 0xF07C40;

    using PFN_LookupByName = __int64(__fastcall*)(__int64 cfg, char* name);

    PFN_LookupByName GetEngineLookup()
    {
        return reinterpret_cast<PFN_LookupByName>(g_pBase + kLookupByNameRVA);
    }

    // ── shared state ────────────────────────────────────────────────────────
    std::atomic<TD::InventoryConfig*> g_pInventoryConfig{ nullptr };
    bool         g_initAttempted    = false;
    bool         g_scanRan          = false;
    std::uint64_t g_lastScanRegionCt = 0;
    std::uint64_t g_lastScanByteCt   = 0;
    std::uint64_t g_lastScanCandCt   = 0;  // candidates considered
    const char*  g_lastScanResult   = "not scanned yet";

    // ── helpers (POD-only, SEH-isolated) ────────────────────────────────────

    // Reads N bytes from an address with SEH around it. Returns false if the
    // read AV'd. POD-only because callers may hold C++ objects (C2712).
    bool TryRead(const void* addr, void* out, std::size_t bytes)
    {
        __try
        {
            std::memcpy(out, addr, bytes);
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            return false;
        }
        return true;
    }

    bool IsReadable(const void* addr, std::size_t bytes)
    {
        if (!addr) return false;
        MEMORY_BASIC_INFORMATION mbi{};
        if (VirtualQuery(addr, &mbi, sizeof(mbi)) == 0) return false;
        if (mbi.State != MEM_COMMIT) return false;
        // Reject guard / no-access pages
        if (mbi.Protect & (PAGE_GUARD | PAGE_NOACCESS)) return false;
        // Bounds check — make sure the whole read fits in the region
        std::uintptr_t base = reinterpret_cast<std::uintptr_t>(mbi.BaseAddress);
        std::uintptr_t end  = base + mbi.RegionSize;
        std::uintptr_t reqEnd = reinterpret_cast<std::uintptr_t>(addr) + bytes;
        return reqEnd <= end;
    }

    // Game module address range — anything that looks like a vtable
    // pointer should land in here. .text / .rdata are loaded contiguously.
    // 256MB upper bound covers any plausible module size.
    std::uintptr_t g_modLow  = 0;
    std::uintptr_t g_modHigh = 0;

    // Cache valid heap allocation bases we've already seen. Repeated
    // VirtualQuery calls dominate the scan cost otherwise.
    bool IsHeapPointer(const void* p)
    {
        if (!p) return false;
        // Filter obvious junk before touching VirtualQuery
        std::uintptr_t v = reinterpret_cast<std::uintptr_t>(p);
        if (v < 0x10000) return false;
        if (v & 0x7) return false;   // not 8-aligned → unlikely a heap header
        if (v >= g_modLow && v < g_modHigh) return false; // in module image, not heap
        return true;
    }

    bool IsModulePointer(const void* p)
    {
        std::uintptr_t v = reinterpret_cast<std::uintptr_t>(p);
        return g_modLow != 0 && v >= g_modLow && v < g_modHigh;
    }

    // FAST pre-filter — just reads from the candidate (all within a known-
    // committed region) and checks shape. SEH-guards the reads in case our
    // region info is stale.
    //
    // Critical optimization: vtable at +0 must point into the game module.
    // Rejects ~99.99% of garbage in nanoseconds before we look further.
    bool LooksLikeInventoryConfig(const void* cand)
    {
        const std::uint8_t* p = reinterpret_cast<const std::uint8_t*>(cand);

        // +0x00: vtable pointer — must be in game module .text/.rdata
        const void* vtbl = nullptr;
        if (!TryRead(p, &vtbl, sizeof(vtbl))) return false;
        if (!IsModulePointer(vtbl)) return false;

        // +0x20: data pointer to vector entries — must be heap
        const void* vecData = nullptr;
        if (!TryRead(p + 0x20, &vecData, sizeof(vecData))) return false;
        if (!IsHeapPointer(vecData)) return false;

        // +0x28 / +0x2C: count / capacity in a sensible range
        std::uint32_t count = 0, cap = 0;
        if (!TryRead(p + 0x28, &count, 4)) return false;
        if (!TryRead(p + 0x2C, &cap, 4)) return false;
        if (count < 500 || count > 200000) return false;
        if (cap < count || cap > 200000) return false;

        // +0x170 and +0x1A0: hashmap entries pointers — must be heap
        const void* hm1 = nullptr;
        if (!TryRead(p + 0x170, &hm1, sizeof(hm1))) return false;
        if (!IsHeapPointer(hm1)) return false;

        const void* hm2 = nullptr;
        if (!TryRead(p + 0x1A0, &hm2, sizeof(hm2))) return false;
        if (!IsHeapPointer(hm2)) return false;

        return true;
    }

    // Strong validation: actually call sub_F07C40 against the candidate
    // with a name we expect to exist in any Division 1 build. If it returns
    // a descriptor whose +64 (myEquipmentSlot) is in [0, 26], we trust it.
    //
    // The known-good names below are descriptor identifiers, not the full
    // .mitem path — sub_F07C40 normalizes its input internally.
    bool ValidateCandidate(TD::InventoryConfig* cand)
    {
        if (!cand) return false;
        PFN_LookupByName lookup = GetEngineLookup();

        static const char* kProbeNames[] = {
            "player_jacket_premade_template",
            "player_shirt_premade_template",
            "player_chest_premade_template",
            "player_pants_premade_template",
        };

        for (const char* name : kProbeNames)
        {
            char buf[256];
            std::size_t i = 0;
            for (; i < sizeof(buf) - 1 && name[i]; ++i) buf[i] = name[i];
            buf[i] = '\0';

            __int64 result = 0;
            __try
            {
                result = lookup(reinterpret_cast<__int64>(cand), buf);
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                return false;
            }
            if (result == 0) continue;

            // Read myEquipmentSlot at +64 from the descriptor
            int slot = 0;
            __try
            {
                slot = *reinterpret_cast<const int*>(
                    reinterpret_cast<const std::uint8_t*>(result) + 64);
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                return false;
            }
            if (slot >= 0 && slot <= 26) return true;
        }
        return false;
    }

    // Walk all committed pages in the process and probe each 8-byte aligned
    // address for the InventoryConfig signature. Returns the first validated
    // pointer or nullptr.
    //
    // We skip the game's image region (the InventoryConfig is heap-allocated)
    // and any pages that are executable (code, not data). This narrows the
    // scan dramatically — most committed heap is a few hundred MB.
    TD::InventoryConfig* ScanForCfg()
    {
        // Determine the game module's address range (used by the vtable
        // pre-filter). The game .exe is loaded at g_pBase; walk forward
        // through committed MEM_IMAGE pages to find its end.
        g_modLow  = g_pBase;
        g_modHigh = g_pBase + 0x10000000; // generous default upper bound
        {
            std::uintptr_t walk = g_pBase;
            MEMORY_BASIC_INFORMATION mbi{};
            while (VirtualQuery(reinterpret_cast<void*>(walk), &mbi, sizeof(mbi)) != 0
                   && reinterpret_cast<std::uintptr_t>(mbi.AllocationBase) == g_pBase)
            {
                walk = reinterpret_cast<std::uintptr_t>(mbi.BaseAddress) + mbi.RegionSize;
                if (walk <= reinterpret_cast<std::uintptr_t>(mbi.BaseAddress)) break;
                g_modHigh = walk;
            }
        }

        SYSTEM_INFO si{};
        GetSystemInfo(&si);
        std::uintptr_t addr = reinterpret_cast<std::uintptr_t>(si.lpMinimumApplicationAddress);
        std::uintptr_t maxA = reinterpret_cast<std::uintptr_t>(si.lpMaximumApplicationAddress);

        std::uint64_t regions = 0;
        std::uint64_t bytes   = 0;
        std::uint64_t cands   = 0;
        TD::InventoryConfig* found = nullptr;

        while (addr < maxA)
        {
            MEMORY_BASIC_INFORMATION mbi{};
            if (VirtualQuery(reinterpret_cast<void*>(addr), &mbi, sizeof(mbi)) == 0)
                break;

            std::uintptr_t nextAddr = reinterpret_cast<std::uintptr_t>(mbi.BaseAddress) + mbi.RegionSize;
            if (nextAddr <= addr) break; // safety

            // Filter: committed, readable, NOT executable (we want heap, not code).
            // Also skip MEM_IMAGE (game's loaded modules — InventoryConfig is heap).
            bool usable = mbi.State == MEM_COMMIT
                       && !(mbi.Protect & (PAGE_GUARD | PAGE_NOACCESS))
                       && !(mbi.Protect & (PAGE_EXECUTE | PAGE_EXECUTE_READ |
                                            PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY))
                       && mbi.Type != MEM_IMAGE
                       && mbi.RegionSize >= 0x200;

            if (usable)
            {
                ++regions;
                bytes += mbi.RegionSize;

                std::uintptr_t base = reinterpret_cast<std::uintptr_t>(mbi.BaseAddress);
                std::uintptr_t end  = base + mbi.RegionSize;
                // Need at least 0x1B0 bytes to validate
                if (mbi.RegionSize >= 0x1B0)
                {
                    for (std::uintptr_t p = base; p + 0x1B0 < end; p += 8)
                    {
                        ++cands;
                        const void* candidate = reinterpret_cast<const void*>(p);
                        if (LooksLikeInventoryConfig(candidate))
                        {
                            TD::InventoryConfig* trial = const_cast<TD::InventoryConfig*>(
                                reinterpret_cast<const TD::InventoryConfig*>(candidate));
                            if (ValidateCandidate(trial))
                            {
                                found = trial;
                                break;
                            }
                        }
                    }
                }
            }

            if (found) break;
            addr = nextAddr;
        }

        g_lastScanRegionCt = regions;
        g_lastScanByteCt   = bytes;
        g_lastScanCandCt   = cands;
        g_lastScanResult   = found ? "FOUND" : "not found";
        return found;
    }
}

namespace ItemDescriptorCache
{
    bool Init()
    {
        // No-op on first call — defer the scan until something actually
        // requests it (via TryCapture()). Scanning all of process memory at
        // game init would block startup for several seconds; we'd rather
        // run it on demand from the probe UI button.
        g_initAttempted = true;
        return true;
    }

    // Walks the engine's item-system singleton chain to InventoryConfig.
    // Five SEH-guarded reads; null at any level short-circuits to nullptr.
    // POD-only, no destructible C++ locals (C2712).
    static TD::InventoryConfig* WalkChainGuarded()
    {
        if (!g_pBase) return nullptr;
        TD::InventoryConfig* cfg = nullptr;
        __try
        {
            void* singleton = *reinterpret_cast<void**>(g_pBase + 0x4688B28);
            if (!singleton) return nullptr;

            void* sub1 = *reinterpret_cast<void**>(
                reinterpret_cast<std::uintptr_t>(singleton) + 0x120);
            if (!sub1) return nullptr;

            void* sub2 = *reinterpret_cast<void**>(
                reinterpret_cast<std::uintptr_t>(sub1) + 0x28);
            if (!sub2) return nullptr;

            void* owner = *reinterpret_cast<void**>(
                reinterpret_cast<std::uintptr_t>(sub2) + 0x138);
            if (!owner) return nullptr;

            cfg = *reinterpret_cast<TD::InventoryConfig**>(
                reinterpret_cast<std::uintptr_t>(owner) + 0xD8);
        }
        __except (EXCEPTION_EXECUTE_HANDLER) { return nullptr; }
        return cfg;
    }

    bool TryCapture()
    {
        if (g_pInventoryConfig.load(std::memory_order_acquire)) return true;
        if (!g_pBase) return false;

        TD::InventoryConfig* cfg = WalkChainGuarded();
        g_scanRan = true;
        if (cfg && ValidateCandidate(cfg))
        {
            g_pInventoryConfig.store(cfg, std::memory_order_release);
            g_lastScanResult = "FOUND via singleton chain";
            return true;
        }
        g_lastScanResult = cfg
            ? "chain resolved but ValidateCandidate failed (offsets drifted?)"
            : "chain returned null (item system not loaded yet?)";
        return false;
    }

    TD::InventoryConfig* GetCfg()
    {
        // Fast path: already captured.
        auto* cached = g_pInventoryConfig.load(std::memory_order_acquire);
        if (cached) return cached;
        // Lazy capture — five SEH-guarded reads. Cheap enough to attempt
        // on every call until the item system finishes loading, after
        // which we permanently hit the fast path above.
        TryCapture();
        return g_pInventoryConfig.load(std::memory_order_acquire);
    }

    TD::ItemDescriptor* LookupByName(const char* itemName)
    {
        TD::InventoryConfig* cfg = g_pInventoryConfig.load(std::memory_order_acquire);
        if (!cfg || !itemName || !*itemName) return nullptr;

        char buf[256];
        std::size_t i = 0;
        for (; i < sizeof(buf) - 1 && itemName[i]; ++i) buf[i] = itemName[i];
        buf[i] = '\0';

        __int64 result = 0;
        __try
        {
            result = GetEngineLookup()(reinterpret_cast<__int64>(cfg), buf);
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            return nullptr;
        }
        return reinterpret_cast<TD::ItemDescriptor*>(result);
    }

    // Diagnostic accessors. These return scan stats now (the hook-based
    // counters are gone since we no longer hook anything).
    bool          IsHookInstalled()        { return g_pInventoryConfig.load() != nullptr; }
    std::uint64_t GetHookCallCount()       { return g_lastScanCandCt; }
    std::uint64_t GetHookNullCfgCount()    { return 0; }
    const char*   GetLastStatusName()      { return g_lastScanResult; }
    int           GetLastStatusCode()      { return g_scanRan ? (g_pInventoryConfig.load() ? 0 : 1) : -1; }
    const char*   GetLastStepName()        { return g_scanRan ? "ScanForCfg" : "not scanned"; }
    void*         GetHookTargetAddress()   { return reinterpret_cast<void*>(g_pBase + kLookupByNameRVA); }

    PageDiag GetPageDiag()
    {
        PageDiag pd{};
        pd.allocationBase = g_lastScanRegionCt;
        pd.state          = static_cast<unsigned long>(g_lastScanByteCt);
        pd.protect        = static_cast<unsigned long>(g_lastScanByteCt >> 32);
        pd.type           = static_cast<unsigned long>(g_lastScanCandCt);
        pd.vqLastError    = 0;
        pd.vpLastError    = 0;
        return pd;
    }

    // Typed accessors (unchanged from the hook-based version).
    int GetEquipmentSlot(TD::ItemDescriptor* desc)
    {
        if (!desc) return 0;
        int v = 0;
        __try { v = *reinterpret_cast<const int*>(reinterpret_cast<const BYTE*>(desc) + 64); }
        __except (EXCEPTION_EXECUTE_HANDLER) { v = 0; }
        return v;
    }

    int GetAttributeGenType(TD::ItemDescriptor* desc)
    {
        if (!desc) return 0;
        int v = 0;
        __try { v = *reinterpret_cast<const int*>(reinterpret_cast<const BYTE*>(desc) + 68); }
        __except (EXCEPTION_EXECUTE_HANDLER) { v = 0; }
        return v;
    }

    int GetInventoryCategory(TD::ItemDescriptor* desc)
    {
        if (!desc) return 0;
        int v = 0;
        __try { v = *reinterpret_cast<const int*>(reinterpret_cast<const BYTE*>(desc) + 88); }
        __except (EXCEPTION_EXECUTE_HANDLER) { v = 0; }
        return v;
    }

    static const char* ReadSnowdropPathAt(TD::ItemDescriptor* desc, std::size_t off,
                                          char* outBuf, std::size_t outSize)
    {
        if (!outBuf || outSize == 0) return "";
        outBuf[0] = '\0';
        if (!desc) return outBuf;
        const BYTE* sstr = reinterpret_cast<const BYTE*>(desc) + off;
        __try
        {
            bool isHeap = sstr[0x0F] != 0;
            if (isHeap)
            {
                const char* heap = *reinterpret_cast<const char* const*>(sstr);
                if (heap)
                {
                    std::uint32_t cap = *reinterpret_cast<const std::uint32_t*>(heap - 4);
                    if (cap > 0 && cap <= 0x1000)
                    {
                        std::size_t copy = cap < outSize - 1 ? cap : outSize - 1;
                        std::memcpy(outBuf, heap, copy);
                        outBuf[copy] = '\0';
                    }
                }
            }
            else
            {
                std::size_t copy = outSize - 1 < 15 ? outSize - 1 : 15;
                std::memcpy(outBuf, sstr, copy);
                outBuf[copy] = '\0';
            }
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            outBuf[0] = '\0';
        }
        return outBuf;
    }

    const char* GetMaleVisualGearPath(TD::ItemDescriptor* desc, char* outBuf, std::size_t outSize)
    {
        return ReadSnowdropPathAt(desc, 368, outBuf, outSize);
    }

    const char* GetFemaleVisualGearPath(TD::ItemDescriptor* desc, char* outBuf, std::size_t outSize)
    {
        return ReadSnowdropPathAt(desc, 384, outBuf, outSize);
    }
}
