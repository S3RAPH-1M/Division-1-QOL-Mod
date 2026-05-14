#include "UIInspector.h"
#include "imgui/imgui.h"
#include <Windows.h>
#include <intrin.h>
#include <cstring>
#include <cstdio>

// RVA of qword_480ECE0 (the UI graph manager singleton pointer).
// See .claude/docs/09e-ui-runtime-state.md / 09f for the chain.
static constexpr std::uintptr_t kUIGraphMgrRVA = 0x480ECE0;

UIInspector::UIInspector()  {}
UIInspector::~UIInspector() {}

// Heap-range filter; same logic AgentInspector used. Avoids the kernel
// transition cost of VirtualQuery in the hot path — the surrounding SEH
// already catches any AV from stale-but-in-range pointers.
static inline bool LooksLikeHeapPtr(std::uintptr_t p)
{
    return p >= 0x10000000000ULL && p < 0x2300000000000ULL;
}

// POD-only SEH helper: copy up to maxLen bytes from `src` into `dst`,
// stopping at first NUL or at the first non-printable byte. Returns the
// number of bytes actually copied (excluding the NUL we write). C2712
// compliance: no C++ objects with destructors in this function.
static int SafeCopyCString(std::uintptr_t src, char* dst, int maxLen)
{
    if (maxLen <= 0) return 0;
    int n = 0;
    __try
    {
        const char* s = reinterpret_cast<const char*>(src);
        while (n < maxLen - 1)
        {
            unsigned char c = static_cast<unsigned char>(s[n]);
            if (c == 0) break;
            // Allow printable ASCII + tab/newline; bail on anything else
            // (catches the secondary pointer when it doesn't point at text).
            if (c < 0x09 || (c > 0x0D && c < 0x20) || c > 0x7E) break;
            dst[n] = static_cast<char>(c);
            ++n;
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        n = 0;
    }
    dst[n] = 0;
    return n;
}

// SEH-guarded read of N bytes. Returns count actually copied (0 on failure).
// POD-only locals.
static int SafeReadBytes(std::uintptr_t src, std::uint8_t* dst, int len)
{
    if (len <= 0) return 0;
    __try
    {
        const std::uint8_t* s = reinterpret_cast<const std::uint8_t*>(src);
        for (int i = 0; i < len; ++i) dst[i] = s[i];
        return len;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return 0;
    }
}

// ─── Engine BVM primitives (callable from our DLL via g_pBase + RVA) ────────
// All discovered + decoded in .claude/docs/09g §4 and 09e §7d.
// Signatures inferred from decompiled callers — do not reorder args.
namespace BvmAPI
{
    // sub_15BDC0: lookup a value by dotted-key path within a dict-typed BVM value.
    //   value_ptr — pointer to BVM value (the dict to walk)
    //   key_str   — char* or SDS-shaped buffer ("foo" or "foo.bar.baz")
    // Returns: BVM value pointer (with ref incremented) or 0.
    constexpr std::uintptr_t kLookupPathRVA = 0x15BDC0;
    using Fn_LookupPath = std::uintptr_t (__fastcall *)(const void* value_ptr, const char* key);

    // sub_142C10: check if value is a dict, and if so, init an iterator-holder.
    //   value, out_iter (zero-init by caller — engine fills it)
    // Returns: 1 = is a dict, 0 = wrong type.
    constexpr std::uintptr_t kAsDictRVA = 0x142C10;
    using Fn_AsDict = std::int8_t (__fastcall *)(const void* value, void* out_iter_state);

    // sub_142760: read as float (type byte 3) or auto-int-to-float (type byte 1).
    constexpr std::uintptr_t kAsFloatRVA = 0x142760;
    using Fn_AsFloat = std::int8_t (__fastcall *)(const void* value, float* out);

    // sub_1427B0: read as string (type 12/13). Writes into a 16-byte SDS slot.
    constexpr std::uintptr_t kAsStringRVA = 0x1427B0;
    using Fn_AsString = std::int8_t (__fastcall *)(const void* value, void* out_sds16);

    static Fn_LookupPath LookupPath() {
        return reinterpret_cast<Fn_LookupPath>(g_pBase + kLookupPathRVA);
    }
    static Fn_AsDict AsDict() {
        return reinterpret_cast<Fn_AsDict>(g_pBase + kAsDictRVA);
    }
    static Fn_AsFloat AsFloat() {
        return reinterpret_cast<Fn_AsFloat>(g_pBase + kAsFloatRVA);
    }
    static Fn_AsString AsString() {
        return reinterpret_cast<Fn_AsString>(g_pBase + kAsStringRVA);
    }
}

// SDS in Snowdrop: 16-byte buffer.
// byte 15 = "is heap" flag. If 1, bytes 0..7 hold a heap pointer to the chars.
// If 0, bytes 0..14 hold the chars inline (15-byte max + NUL).
// Used by SafeReadSDS to convert into a std::string.
static std::string SdsToString(const std::uint8_t sds[16])
{
    if (sds[15])
    {
        std::uintptr_t heap = 0;
        std::memcpy(&heap, sds, sizeof(heap));
        char buf[260] = {};
        SafeCopyCString(heap, buf, sizeof(buf));
        return std::string(buf);
    }
    // Inline mode — chars stored in the 15-byte buffer.
    char buf[16] = {};
    std::memcpy(buf, sds, 15);
    buf[15] = 0;
    return std::string(buf);
}

// POD-only SEH wrapper around AsDict.
static bool SafeAsDict(const void* value, void* out_iter)
{
    __try { return BvmAPI::AsDict()(value, out_iter) != 0; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

// POD-only SEH wrapper around LookupPath.
static std::uintptr_t SafeLookupPath(const void* value, const char* key)
{
    __try { return BvmAPI::LookupPath()(value, key); }
    __except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
}

// POD-only SEH wrapper around AsString.
static bool SafeAsString(const void* value, std::uint8_t out[16])
{
    __try { return BvmAPI::AsString()(value, out) != 0; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

// ─── Shipped-graph loader (Path 1: load existing-but-unloaded .muigraph) ────
// Replicates sub_1B72FF0's per-file activation pattern. The engine does the
// same thing for every graph at startup, so this should reach the same code
// path the streamer uses to actually instantiate wrappers.
//
// Per-file recipe (from sub_1B72FF0's inner loop, fully decompiled):
//
//   void*  wrapper = nullptr;
//   char   flag    = 1;
//   sub_1AE39F0(&wrapper, sub_1B9D950, path_cstr, &flag);
//   if (wrapper && InterlockedDecrement(wrapper+8) == 0)
//       vtable[0](wrapper, 1);   // activate slot 0 of outer vtable
//
// Notes:
//  * sub_1AE39F0 heap-allocates a 160-byte deferred-call object, initializes
//    its outer vtable (slot 0 = activate/destroy polymorphic), captures
//    (callback, path, flag) into a 280-byte heap config blob, and schedules
//    via sub_160E10 → sub_178660 into worker pool 1.
//  * sub_1B9D950 is the engine's per-asset-arrived callback. We pass it as
//    the "what to do after asset is loaded" function — same as startup.
//  * Path arg is a NUL-terminated char* (sub_1256B0 just walks bytes). Not
//    an SDS — we pass a literal string pointer directly.
//  * Refcount: sub_108BD0 sets +0x08 = 2. Anyone holding a reference (us +
//    scheduler) must decrement. When it hits 0, slot 0 fires.
//  * Gate byte at [Client+0x90]+0x2B1 — forced to 1 around the call, same
//    as sub_1B327E0 does at startup.
namespace ShippedGraphLoader
{
    constexpr std::uintptr_t kSub_1AE39F0_RVA = 0x1AE39F0;
    constexpr std::uintptr_t kSub_1B9D950_RVA = 0x1B9D950;

    using Fn_BuildDeferred = void* (__fastcall *)(
        void** out_wrapper, void* callback, const char* path, char* flag);
    using Fn_Activate = void (__fastcall *)(void* self, int arg);

    struct Candidate
    {
        const char* path;
        const char* note;
    };

    // Curated list: the 9 debug graphs + 1 example. All exist in the asset
    // bundles per the unpacked snowdrop-source dump; none are on the retail
    // startup load list (the cmdline gate `-enabledebugui` keeps them off).
    static const Candidate kCandidates[] = {
        { "rogue/ui/graphs/debug/ui_debug_displayinput.muigraph",       "debug: input display" },
        { "rogue/ui/graphs/debug/ui_debug_lootextraction.muigraph",     "debug: loot extraction" },
        { "rogue/ui/graphs/debug/ui_debug_propsagentinfo.muigraph",     "debug: props / agent info" },
        { "rogue/ui/graphs/debug/ui-debug agent stimulus.muigraph",     "debug: agent stimulus" },
        { "rogue/ui/graphs/debug/ui-debug boo overview.muigraph",       "debug: boo overview" },
        { "rogue/ui/graphs/debug/ui-debug combat state.muigraph",       "debug: combat state" },
        { "rogue/ui/graphs/debug/ui-debug map reference debug info.muigraph", "debug: map ref info" },
        { "rogue/ui/graphs/debug/ui-debug survival matchmaking.muigraph",     "debug: survival mm" },
        { "rogue/ui/graphs/debug/ui-debug_global_colors.muigraph",      "debug: global colors" },
        { "rogue/ui/graphs/examples/ui features/example - render pass test.muigraph", "example: render pass" },
    };

    // Last-attempt diagnostics shown in the UI.
    struct LastAttempt
    {
        const char*    path;
        std::int32_t   mgrCountBefore;
        std::int32_t   mgrCountAfter;
        std::uint8_t   gateBefore;
        bool           gateResolved;
        bool           buildOk;
        bool           activateOk;
        bool           wrapperNonNull;
        bool           activated;       // refcount-hit-0 path was taken
        std::uintptr_t wrapperAddr;
        std::int32_t   refcountAfterDec;
    };
    static LastAttempt g_last = { nullptr, -1, -1, 0xFF, false, false, false, false, false, 0, -1 };

    // POD-only SEH helper. Caller must use C++-aware code separately.
    static std::uint8_t* SafeResolveGateByte()
    {
        __try
        {
            TD::RogueClient* rc = TD::RogueClient::Singleton();
            if (!rc) return nullptr;
            TD::Client* client = rc->m_pClient;
            if (!client) return nullptr;
            void* innerMgr = *reinterpret_cast<void**>(
                reinterpret_cast<std::uint8_t*>(client) + 0x90);
            if (!innerMgr) return nullptr;
            return reinterpret_cast<std::uint8_t*>(innerMgr) + 0x2B1;
        }
        __except (EXCEPTION_EXECUTE_HANDLER) { return nullptr; }
    }

    // POD-only SEH wrapper around sub_1AE39F0. Builds the deferred-call
    // wrapper. Returns true on clean return, populates *out_wrapper.
    static bool SafeBuildDeferred(const char* path, void** out_wrapper)
    {
        *out_wrapper = nullptr;
        __try
        {
            auto build = reinterpret_cast<Fn_BuildDeferred>(g_pBase + kSub_1AE39F0_RVA);
            void* cb = reinterpret_cast<void*>(g_pBase + kSub_1B9D950_RVA);
            char  flag = 1;
            build(out_wrapper, cb, path, &flag);
            return true;
        }
        __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
    }

    // POD-only SEH wrapper around the activation virtual call. The decrement
    // mirrors sub_1B72FF0's gate: if the refcount-after-decrement is 0, slot 0
    // does the actual work (asset request → wrapper instantiation). Otherwise
    // the scheduler will fire it asynchronously and we just hand off.
    // Writes refcount-after-decrement into *out_refcount for diagnostics.
    // Sets *out_activated = true iff we fired slot 0.
    static bool SafeActivate(void* wrapper, std::int32_t* out_refcount,
                             bool* out_activated)
    {
        *out_refcount  = -1;
        *out_activated = false;
        __try
        {
            auto refcountPtr = reinterpret_cast<volatile long*>(
                reinterpret_cast<std::uint8_t*>(wrapper) + 8);
            long after = _InterlockedDecrement(refcountPtr);
            *out_refcount = static_cast<std::int32_t>(after);
            if (after == 0)
            {
                auto vtbl = *reinterpret_cast<void***>(wrapper);
                auto act  = reinterpret_cast<Fn_Activate>(vtbl[0]);
                act(wrapper, 1);
                *out_activated = true;
            }
            return true;
        }
        __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
    }

    // Read manager count via the same singleton + +0x28 chain UIInspector uses.
    static std::int32_t SafeReadMgrCount()
    {
        __try
        {
            void* mgr = *reinterpret_cast<void**>(g_pBase + 0x480ECE0);
            if (!mgr) return -1;
            return *reinterpret_cast<std::int32_t*>(
                reinterpret_cast<std::uint8_t*>(mgr) + 0x28);
        }
        __except (EXCEPTION_EXECUTE_HANDLER) { return -1; }
    }

    // Try-load primitive: set gate, build the deferred-call wrapper, decrement
    // refcount, activate slot 0 if it dropped to 0, then restore the gate.
    // All call-state lives in g_last so the UI can render it next frame.
    static void Attempt(const char* path)
    {
        g_last.path             = path;
        g_last.mgrCountBefore   = SafeReadMgrCount();
        g_last.mgrCountAfter    = -1;
        g_last.gateResolved     = false;
        g_last.gateBefore       = 0xFF;
        g_last.buildOk          = false;
        g_last.activateOk       = false;
        g_last.wrapperNonNull   = false;
        g_last.activated        = false;
        g_last.wrapperAddr      = 0;
        g_last.refcountAfterDec = -1;

        std::uint8_t* gate = SafeResolveGateByte();
        if (gate)
        {
            g_last.gateResolved = true;
            g_last.gateBefore   = *gate;
            *gate = 1;
        }

        void* wrapper = nullptr;
        g_last.buildOk = SafeBuildDeferred(path, &wrapper);
        g_last.wrapperAddr    = reinterpret_cast<std::uintptr_t>(wrapper);
        g_last.wrapperNonNull = (wrapper != nullptr);

        if (wrapper)
        {
            g_last.activateOk = SafeActivate(
                wrapper, &g_last.refcountAfterDec, &g_last.activated);
        }

        if (gate) *gate = g_last.gateBefore;

        g_last.mgrCountAfter = SafeReadMgrCount();
    }
}

// ─── UI:Root field offsets ───────────────────────────────────────────────────
// Decoded from the runtime Deserialize function (sub_1BA0800) at RVA 0x1BA0800.
// `wrap+0x90` dereferences to a Gfx_UINodeGraphRoot instance whose fields sit
// at the offsets below relative to that instance. Engine reads them every
// frame — no caching — so writes take effect on the next tick. ACG-safe.
namespace UIRootFields
{
    constexpr std::size_t kWrapToRoot     = 0x90;  // wrapper -> UI:Root ptr
    constexpr std::size_t kMyEditorEnable = 0x28;  // byte
    constexpr std::size_t kMyFactionsPtr  = 0x30;  // qword
    constexpr std::size_t kMyFactionsCnt  = 0x38;  // int
    constexpr std::size_t kMyFactionsCap  = 0x3C;  // int
    constexpr std::size_t kMyTargetType   = 0x40;  // int (enum)
    constexpr std::size_t kMyGameStates   = 0x44;  // uint32 bitmask (22 bits)
    constexpr std::size_t kMyScheduling   = 0x4C;  // int
    constexpr std::size_t kMyEnabled      = 0x50;  // byte ← MASTER TOGGLE
    constexpr std::size_t kMyInteract     = 0x58;  // bitset ptr
    constexpr std::size_t kMyActiveDist   = 0x68;  // float
    constexpr std::size_t kMyInstanceCap  = 0x6C;  // int
    constexpr std::size_t kMyDisableDead  = 0x70;  // byte
    constexpr std::size_t kMyDisableGrp   = 0x71;  // byte
    constexpr std::size_t kMyConcurrency  = 0x72;  // byte
}

// ─── Game-state names (matches game's RClient_GameStateEnum at sub_285A350) ─
// Live bitfield at root+0x44 has 22 bits, one per game state. Order matters —
// the enum order is what defines bit indices.
static const char* kGameStateNames[] = {
    "Startup", "SplashScreen", "LanguageSelect", "BrightnessSetup",
    "GeneralError", "Login", "Logout", "AccountLinking",
    "MainMenu", "DownloadProfile", "SetupGame", "BetaScreen",
    "Loading", "ResumeFromSuspend", "InstallLaunchChunk", "InstallFullGame",
    "DownloadDLC", "Normal", "WaitForCinematic", "Cinematic",
    "ScreenShotMode", "Spectator"
};

// Is this a likely code/rdata pointer inside TheDivision.exe (or near it)?
// Used to flag "this looks like a vtable" — we treat the first 8 bytes of a
// candidate live object as a vtable if it lands in the exe's module range.
static inline bool LooksLikeCodePtr(std::uintptr_t p)
{
    // The Division image is 120MB. base..base+0x7311000.
    // Be a bit generous to also catch close .rdata.
    if (!g_pBase) return false;
    auto v = static_cast<std::uintptr_t>(p);
    return v >= g_pBase && v < (g_pBase + 0x10000000ULL);
}

// ─── Live UI:Root snapshot (read) ────────────────────────────────────────────
struct UIRootSnap
{
    std::uintptr_t rootAddr;        // wrap+0x90 dereferenced
    std::uintptr_t vtable;
    std::uint8_t   myEditorEnabled;
    std::uint8_t   myEnabled;
    std::uint8_t   myDisableWhenAgentDead;
    std::uint8_t   myDisableIfGroupmember;
    std::uint8_t   myUseConcurrency;
    std::int32_t   myTargetType;
    std::uint32_t  myGameStatesString;
    std::int32_t   myScheduling;
    std::int32_t   myInstanceCap;
    float          myActiveDistance;
    std::int32_t   myFactionsCount;
    std::int32_t   myFactionsCap;
    bool           valid;
};

static bool SafeReadUIRoot(std::uintptr_t wrap, UIRootSnap* out)
{
    out->valid = false;
    __try
    {
        std::uintptr_t root = *reinterpret_cast<std::uintptr_t*>(wrap + UIRootFields::kWrapToRoot);
        if (root < 0x10000000000ULL || root >= 0x2300000000000ULL) return false;
        out->rootAddr = root;
        out->vtable   = *reinterpret_cast<std::uintptr_t*>(root + 0x00);
        out->myEditorEnabled       = *reinterpret_cast<std::uint8_t*>(root + UIRootFields::kMyEditorEnable);
        out->myEnabled             = *reinterpret_cast<std::uint8_t*>(root + UIRootFields::kMyEnabled);
        out->myDisableWhenAgentDead= *reinterpret_cast<std::uint8_t*>(root + UIRootFields::kMyDisableDead);
        out->myDisableIfGroupmember= *reinterpret_cast<std::uint8_t*>(root + UIRootFields::kMyDisableGrp);
        out->myUseConcurrency      = *reinterpret_cast<std::uint8_t*>(root + UIRootFields::kMyConcurrency);
        out->myTargetType          = *reinterpret_cast<std::int32_t*>(root + UIRootFields::kMyTargetType);
        out->myGameStatesString    = *reinterpret_cast<std::uint32_t*>(root + UIRootFields::kMyGameStates);
        out->myScheduling          = *reinterpret_cast<std::int32_t*>(root + UIRootFields::kMyScheduling);
        out->myInstanceCap         = *reinterpret_cast<std::int32_t*>(root + UIRootFields::kMyInstanceCap);
        out->myActiveDistance      = *reinterpret_cast<float*>(root + UIRootFields::kMyActiveDist);
        out->myFactionsCount       = *reinterpret_cast<std::int32_t*>(root + UIRootFields::kMyFactionsCnt);
        out->myFactionsCap         = *reinterpret_cast<std::int32_t*>(root + UIRootFields::kMyFactionsCap);
        out->valid = true;
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

// POD-only setters. Each writes ONE field. SEH-guarded so a bad wrapper
// pointer doesn't crash. Pattern: read the root pointer, validate it,
// write the byte/int/float at the known offset.
static bool SafeWriteByte(std::uintptr_t wrap, std::size_t off, std::uint8_t v)
{
    __try
    {
        std::uintptr_t root = *reinterpret_cast<std::uintptr_t*>(wrap + UIRootFields::kWrapToRoot);
        if (root < 0x10000000000ULL || root >= 0x2300000000000ULL) return false;
        *reinterpret_cast<std::uint8_t*>(root + off) = v;
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

static bool SafeWriteI32(std::uintptr_t wrap, std::size_t off, std::int32_t v)
{
    __try
    {
        std::uintptr_t root = *reinterpret_cast<std::uintptr_t*>(wrap + UIRootFields::kWrapToRoot);
        if (root < 0x10000000000ULL || root >= 0x2300000000000ULL) return false;
        *reinterpret_cast<std::int32_t*>(root + off) = v;
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

static bool SafeWriteU32(std::uintptr_t wrap, std::size_t off, std::uint32_t v)
{
    __try
    {
        std::uintptr_t root = *reinterpret_cast<std::uintptr_t*>(wrap + UIRootFields::kWrapToRoot);
        if (root < 0x10000000000ULL || root >= 0x2300000000000ULL) return false;
        *reinterpret_cast<std::uint32_t*>(root + off) = v;
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

static bool SafeWriteF32(std::uintptr_t wrap, std::size_t off, float v)
{
    __try
    {
        std::uintptr_t root = *reinterpret_cast<std::uintptr_t*>(wrap + UIRootFields::kWrapToRoot);
        if (root < 0x10000000000ULL || root >= 0x2300000000000ULL) return false;
        *reinterpret_cast<float*>(root + off) = v;
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

// SEH-guarded read of one entry slot. Returns true on success.
struct EntryRaw
{
    std::uintptr_t pathPtr;
    std::uint64_t  tagWord;
    std::uintptr_t secondary;
};

static bool SafeReadEntry(std::uintptr_t entryAddr, EntryRaw* out)
{
    __try
    {
        const std::uintptr_t* p = reinterpret_cast<const std::uintptr_t*>(entryAddr);
        out->pathPtr   = p[0];
        out->tagWord   = reinterpret_cast<const std::uint64_t*>(entryAddr)[1];
        out->secondary = p[2];
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

// POD-only SEH wrapper for the single qword deref through qword_480ECE0.
// Pulled out so Refresh() (which uses std::vector / std::string) doesn't end
// up with __try and object unwinding in the same function (MSVC C2712).
static std::uintptr_t SafeReadPtr(std::uintptr_t slotAddr)
{
    __try
    {
        return *reinterpret_cast<const std::uintptr_t*>(slotAddr);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return 0;
    }
}

// SEH-guarded read of the manager header (vtable, array ptr, count, cap).
struct MgrSnap
{
    std::uintptr_t vtbl;
    std::uintptr_t arrayPtr;
    std::int32_t   count;
    std::int32_t   cap;
};

static bool SafeReadManagerSnap(std::uintptr_t mgrAddr, MgrSnap* out)
{
    __try
    {
        const std::uint8_t* m = reinterpret_cast<const std::uint8_t*>(mgrAddr);
        out->vtbl     = *reinterpret_cast<const std::uintptr_t*>(m + 0x00);
        out->arrayPtr = *reinterpret_cast<const std::uintptr_t*>(m + 0x20);
        out->count    = *reinterpret_cast<const std::int32_t*>(m + 0x28);
        out->cap      = *reinterpret_cast<const std::int32_t*>(m + 0x2C);
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

void UIInspector::Refresh()
{
    m_entries.clear();
    m_managerAddr = 0;
    m_arrayPtr    = 0;
    m_arrayCount  = -1;
    m_arrayCap    = -1;
    m_managerVtbl = 0;

    if (!g_pBase) return;

    std::uintptr_t mgr = SafeReadPtr(g_pBase + kUIGraphMgrRVA);
    if (!LooksLikeHeapPtr(mgr)) return;
    m_managerAddr = mgr;

    MgrSnap snap{};
    if (!SafeReadManagerSnap(mgr, &snap)) return;
    m_managerVtbl = snap.vtbl;
    m_arrayPtr    = snap.arrayPtr;
    m_arrayCount  = snap.count;
    m_arrayCap    = snap.cap;

    if (!LooksLikeHeapPtr(m_arrayPtr)) return;
    if (m_arrayCount < 0 || m_arrayCount > 4096) return;  // sanity

    m_entries.reserve(m_arrayCount);
    for (int i = 0; i < m_arrayCount; ++i)
    {
        std::uintptr_t entryAddr = m_arrayPtr + static_cast<std::uintptr_t>(i) * 24;
        EntryRaw raw{};
        if (!SafeReadEntry(entryAddr, &raw)) continue;

        Entry e{};
        e.entryAddr = entryAddr;
        e.pathPtr   = raw.pathPtr;
        e.secondary = raw.secondary;
        // tag = first 4 bytes of tagWord, unk1 = upper 4 bytes
        std::memcpy(e.tag, &raw.tagWord, 4);
        for (int b = 0; b < 4; ++b)
            if (e.tag[b] < 0x20 || e.tag[b] > 0x7E) e.tag[b] = '?';
        e.tag[4] = 0;
        e.unk1 = static_cast<std::uint32_t>(raw.tagWord >> 32);

        char buf[260];
        if (LooksLikeHeapPtr(e.pathPtr) && SafeCopyCString(e.pathPtr, buf, sizeof(buf)) > 0)
            e.path = buf;

        if (LooksLikeHeapPtr(e.secondary) && SafeCopyCString(e.secondary, buf, sizeof(buf)) > 0)
            e.secondaryStr = buf;

        m_entries.push_back(std::move(e));
    }
}

void UIInspector::DrawUI()
{
    if (!m_loadedOnce)
    {
        Refresh();
        m_loadedOnce = true;
    }

    ImGui::Text("Live UI graph manager (qword_480ECE0):");
    if (!m_managerAddr)
    {
        ImGui::TextColored(ImVec4(1, 0.4f, 0.4f, 1),
            "  manager pointer is null — engine not ready or wrong RVA");
        if (ImGui::Button("Retry")) Refresh();
        return;
    }
    ImGui::Text("  this:        0x%p", reinterpret_cast<void*>(m_managerAddr));
    ImGui::Text("  vtable:      0x%p", reinterpret_cast<void*>(m_managerVtbl));
    ImGui::Text("  array:       0x%p", reinterpret_cast<void*>(m_arrayPtr));
    ImGui::Text("  count / cap: %d / %d   (entries are 24 bytes each)",
                m_arrayCount, m_arrayCap);
    ImGui::Spacing();

    ImGui::SetNextItemWidth(260);
    ImGui::InputTextWithHint("##uifilter", "filter path substring (case-insensitive)",
                             m_filter, sizeof(m_filter));
    ImGui::SameLine();
    if (ImGui::Button("Refresh")) Refresh();
    ImGui::SameLine();
    ImGui::Text("(%zu entries)", m_entries.size());

    ImGui::Separator();

    // Lowercase filter for case-insensitive substring match
    char filterLo[128];
    {
        int n = 0;
        for (; n < (int)sizeof(filterLo) - 1 && m_filter[n]; ++n)
            filterLo[n] = (char)std::tolower((unsigned char)m_filter[n]);
        filterLo[n] = 0;
    }

    // Two-pane layout: list on the left, details on the right
    ImGui::BeginChild("##uilist", ImVec2(420, 360), true);
    for (int i = 0; i < (int)m_entries.size(); ++i)
    {
        const Entry& e = m_entries[i];
        if (filterLo[0])
        {
            char pathLo[260];
            int n = 0;
            for (; n < (int)sizeof(pathLo) - 1 && n < (int)e.path.size(); ++n)
                pathLo[n] = (char)std::tolower((unsigned char)e.path[n]);
            pathLo[n] = 0;
            if (!std::strstr(pathLo, filterLo)) continue;
        }

        char label[320];
        std::snprintf(label, sizeof(label), "[%3d] %s##e%d",
                      i,
                      e.path.empty() ? "(no path string)" : e.path.c_str(),
                      i);
        if (ImGui::Selectable(label, m_selectedIndex == i))
            m_selectedIndex = i;
    }
    ImGui::EndChild();

    ImGui::SameLine();

    ImGui::BeginChild("##uidetail", ImVec2(0, 360), true);
    if (m_selectedIndex >= 0 && m_selectedIndex < (int)m_entries.size())
    {
        const Entry& e = m_entries[m_selectedIndex];
        ImGui::Text("Entry #%d", m_selectedIndex);
        ImGui::Text("entry addr   : 0x%p", reinterpret_cast<void*>(e.entryAddr));
        ImGui::Separator();
        ImGui::Text("path ptr     : 0x%p", reinterpret_cast<void*>(e.pathPtr));
        if (!e.path.empty())
            ImGui::TextWrapped("path str     : %s", e.path.c_str());
        else
            ImGui::TextDisabled("path str     : <unreadable>");
        ImGui::Separator();
        ImGui::Text("tag (4B)     : '%s'", e.tag);
        ImGui::Text("flag/hash    : 0x%08X", e.unk1);
        ImGui::Separator();
        ImGui::Text("secondary ptr: 0x%p", reinterpret_cast<void*>(e.secondary));
        if (!e.secondaryStr.empty())
            ImGui::TextWrapped("secondary str: %s", e.secondaryStr.c_str());
        else
            ImGui::TextDisabled("secondary str: <not text>");

        // Raw hex dump of the secondary's first 64 bytes + vtable check.
        // Lets us spot live objects: if [0..7] looks like a TheDivision.exe
        // pointer, secondary likely IS a C++ object (Gfx_UINodeGraphRoot or
        // similar) and we can walk its tree. If it's all 0/text/garbage,
        // secondary is just metadata.
        ImGui::Separator();
        if (LooksLikeHeapPtr(e.secondary))
        {
            std::uint8_t buf[64] = {};
            int got = SafeReadBytes(e.secondary, buf, sizeof(buf));
            if (got > 0)
            {
                std::uintptr_t firstQ = 0;
                std::memcpy(&firstQ, buf, sizeof(firstQ));
                const bool looksLikeVtbl = LooksLikeCodePtr(firstQ);

                ImGui::TextColored(
                    looksLikeVtbl ? ImVec4(0.5f, 1.f, 0.5f, 1.f)
                                  : ImVec4(0.8f, 0.8f, 0.8f, 1.f),
                    "[secondary+0..0x40] %s",
                    looksLikeVtbl
                        ? "first qword IS in module range — looks like a vtable / live object"
                        : "first qword NOT in module range — likely data/metadata");
                ImGui::Text("[+0x00] vtable?   = 0x%016llX", (unsigned long long)firstQ);

                // 4 rows of 16 bytes
                for (int row = 0; row < 4; ++row)
                {
                    char line[128];
                    int off = row * 16;
                    int n = std::snprintf(line, sizeof(line), "+%02X  ", off);
                    for (int c = 0; c < 16 && off + c < got; ++c)
                        n += std::snprintf(line + n, sizeof(line) - n, "%02X ", buf[off + c]);
                    n += std::snprintf(line + n, sizeof(line) - n, " ");
                    for (int c = 0; c < 16 && off + c < got; ++c)
                    {
                        unsigned char b = buf[off + c];
                        char ch = (b >= 0x20 && b <= 0x7E) ? (char)b : '.';
                        n += std::snprintf(line + n, sizeof(line) - n, "%c", ch);
                    }
                    ImGui::TextUnformatted(line);
                }
            }
            else
            {
                ImGui::TextDisabled("[secondary] read failed");
            }
        }
        else
        {
            ImGui::TextDisabled("[secondary] not a heap pointer — skipped raw dump");
        }
    }
    else
    {
        ImGui::TextDisabled("Select an entry to inspect it.");
    }
    ImGui::EndChild();

    // ─── Live HUD editor (Layer 1 + 2) ──────────────────────────────────────
    // Selected wrapper's UI:Root fields, editable. Writes propagate to the
    // running graph on the next evaluation tick (~16ms at 60fps).
    if (m_selectedIndex >= 0 && m_selectedIndex < (int)m_entries.size())
    {
        ImGui::Spacing();
        ImGui::Separator();

        const Entry& e = m_entries[m_selectedIndex];
        const std::uintptr_t wrap = e.secondary; // wrapper pointer

        UIRootSnap s{};
        if (!LooksLikeHeapPtr(wrap) || !SafeReadUIRoot(wrap, &s))
        {
            ImGui::TextColored(ImVec4(1, 0.5f, 0.5f, 1),
                "Live edit unavailable: wrapper+0x90 not a valid UI:Root ptr.");
        }
        else if (ImGui::CollapsingHeader("Live HUD Edit (UI:Root fields)",
                                         ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::TextDisabled("UI:Root @ 0x%p   vtable 0x%p",
                                reinterpret_cast<void*>(s.rootAddr),
                                reinterpret_cast<void*>(s.vtable));
            ImGui::Spacing();

            // ── REAL runtime master toggle = the game-state bitmask ──
            // myEnabled / myEditorEnabled are init/editor-only and have no
            // visible effect at runtime (user-verified). The per-frame
            // evaluator gates on myGameStatesString instead — clear the
            // bitmask and the graph stops rendering immediately.
            const bool isHidden = (s.myGameStatesString == 0);
            ImGui::PushStyleColor(ImGuiCol_Button,
                isHidden ? ImVec4(0.7f, 0.2f, 0.2f, 1.0f)
                         : ImVec4(0.25f, 0.25f, 0.25f, 1.0f));
            if (ImGui::Button(isHidden ? "[HIDDEN]  click to restore" : "Hide graph (live)",
                              ImVec2(260, 28)))
            {
                if (isHidden)
                    SafeWriteU32(wrap, UIRootFields::kMyGameStates, 0x003FFFFFu); // all states
                else
                    SafeWriteU32(wrap, UIRootFields::kMyGameStates, 0u);          // none
            }
            ImGui::PopStyleColor();
            ImGui::SameLine();
            ImGui::TextDisabled("(zeros the game-state bitmask — actual live gate)");

            ImGui::Spacing();
            ImGui::TextDisabled("Advanced (init/editor-only — may not change live):");

            bool enabled = s.myEnabled != 0;
            if (ImGui::Checkbox("myEnabled (init-only)", &enabled))
                SafeWriteByte(wrap, UIRootFields::kMyEnabled, enabled ? 1 : 0);

            bool editorEnabled = s.myEditorEnabled != 0;
            if (ImGui::Checkbox("myEditorEnabled (editor-only)", &editorEnabled))
                SafeWriteByte(wrap, UIRootFields::kMyEditorEnable, editorEnabled ? 1 : 0);

            ImGui::Separator();

            // ── Death/group behavior ──
            bool dwd = s.myDisableWhenAgentDead != 0;
            if (ImGui::Checkbox("myDisableWhenAgentDead", &dwd))
                SafeWriteByte(wrap, UIRootFields::kMyDisableDead, dwd ? 1 : 0);

            bool dgm = s.myDisableIfGroupmember != 0;
            if (ImGui::Checkbox("myDisableIfGroupmember", &dgm))
                SafeWriteByte(wrap, UIRootFields::kMyDisableGrp, dgm ? 1 : 0);

            bool conc = s.myUseConcurrency != 0;
            if (ImGui::Checkbox("myUseConcurrency", &conc))
                SafeWriteByte(wrap, UIRootFields::kMyConcurrency, conc ? 1 : 0);

            ImGui::Separator();

            // ── Numeric tunables ──
            float actDist = s.myActiveDistance;
            if (ImGui::DragFloat("myActiveDistance (m)", &actDist, 0.5f, 0.0f, 10000.0f, "%.1f"))
                SafeWriteF32(wrap, UIRootFields::kMyActiveDist, actDist);

            int insCap = s.myInstanceCap;
            if (ImGui::DragInt("myInstanceCap (-1 = unlimited)", &insCap, 1, -1, 1024))
                SafeWriteI32(wrap, UIRootFields::kMyInstanceCap, insCap);

            int sched = s.myScheduling;
            if (ImGui::DragInt("myScheduling", &sched, 1, 0, 16))
                SafeWriteI32(wrap, UIRootFields::kMyScheduling, sched);

            int target = s.myTargetType;
            if (ImGui::DragInt("myTargetType (0=?, 8=Player observed)", &target, 1, 0, 32))
                SafeWriteI32(wrap, UIRootFields::kMyTargetType, target);

            ImGui::Separator();

            // ── Game-state bitmask editor ──
            ImGui::Text("myGameStatesString (bitmask, 22 bits — when graph runs)");
            ImGui::SameLine();
            if (ImGui::SmallButton("All"))
                SafeWriteU32(wrap, UIRootFields::kMyGameStates, 0x3FFFFFu);
            ImGui::SameLine();
            if (ImGui::SmallButton("None"))
                SafeWriteU32(wrap, UIRootFields::kMyGameStates, 0);
            ImGui::SameLine();
            if (ImGui::SmallButton("Gameplay only (Normal)"))
                SafeWriteU32(wrap, UIRootFields::kMyGameStates, (1u << 17));

            std::uint32_t bits = s.myGameStatesString;
            constexpr int kCount = sizeof(kGameStateNames) / sizeof(kGameStateNames[0]);
            for (int i = 0; i < kCount; ++i)
            {
                bool on = (bits & (1u << i)) != 0;
                char label[80];
                std::snprintf(label, sizeof(label), "%02d  %s##gs%d", i, kGameStateNames[i], i);
                if (ImGui::Checkbox(label, &on))
                {
                    if (on) bits |=  (1u << i);
                    else    bits &= ~(1u << i);
                    SafeWriteU32(wrap, UIRootFields::kMyGameStates, bits);
                }
                // 2 columns visually: every other entry on the same line
                if ((i % 2) == 0 && i + 1 < kCount) ImGui::SameLine(220);
            }

            ImGui::Separator();

            // ── Read-only info ──
            ImGui::TextDisabled("Read-only:");
            ImGui::TextDisabled("  myFactions: count=%d cap=%d",
                                s.myFactionsCount, s.myFactionsCap);
        }

        // ─── Layer-4 probe: pool0 (wrapper+0x30 = live nodesById dict) ──────
        // Read-only diagnostic for now. Once the BVM-value handle for this
        // dict is correctly located (pool0 has the hashmap, but the BVM-value
        // header — refcount/type-byte — lives elsewhere), we'll wire in the
        // engine's BvmAPI primitives for typed read/write.
        if (ImGui::CollapsingHeader("Layer-4: Live node dict (pool0 probe)",
                                    ImGuiTreeNodeFlags_DefaultOpen))
        {
            struct Pool0Snap {
                std::uintptr_t pool0Addr;
                std::uintptr_t entryArrayPtr;
                std::int32_t   count;
                std::int32_t   capacity;
                // pool1 (wrap+0x38) — likely holds the named BVM dict.
                std::uintptr_t pool1Addr;
                std::uintptr_t pool1EntryPtr;
                std::int32_t   pool1Count;
                std::int32_t   pool1Cap;
                bool           valid;
            };

            auto ReadPool0 = [](std::uintptr_t wrap, Pool0Snap* out) -> bool
            {
                out->valid = false;
                __try
                {
                    std::uintptr_t p0 = *reinterpret_cast<std::uintptr_t*>(wrap + 0x30);
                    if (p0 < 0x10000000000ULL || p0 >= 0x2300000000000ULL) return false;
                    out->pool0Addr      = p0;
                    out->entryArrayPtr  = *reinterpret_cast<std::uintptr_t*>(p0 + 0x20);
                    out->count          = *reinterpret_cast<std::int32_t*>(p0 + 0x28);
                    out->capacity       = *reinterpret_cast<std::int32_t*>(p0 + 0x2C);

                    // pool1
                    std::uintptr_t p1 = *reinterpret_cast<std::uintptr_t*>(wrap + 0x38);
                    if (p1 >= 0x10000000000ULL && p1 < 0x2300000000000ULL)
                    {
                        out->pool1Addr     = p1;
                        out->pool1EntryPtr = *reinterpret_cast<std::uintptr_t*>(p1 + 0x20);
                        out->pool1Count    = *reinterpret_cast<std::int32_t*>(p1 + 0x28);
                        out->pool1Cap      = *reinterpret_cast<std::int32_t*>(p1 + 0x2C);
                    }
                    else
                    {
                        out->pool1Addr = 0;
                        out->pool1EntryPtr = 0;
                        out->pool1Count = 0;
                        out->pool1Cap = 0;
                    }

                    out->valid = true;
                    return true;
                }
                __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
            };

            Pool0Snap p{};
            if (!ReadPool0(wrap, &p))
            {
                ImGui::TextColored(ImVec4(1, 0.4f, 0.4f, 1),
                    "pool0 unreadable for this wrapper.");
            }
            else
            {
                ImGui::Text("pool0 (wrap+0x30) : 0x%p   data 0x%p   count %d / %d",
                            reinterpret_cast<void*>(p.pool0Addr),
                            reinterpret_cast<void*>(p.entryArrayPtr),
                            p.count, p.capacity);
                if (p.pool1Addr)
                    ImGui::Text("pool1 (wrap+0x38) : 0x%p   data 0x%p   count %d / %d",
                                reinterpret_cast<void*>(p.pool1Addr),
                                reinterpret_cast<void*>(p.pool1EntryPtr),
                                p.pool1Count, p.pool1Cap);
                else
                    ImGui::TextDisabled("pool1 (wrap+0x38) : null");

                ImGui::Spacing();
                static int s_scanWhich = 1;  // 0=pool0, 1=pool1 (default — likely named dict)
                ImGui::Text("Scan source:");
                ImGui::SameLine();
                ImGui::RadioButton("pool0##sw", &s_scanWhich, 0);
                ImGui::SameLine();
                ImGui::RadioButton("pool1##sw", &s_scanWhich, 1);
                ImGui::Spacing();

                // Pick which pool to scan
                std::uintptr_t scanBase = (s_scanWhich == 1 && p.pool1EntryPtr)
                                         ? p.pool1EntryPtr : p.entryArrayPtr;

                if (!LooksLikeHeapPtr(scanBase))
                {
                    ImGui::TextDisabled("Selected pool's entry pointer outside heap range, not reading.");
                }
                else
                {
                    // ─── String scanner (Layer-4 first diagnostic) ──────
                    // The BVM entries don't have a clean fixed stride — each
                    // entry contains variable-length inline strings/values.
                    // Brute-force: read the whole entry buffer and pull out
                    // every printable ASCII C-string of length >= 4. That
                    // surfaces every BVM key name, asset path, and inline
                    // string value the graph references.
                    //
                    // For 1166 entries this scans ~64–128 KB which is fine
                    // off the UI thread (we cache the result per-selection).
                    static int s_scannedFor = -1;
                    static int s_scannedWhich = -1;
                    static std::vector<std::string> s_strings;
                    static std::vector<std::uintptr_t> s_stringAddrs;
                    static char s_filter[64] = {};

                    if (s_scannedFor != m_selectedIndex || s_scannedWhich != s_scanWhich)
                    {
                        s_strings.clear();
                        s_stringAddrs.clear();

                        // Scan 64 KB of entry data. With 1166 entries × ~48-
                        // 96 B average each this covers most. Bail early if
                        // we hit unreadable memory.
                        constexpr int kScanSize = 64 * 1024;
                        std::vector<std::uint8_t> buf(kScanSize, 0);
                        int got = SafeReadBytes(scanBase, buf.data(), kScanSize);

                        // Sliding window: any run of >= 4 printable ASCII
                        // bytes ending in NUL or non-printable is one string.
                        int run = 0;
                        for (int j = 0; j < got; ++j)
                        {
                            std::uint8_t b = buf[j];
                            bool printable = (b >= 0x20 && b <= 0x7E);
                            if (printable)
                            {
                                ++run;
                            }
                            else
                            {
                                if (run >= 4)
                                {
                                    std::string s(
                                        reinterpret_cast<const char*>(&buf[j - run]),
                                        run);
                                    s_strings.push_back(std::move(s));
                                    s_stringAddrs.push_back(
                                        scanBase + (j - run));
                                }
                                run = 0;
                            }
                        }
                        s_scannedFor = m_selectedIndex;
                        s_scannedWhich = s_scanWhich;
                    }

                    ImGui::Text("String scan of pool%d entry buffer (first 64 KB) @ 0x%p:",
                                s_scanWhich,
                                reinterpret_cast<void*>(scanBase));
                    ImGui::Text("Found %zu printable strings.", s_strings.size());

                    ImGui::SetNextItemWidth(260);
                    ImGui::InputTextWithHint("##strfilter", "filter substring",
                                             s_filter, sizeof(s_filter));

                    // Lowercase filter
                    char filterLo[64] = {};
                    {
                        int n = 0;
                        for (; n < (int)sizeof(filterLo) - 1 && s_filter[n]; ++n)
                            filterLo[n] = (char)std::tolower((unsigned char)s_filter[n]);
                    }

                    ImGui::BeginChild("##strscan", ImVec2(0, 220), true);
                    int shown = 0;
                    for (size_t i = 0; i < s_strings.size() && shown < 400; ++i)
                    {
                        const std::string& str = s_strings[i];
                        if (filterLo[0])
                        {
                            // case-insensitive substring match
                            std::string lo;
                            lo.reserve(str.size());
                            for (char c : str)
                                lo.push_back((char)std::tolower((unsigned char)c));
                            if (lo.find(filterLo) == std::string::npos) continue;
                        }
                        ImGui::Text("0x%p  %s",
                                    reinterpret_cast<void*>(s_stringAddrs[i]),
                                    str.c_str());
                        ++shown;
                    }
                    if (shown >= 400)
                        ImGui::TextDisabled("(showing first 400 — refine filter)");
                    ImGui::EndChild();
                }
            }

            ImGui::Spacing();
            ImGui::TextDisabled(
                "pool0 holds packed numeric/value data (1166 entries, hash-indexed).\n"
                "pool1 likely holds the named BVM dict. If pool1 shows readable\n"
                "keys (type/pins/myColor/etc.) we have the offset anchor we need.");
        }
    }

    // ─── Load shipped graph (existing-but-unloaded .muigraph) ────────────────
    // Calls sub_1B9D950 with a path we construct, with the gate byte forced
    // open. Watch the manager count and the m_entries list for a new entry to
    // appear; if it does, the loader works and the same approach extends to
    // any shipped path.
    ImGui::Spacing();
    ImGui::Separator();
    if (ImGui::CollapsingHeader("Load shipped graph (debug + examples)",
                                ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::TextWrapped(
            "Targets are .muigraph files that ship in the asset bundles but "
            "aren't on the retail startup load list. Click Load to call "
            "sub_1B9D950 with the path (gate byte at [Client+0x90]+0x2B1 "
            "forced to 1 around the call). Then check whether the manager "
            "count grew and whether the path shows up in the list above.");
        ImGui::Spacing();

        // Build a quick set of currently-loaded paths so we can flag each
        // candidate as Loaded / Not loaded.
        auto isLoaded = [this](const char* path) -> bool {
            for (const auto& e : m_entries)
                if (!e.path.empty() && e.path == path) return true;
            return false;
        };

        const int nCand = (int)(sizeof(ShippedGraphLoader::kCandidates)
                              / sizeof(ShippedGraphLoader::kCandidates[0]));
        ImGui::BeginChild("##loadcands", ImVec2(0, 220), true);
        for (int i = 0; i < nCand; ++i)
        {
            const auto& c = ShippedGraphLoader::kCandidates[i];

            // Column 0: Load button (fixed width)
            char btn[64];
            std::snprintf(btn, sizeof(btn), "Load##sg%d", i);
            if (ImGui::Button(btn, ImVec2(70, 0)))
            {
                ShippedGraphLoader::Attempt(c.path);
                Refresh();
            }

            // Column 1: status (fixed offset)
            ImGui::SameLine(90);
            if (isLoaded(c.path))
                ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.5f, 1.0f), "loaded   ");
            else
                ImGui::TextDisabled("not loaded");

            // Column 2: note (fixed offset)
            ImGui::SameLine(190);
            ImGui::TextUnformatted(c.note);

            // Column 3: path (fixed offset, wraps)
            ImGui::SameLine(390);
            ImGui::TextUnformatted(c.path);
        }
        ImGui::EndChild();

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Text("Last attempt:");
        if (!ShippedGraphLoader::g_last.path)
        {
            ImGui::TextDisabled("  (no attempt yet)");
        }
        else
        {
            const auto& a = ShippedGraphLoader::g_last;
            ImGui::TextWrapped("  path     : %s", a.path);
            ImGui::Text("  mgr count: %d -> %d   (delta %+d)",
                        a.mgrCountBefore, a.mgrCountAfter,
                        a.mgrCountAfter - a.mgrCountBefore);
            if (a.gateResolved)
                ImGui::Text("  gate byte: was 0x%02X (forced to 1 during call)",
                            a.gateBefore);
            else
                ImGui::TextColored(ImVec4(1, 0.6f, 0.4f, 1),
                            "  gate byte: NOT resolved (Client not ready)");

            ImVec4 ok(0.6f, 1.0f, 0.6f, 1), bad(1, 0.4f, 0.4f, 1);
            ImGui::TextColored(a.buildOk ? ok : bad,
                "  build    : %s (sub_1AE39F0)",
                a.buildOk ? "returned without SEH" : "raised SEH");
            ImGui::Text("  wrapper  : 0x%p   %s",
                        reinterpret_cast<void*>(a.wrapperAddr),
                        a.wrapperNonNull ? "(allocated)" : "(NULL — alloc failed)");
            if (a.wrapperNonNull)
            {
                ImGui::TextColored(a.activateOk ? ok : bad,
                    "  activate : %s (refcount after dec = %d)",
                    a.activateOk ? "returned without SEH" : "raised SEH",
                    a.refcountAfterDec);
                ImGui::Text("  fired    : %s",
                    a.activated
                      ? "YES (refcount hit 0 — slot 0 invoked)"
                      : "no  (refcount > 0 — scheduler will fire async)");
            }
        }

        ImGui::Spacing();
        ImGui::TextDisabled(
            "If mgr count doesn't grow immediately and 'fired = no', the\n"
            "scheduler should pick the job up within a frame or two — try\n"
            "Refresh after ~1 second. If 'fired = YES' and count is still\n"
            "flat, the activation reached but didn't add to manager — that\n"
            "means asset bytes weren't found in any mounted bundle.");
    }
}
