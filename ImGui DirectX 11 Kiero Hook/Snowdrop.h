#pragma once
#include <d3d11.h>
#include <DirectXMath.h>
#include <dxgi.h>
#include <Windows.h>
#include <stdio.h>
#include <iostream>
#include <iomanip>
#include "Util.h"

using namespace DirectX;

extern uintptr_t g_pBase;

namespace TD
{
    class Agent;
    class AgentInfo;
    class AppearanceManager;
    class CameraManager;
    class Client;
    class EnvironmentManager;
    class GameCamera;
    class GameRenderer;
    class MouseInput;
    class RogueClient;
    class World;

    // 16-byte string type used pervasively in Snowdrop.
    // +0x00 QWORD heap cstring ptr (capacity at ptr-4 DWORD); +0x0F BYTE = 1 if heap, 0 if inline.
    struct SnowdropString
    {
        BYTE bytes[0x10];
    };

    // 0x40-byte asset reference struct (init by sub_4EA830). Holds two SnowdropStrings + hash header.
    struct AgentAssetRef
    {
        void*          m_pTableA;       // 0x00
        void*          m_pTableB;       // 0x08
        SnowdropString m_StringA;       // 0x10
        int32_t        m_IntA;          // 0x20
        BYTE           Pad24[0x4];      // 0x24
        SnowdropString m_StringB;       // 0x28
        int32_t        m_IntB;          // 0x38
        uint16_t       m_Word;          // 0x3C
        BYTE           Pad3E[0x2];      // 0x3E
    }; // sizeof = 0x40

    // Agent — total size ~0xB58. Verified against ctor sub_152E470 / dtor sub_153BCE0.
    // Most fields are engine-internal (anim tracks, mutex slots, intrusive list heads,
    // init counters). Only the named fields below are useful for modding/debug; the
    // labeled padding ranges describe what's in those zones if you ever need to dig.
    class Agent
    {
    public:
        // ─── header ──────────────────────────────────────────────────────
        void*              vtable;              // 0x000  &unk_30BB618
        BYTE               Pad008[0x28 - 0x08]; // 0x008  packed flags + small counters
        AgentInfo*         m_Info;              // 0x028
        BYTE               Pad030[0x8];         // 0x030  zero
        World*             m_pWorld;            // 0x038
        XMMATRIX           m_Transform;         // 0x040  rotation 3x3 + translation row at +0x70

        // ─── 0x080..0x1BF: alt-position / aim block, anim-track ptrs, small floats.
        //   m_Position2 (mirror of translation row) is at +0x80; misc anim-system pointers
        //   continue to ~0x1B0. Nothing useful exposed.
        BYTE               Pad080[0x1C0 - 0x80]; // 0x080

        // ─── alive / skinned mesh / tag ──────────────────────────────────
        uint8_t            m_IsDead;            // 0x1C0
        BYTE               Pad1C1[0xF];         // 0x1C1
        void*              m_pSkinnedMesh;      // 0x1D0  bone root for HeadManager
        char               m_EntityTag[8];      // 0x1D8  inline ASCII (e.g. "soldier\0")

        // ─── 0x1E0..0x254: bit flags + intrusive pool list head — no use. ─
        BYTE               Pad1E0[0x255 - 0x1E0]; // 0x1E0

        uint8_t            m_DZFlagFallback;    // 0x255  sub_D15C40 fallback path
        BYTE               Pad256[1];           // 0x256
        uint8_t            m_DZFlagFallback2;   // 0x257  sub_EF8750 reads here

        // ─── 0x258..0x3A3: pool list head, init counter (0x3A0 ctor=1), .data ptr,
        //   small floats (0x340 int=500 max-like, 0x348 = 1.0). Nothing actionable. ─
        BYTE               Pad258[0x3A4 - 0x258]; // 0x258

        int32_t            m_EntityType;        // 0x3A4  1=player, 7=NPC
        uint32_t           m_EntityId1;         // 0x3A8
        uint32_t           m_EntityId2;         // 0x3AC

        // ─── 0x3B0..0x417: includes 4 ints at 0x19C..0x1A8 region copied from
        //   the spawn descriptor at ctor time (likely faction/team/loadout indices).
        //   Wait — those are at 0x19C, much earlier. This range is mostly unused. ─
        BYTE               Pad3B0[0x418 - 0x3B0]; // 0x3B0

        uint64_t           m_CharGuidHash[2];   // 0x418  16-byte signature
        SnowdropString     m_CharGuidString;    // 0x428  "$<uuid>" per-character ID

        BYTE               Pad438[0x4B0 - 0x438]; // 0x438
        int32_t            m_ComponentDescCount; // 0x4B0  count for m_pComponentSet
        BYTE               Pad4B4[0x4];         // 0x4B4
        void*              m_pComponentSet;     // 0x4B8  built by sub_1B07A60 (component-system root)

        // ─── 0x4C0..0x6DF: anim-track ptr arrays, 2 small managed objects
        //   (16B at +0x698 vtable &unk_30D6058; 208B at +0x6B0). ──────────
        BYTE               Pad4C0[0x6E0 - 0x4C0]; // 0x4C0

        // ─── embedded sub-object (multi-inheritance) ─────────────────────
        void*              vtable2;             // 0x6E0  &unk_30BB9A8
        void**             m_ComponentSlots;    // 0x6E8  80-byte polymorphic slots,
                                                //   count at slots[-16]; dtor calls slot.vtable[7]

        // ─── 0x6F0..0x75B: cached state, alt-position {x,y,z} at 0x708,
        //   floats at 0x720 (75.0) / 0x728 / 0x730, SnowdropString at 0x768,
        //   intrusive list at 0x788 with inline buffer at 0x7A0. Engine-internal. ─
        BYTE               Pad6F0[0x75C - 0x6F0]; // 0x6F0

        uint8_t            m_IsRogue;           // 0x75C
        BYTE               Pad75D[0x7C8 - 0x75D]; // 0x75D  4 managed object slots
        AppearanceManager* m_pAppearance;       // 0x7C8  ★ clothing manager (sub_1534EB0)

        // ─── 0x7D0..0x95F: PlatformMutex, 2× AgentAssetRef (0x40 bytes each
        //   at 0x810/0x850 — name+path SnowdropStrings), small init fields,
        //   3× sub_1018150 structs (0x28 bytes each, animation/scalar tracks). ─
        BYTE               Pad7D0[0x960 - 0x7D0]; // 0x7D0

        // ─── scale block: each scale value followed by 16 bytes of paired track data ─
        float              m_Scale_X;           // 0x960  ★ default 1.0
        BYTE               Pad964[0x10];        // 0x964
        float              m_Scale_Y;           // 0x974  ★ default 1.0
        BYTE               Pad978[0x10];        // 0x978
        float              m_Scale_Z;           // 0x988  ★ default 1.0

        // ─── 0x98C..0xB57: remainder. Includes m_ComponentArrayPtr/Count at
        //   0x9D0, m_pInterface1/2 at 0xA38/0xA88, hash-tree at 0xAB8, two
        //   sub_10181B0 structs (3000.0 / 750), trailing SnowdropString at 0xB18. ─
        BYTE               Pad98C[0xB58 - 0x98C]; // 0x98C

    public:
        bool IsPlayer()
        {
            int type = *(int*)((__int64)this + 0x3A4);
            return (type == 1 || type == 7);
        }

        bool IsInDarkZone()
        {
            typedef bool(__fastcall* tAgentIsInDarkZone)(Agent*);
            tAgentIsInDarkZone AgentIsInDarkZone = (tAgentIsInDarkZone)(g_pBase + 0xD15C40); // 48 8B 81 ? ? ? ? 48 85 C0 74 ? 48 8B 51
            return AgentIsInDarkZone(this);
        }

        bool IsRogue() const
        {
            return *reinterpret_cast<const BYTE*>(reinterpret_cast<const char*>(this) + 0x75C) != 0;
        }

        bool IsDead() const
        {
            return *reinterpret_cast<const BYTE*>(  reinterpret_cast<const char*>(this) + 0x1C0) != 0;
        }

        XMMATRIX GetHeadBoneMatrix()
        {
            __int64 bone_root = *(__int64*)((__int64)this + 0x1D0);
            if (!bone_root)
            {
                std::cout << "bone_root not found!\n";
                return XMMatrixIdentity();
            }

            // generic bone array start
            __int64 boneArray = *(__int64*)(bone_root + 0x1460);
            if (!boneArray)
            {
                std::cout << "Bone array not found!\n";
                return XMMatrixIdentity();
            }

            constexpr UINT HEAD_BONE_INDEX = 5;

            __int64 headBoneMatrixAddr = boneArray + (HEAD_BONE_INDEX * 0x40);

            return *reinterpret_cast<XMMATRIX*>(headBoneMatrixAddr);
        }

    };

    // sizeof = 0x60. Lives in a packed pool (each entry 96 bytes).
    // Verified via ctor sub_5D3220 / dtor sub_5F97A0:
    //   ctor(this, a2 [16-byte key], a3 [descriptor], a4 [manager refcounted obj])
    //   dtor: refcount-dec m_pManager; (*m_pSubVtbl)(this+0x20); destroy m_Name; base dtor.
    class AgentInfo
    {
    public:
        void*           vtable;          // 0x00  &unk_2A5E360 (final). Ctor briefly assigns parent
                                         //   vtable &unk_292D1D0 first, then overrides — has a base class.
        uint32_t        m_Id;            // 0x08  numeric id (ctor inits 0; live: 0x2B0)
        uint8_t         m_TypeByte;      // 0x0C  ctor inits 1 (single byte; small enum / role flag)
        BYTE            Pad0D[3];        // 0x0D  pad to 8-align m_Name
        SnowdropString  m_Name;          // 0x10  ★ Snowdrop string — inline ("ssh_.") or heap (longer).
                                         //   Was previously typed as char[16]; wrong for names >15 chars
                                         //   (heap-flag byte at +0x0F switches between inline/heap).
        // Generic 24-byte typed-value slot. Operations dispatched via m_pSubVtbl below
        // (type-erasure, not polymorphism). The first DWORD often carries a type-tag/sentinel
        // value (live: 0xFFFFFFFE). Treat the rest as opaque payload bytes — the layout is
        // determined by whichever operator-table m_pSubVtbl points at.
        BYTE            m_TypedValue[0x18]; // 0x20  24 bytes
        void*           m_pSubVtbl;      // 0x38  Type-erasure operator table (NOT a class vtable).
                                         //   8 function-ptr slots in .data, e.g. off_42BC730:
                                         //     [0] dtor    [1] copy    [2] assign   [3] equals
                                         //     [4-7] hash / less / serialize / type-id (static-init)
                                         //   Ctor copies this from descriptor a3+24; different
                                         //   AgentInfo categories may use different operator tables
                                         //   for different value types stored in m_TypedValue.
        uint64_t        m_Key[2];        // 0x40  16-byte key/signature copied from ctor's a2 arg.
                                         //   Was labeled m_NameHash — actual purpose unconfirmed.
        Agent*          m_pAgent;        // 0x50  back-reference (ctor inits null; assigned later)
        void*           m_pManager;      // 0x58  ★ AgentInfoPool — refcounted owner that allocated this entry.
                                         //   Custom slab allocator (sub_60E440): 192 KB chunks split into
                                         //   2047 × 0x60 slots. Fibonacci-hashed registry (sub_6510A0)
                                         //   maps m_Key[16 bytes] → AgentInfo*. Pool internals:
                                         //     +0x10 hashmap header / lock
                                         //     +0x18 hashmap state (buckets, count, capacity)
                                         //     +0x28 freelist head
                                         //     +0x40 total-alloc counter
                                         //   Walk this to enumerate every AgentInfo in the game.
    }; // sizeof = 0x60

    // AppearanceManager — total 0x990 bytes (2448). One per Character (player + each NPC).
    // Reachable from Agent::m_pAppearance.
    //
    // Verified via ctor sub_1534EB0, init sub_15A39F0, dtor sub_153D7C0,
    // and call sites sub_16679B0 (ApplyClothingId), sub_162DD60 (SetClothingIdList),
    // sub_162FDA0 (model-load trigger).
    //
    // What lives in here, in plain terms:
    //   • A 27-entry clothing slot table (m_Clothes), keyed by clothing_id (0..26).
    //     Each slot holds the loaded model's cached path; mutating the path triggers
    //     a different .mgraphobject to be loaded for that slot on next refresh.
    //   • A mirror table (m_Clothes2) for the alternate viewpoint set
    //     (3rd-person vs 1st-person; selected by m_Category).
    //   • An attachment hashmap (+0x18) keyed by the model PATH SnowdropString.
    //     Each bucket also carries an attachment slot name ("Holster", "Mask", etc.)
    //     and a small-int "category" (clothing_id reference, e.g. 5=Holster, 27=accessory).
    //   • Two arrays of texture-override paths (m_PathStrings1, m_PathStrings2).
    //     Live observation on the player: PathStrings1 = 6 .dds paths for the arm patch
    //     (3 base + 3 override: diffuse / normal / mask).
    //   • A clothing-id list (+0x950 / m_ClothingIdList) — the input list driven
    //     by Character_SetClothingIdList; SBO-style with inline buffer at +0x964.
    //   • Two dirty flags (+0x4E0 m_DirtyFlag, +0x4E1 m_ListUpdated) toggled when
    //     clothing changes; a "synced" gate at +0x948 (sync triggers if 0).
    //
    // Engine functions of interest (all g_pBase + offset):
    //   0x1534EB0  ctor(this, category, owner)
    //   0x153D7C0  dtor
    //   0x15A39F0  full clothing init — populates clothing IDs 1, 3, 8, 10, 11 from a
    //              global default-clothing descriptor; skips slots with m_InitFlag==1.
    //   0x16679B0  Character_ApplyClothingId(this, &id) — assigns slot[id].m_Path to the
    //              cached path for that id, and removes any matching attachment buckets
    //              (those whose +60 field equals id).
    //   0x162DD60  Character_SetClothingIdList(this, list)  ★ clean public API.
    //              list = {ptr→int[], count}. Applies each id in turn, copies the
    //              new list into m_ClothingIdList (+0x950), sets m_DirtyFlag and
    //              m_ListUpdated. Auto-syncs if +0x948 is 0.
    //   0x162FDA0  model_load_trigger(this, &snowdropstr_path, &id) — ensures an
    //              attachment bucket exists for (path, id); inserts via sub_1544E60.
    //   0x1542C60  triple-string finalize/register (used for model paths when empty).
    //   0x1543F20  hashmap_lookup(hashmap_state*, uint32_t* key) → SnowdropString*
    //              ★ DO NOT call against m_ClothingHashmap (+0x28) — that hashmap
    //              has NULL entries on this build; the resize path scribbles memory.
    //   0x116830   string_assign(SnowdropString* dst, const char* src)
    //
    // For in-place skin swap on the player only, mutate m_Clothes[id].m_Path bytes
    // directly via VirtualProtect+memcpy. See clothing_swap memory for the recipe.
    class AppearanceManager
    {
    public:
        // ── Per-clothing-slot record (40 bytes). Slot N at m_Clothes[N]. ──────
        // Slots populated for clothing_ids 0, 1, 3, 8, 10, 11 on the player by the
        // default-clothing init (sub_15A39F0). Other indices may be empty.
        struct ClothingSlot
        {
            void*           m_pSlot;       // 0x00  per-slot loaded asset ptr (set on init)
            SnowdropString  m_Path;        // 0x08  cached model path — ★ mutate this for skin swap
            uint8_t         m_InitFlag;    // 0x18  1 = initialized (sub_15A39F0 skips when set)
            uint8_t         m_StateByte;   // 0x19  paired init/state flag (e.g. "applied")
            BYTE            Pad1A[6];      // 0x1A
            void*           m_Aux;         // 0x20  saved descriptor handle (v52 in sub_15A39F0)
        }; // sizeof = 0x28

        // ── Attachment hashmap bucket (64 bytes). Key = model path (the SnowdropString).
        // Lookup-by-path returns one of these; the inline name and clothing_id grouping
        // let Character_ApplyClothingId remove all attachments for a specific clothing_id.
        struct AttachBucket
        {
            SnowdropString  m_ModelPath;       // 0x00  e.g. "rogue/graph objects/gear/bp_hazmat_bag.mgraphobject"
            void*           m_SBO_DataPtr;     // 0x10  inline buffer ptr (= this+0x28) or heap if grown
            int32_t         m_SBO_Count;       // 0x18  attachment-list count for this bucket
            int32_t         m_SBO_CapFlags;    // 0x1C  capacity | 0x80000000 inline-mode bit
            int32_t         m_Initialized;     // 0x20  1 once filled
            int32_t         m_HeapTag;         // 0x24  often 0x250 (heap-arena id)
            char            m_SlotName[16];    // 0x28  inline ASCII (e.g. "Holster", "Mask")
            int32_t         m_Reserved38;      // 0x38  usually 0
            int32_t         m_ClothingId;      // 0x3C  Character_ApplyClothingId removes by this:
                                               //   1=Top, 2=Mask, 3=Pants/Glove, 5=Holster,
                                               //   ... 27=accessory (no associated clothing slot)
        }; // sizeof = 0x40

        void*               vtable;                     // 0x000  &unk_30BB5F8
        int32_t             field_8;                    // 0x008  small int (init 0)
        uint8_t             m_Category;                 // 0x00C  ctor arg2 — selects which descriptor
                                                        //   set sub_15A39F0 reads. Live=1 in 3rd-person.
        BYTE                Pad00D[3];                  // 0x00D
        void*               m_pOwner;                   // 0x010  ctor arg3 — parent Character/owner

        // 64-byte-bucket hashmap: model path → AttachBucket (slot name + clothing_id).
        // Live on player: 17 entries / 31 capacity (Holster, Mask, Glove, KA-BAR_Knife,
        // bp_hazmat_bag, CA_SHD_Watch, Backpack_Base, …).
        AttachBucket*       m_AttachHashmap_Buckets;    // 0x018
        int32_t             m_AttachHashmap_Count;      // 0x020
        int32_t             m_AttachHashmap_Cap;        // 0x024

        // 24-byte-bucket hashmap: clothing_id → cached SnowdropString.
        // ★ NULL on most live instances on this build — sub_1543F20 against this crashes.
        void*               m_ClothingHashmap_Buckets;  // 0x028
        int32_t             m_ClothingHashmap_Count;    // 0x030
        int32_t             m_ClothingHashmap_Cap;      // 0x034

        // 27-slot clothing array, slot N at +0x38 + N*0x28. Indexed by clothing_id (0..26).
        ClothingSlot        m_Clothes[27];              // 0x038..0x46F

        // Heap-array of (ptr, count, cap) populated at character load — content unverified
        // (live on player: ptr → some ~3-entry table). Stays stable across clothing changes.
        void*               m_TableA_Ptr;               // 0x470
        int32_t             m_TableA_Count;             // 0x478  (live: 3)
        int32_t             m_TableA_Cap;               // 0x47C  (live: 3)

        // Texture-override path strings. Live on player: arm-patch textures —
        //   [0] ca_shd_armpatch_d.dds   (diffuse, base)
        //   [1] ca_shd_armpatch_n.dds   (normal,  base)
        //   [2] CA_SHD_Armpatch_Empty_M.dds (mask, "empty" placeholder)
        //   [3] CA_SHD_Armpatch_D.dds   (diffuse, override)
        //   [4] ca_shd_armpatch_n.dds   (normal,  override)
        //   [5] CA_SHD_Armpatch_M.dds   (mask,    override)
        // sub_15A39F0 calls sub_1542C60 to finalize/register these if any are empty.
        SnowdropString      m_PathStrings1[6];          // 0x480..0x4DF

        // ── Three change-tracking flags. Verified by toggling clothing in-game:
        uint8_t             m_DirtyFlag;                // 0x4E0  set by SetClothingIdList; auto-clears
                                                        //   when consumed — observed back at 0 by the
                                                        //   time the change had visibly applied.
        uint8_t             m_ListUpdated;              // 0x4E1  set to 1 on clothing change; ★ stable
                                                        //   "changes pending" indicator (didn't auto-clear).
        uint8_t             m_NeedsResync;              // 0x4E2  set to 1 on clothing change; likely
                                                        //   "needs visual resync" — pairs with m_ListUpdated.
        BYTE                Pad4E3[5];                  // 0x4E3

        // ── Pending-changes attachment list — populated during a clothing-update pass.
        // Points to a heap array of AttachBucket-shaped 64-byte records (verified live:
        // first record had inline name "bp_hazmat_bag"). Use as a hook to inspect what
        // the engine is about to apply. Null between updates.
        AttachBucket*       m_PendingAttach;            // 0x4E8
        void*               field_4F0;                  // 0x4F0  null between updates

        // 0x4F8..0x92F: a second 27-slot ClothingSlot array (mirrors m_Clothes layout).
        // Stays empty on the player across normal play and clothing changes; purpose
        // unverified. Padded out rather than exposed — un-pad to ClothingSlot[27] if
        // a future feature ever needs to inspect it.
        BYTE                Pad4F8[0x930 - 0x4F8];      // 0x4F8

        // ── Three back-to-back (ptr, count, cap) heap-array trios at +0x930..+0x95F.
        // All three populate during a clothing-update pass; trio C (the clothing-id list)
        // is only populated when SetClothingIdList is called — UI-driven changes use
        // ApplyClothingId per slot and leave it empty.

        // Trio A: heap array of 32-byte entries — each entry = (SnowdropString,
        // heap-array-of-24-byte-items-each-containing-a-SnowdropString). Material/variant
        // lists per applied attachment. Live after clothing change: 3 entries.
        void*               m_DynArrayA_Ptr;            // 0x930
        int32_t             m_DynArrayA_Count;          // 0x938
        int32_t             m_DynArrayA_Cap;            // 0x93C

        // Trio B: vector<AssetRecord*> — pointers to 704-byte character-asset records
        // (vtable g_pBase + 0x2CB9E38, allocated by sub_EB2F20). Each AssetRecord embeds
        // the same qword_42ADF70/78 type-erasure operator table at +0x2B0/+0x2B8 that
        // AgentInfo's typed-value slot uses, plus an int at +168 (asset-type enum, =5).
        // Live observation: empty before clothing change; count=1 after — one new asset
        // record was pushed when the new clothing piece's model/materials were bound.
        // Dtor frees the heap with no per-element cleanup → element type is POD (8-byte
        // pointer). Use this list to enumerate which loaded model+material records
        // belong to this AppearanceManager.
        void**              m_AssetRecords_Ptr;         // 0x940  vector<AssetRecord*>
        int32_t             m_AssetRecords_Count;       // 0x948
        int32_t             m_AssetRecords_Cap;         // 0x94C

        // Trio C: SBO-style clothing-id list. SetClothingIdList copies the input list
        // here and applies each id via ApplyClothingId. Stays empty when the change
        // was driven via per-slot ApplyClothingId rather than the list API.
        int32_t*            m_ClothingIdList_Ptr;       // 0x950  (inline buffer at +0x964 if small)
        int32_t             m_ClothingIdList_Count;     // 0x958
        int32_t             m_ClothingIdList_CapFlags;  // 0x95C  high bit = inline mode

        // Three more SnowdropStrings — populated during a clothing-update pass with the
        // base texture set (live: matches m_PathStrings1[0..2] — diffuse/normal/empty-mask
        // for the arm patch). Possibly the "incoming" texture set staged for swap.
        SnowdropString      m_PathStrings2[3];          // 0x960..0x98F
    }; // sizeof = 0x990

    class CameraManager
    {
    public:
        BYTE Pad000[0x18];
        GameCamera* m_pCamera1;
        GameCamera* m_pCamera2;
    };

    // Client is the online/session manager. Most of its fields are session-internal
    // subsystems (network, world streaming, asset cache, listeners) — only the slots
    // exposed below have proven useful for this mod.
    enum class ClientWorldState : int32_t
    {
        Idle                                       = 0,
        ClearingWorld                              = 1,
        PartiallyClearingWorldOnServerSwitch       = 2,
        ClearingWorldAfterDifferentMapServerSwitch = 3,
        WaitingForLoadMapRequest                   = 4,
        WaitingForLoadMapRequestAfterServerSwitch  = 5,
        Game_StartLoading                          = 6,
        Game_Loading                               = 7,
        Game_WaitingForTasks                       = 8,
        Frontend_StartLoading                      = 9,
        Frontend_Loading                           = 10,
        Frontend_WaitingForTasks                   = 11,
        SameMapServerSwitch_CreatingPlayer         = 12,
    };

    class Client
    {
    public:
        void*            vtable;                  // 0x000
        BYTE             Pad008[0x28 - 0x08];     // 0x008  internal subsystem ptrs / counters
        World*           m_pWorld;                // 0x028
        MouseInput*      m_pMouseInput;           // 0x030  also holds cursor-vis flags at +0x270 / +0x288
        void*            m_pVarSystem;            // 0x038  pValueStoreThingy — KB_SHOW_MOUSE et al.
        BYTE             Pad040[0x60 - 0x40];     // 0x040  asset/HUD cache + listeners (not exposed)
        void*            m_pGameplayStatsMgr;     // 0x060  GameplayStatisticsManager (RTTI-confirmed)
        BYTE             Pad068[0x248 - 0x68];    // 0x068  many subsystems incl. ~250 KB streaming DB at +0x68
        ClientWorldState m_WorldStreamState;      // 0x248  ctor=ClearingWorld(1). Mostly parks at 1 in-world
                                                  //   and 2 at menu; transition states flicker too fast to
                                                  //   gate on. Useful for watching engine transitions.
                                                  // …session/transition fields continue past 0x248 to at least 0x2F0; not exposed.
    };

    class HudSettings
    {
    public:
        BYTE Pad000[0x20];
        float m_CloseUpEffectsDistance; // 0x20
        float m_CloseUpEffectsFadeInDistance; // 0x24
        float m_unk1;
        float m_DOFFStop; // 0x2C
        float m_DOFLerpSpeed; //0x30
        float m_MinCoC; // 0x34
        float m_MaxCoC; // 0x38
        BYTE Pad03C[0x24];
        float m_FarDistance; // 0x60
        float m_FarFadeInDistance; // 0x64
        int m_Timer;
    };

    class EnvironmentFileSystem
    {
        struct EntityHandle
        {
            BYTE GUID[0x10];
            __int64 pEntity;
        };

    public:
        BYTE Pad000[0x8];
        EntityHandle* m_pHandles;
        int m_handleCount;

    public:
        static EnvironmentFileSystem* Singleton()
        {
            return *(EnvironmentFileSystem**)(g_pBase + 0x4601618); // 48 89 05 ? ? ? ? 48 8B 44 24 ? ? ? 48 8B C3 48 8B 9C 24
        }

        __int64 GetEnvByName(const char* name)
        {
            for (int i = 0; i < m_handleCount; ++i)
            {
                const char* pFilename = *(const char**)(m_pHandles[i].pEntity + 0x10);
                if (strcmp(pFilename, name) == 0)
                    return m_pHandles[i].pEntity;
            }
            return 0;
        }
    };

    class EnvironmentManager
    {
    public:
        class EnvironmentValues
        {
        public:
            BYTE Pad000[0x60];
            float m_BlendValue;
        };

    public:
        virtual void Func1();
        virtual void Func2();
        virtual void SetTimeOfDay(int TimeOfDay, bool FreezeTimer);
        virtual void Func4();
        virtual void Func5();
        virtual void Func6();
        virtual void Func7();
        virtual void SetWeather(__int64 pWeather);

        BYTE Pad008[0x8];
        int m_TimeOfDay;
        bool m_FreezeToD;
        BYTE Pad15[0xB];
        __int64 m_pCurrentWeather;
        __int64 m_pNextWeather;
        int m_WeatherTimer;
        int m_WeatherTimerMax;
        BYTE Pad038[0x8];
        bool m_RunWeatherTimer;
        BYTE Pad41[0x13F];
        EnvironmentValues* m_pEnvironmentValues;

    public:
        typedef __int64(__fastcall* tCopyEnvironmentValues)(__int64 a1, __int64 a2, __int64 a3, int a4);
        void SetCurrentWeather(__int64 pWeatherEntity)
        {

            this->m_WeatherTimer = 0;
            this->m_RunWeatherTimer = 0;
            tCopyEnvironmentValues CopyEnvironmentValues = (tCopyEnvironmentValues)(g_pBase + 0x1A14870); // 48 89 5C 24 ? 48 89 6C 24 ? 48 89 74 24 ? 57 48 83 EC ? 80 7C 24 ? 00

            this->m_pCurrentWeather = pWeatherEntity;
            __int64 pNextWeatherBlender = *(__int64*)((__int64)this + 0x168);
            __int64 pFactoryThing = *(__int64*)((__int64)this + 0x188);

            CopyEnvironmentValues(pFactoryThing, pNextWeatherBlender, pWeatherEntity, -1);
        }

        void SetNextWeather(__int64 pWeatherEntity)
        {
            tCopyEnvironmentValues CopyEnvironmentValues = (tCopyEnvironmentValues)(g_pBase + 0x1A14870); // 48 89 5C 24 ? 48 89 6C 24 ? 48 89 74 24 ? 57 48 83 EC ? 80 7C 24 ? 00

            this->m_pNextWeather = pWeatherEntity;
            __int64 pNextWeatherBlender = *(__int64*)((__int64)this + 0x170);
            __int64 pFactoryThing = *(__int64*)((__int64)this + 0x188);

            CopyEnvironmentValues(pFactoryThing, pNextWeatherBlender, pWeatherEntity, -1);
        }


    }; // Size: 0x208

    class GameCamera
    {
    public:
        BYTE Pad000[0x10];
        XMMATRIX m_Transform;
        XMMATRIX m_ViewProjection;
        float m_FieldOfView;
        BYTE Pad0A4[0x3C];
    };

    class GameRenderer
    {
    public:
        BYTE Pad000[0x20];
        int m_Width; // 0x20
        int m_Height; // 0x24
        BYTE Pad028[0x28];
        IDXGISwapChain* m_pSwapChain; // 0x50

    public:
        static GameRenderer* Singleton()
        {
            __int64 ptr1 = *(__int64*)(g_pBase + 0x44DD210); // 48 89 1D ? ? ? ? 66 C7 83
            return *(GameRenderer**)(ptr1 + 0x1E8);
        }

        static ID3D11Device* GetDevice()
        {
            return *(ID3D11Device**)(g_pBase + 0x44DD230); // 48 89 3D ? ? ? ? 48 85 C9 74 ? ? ? ? FF 50 ? 48 8B 0D ? ? ? ? 48 89 3D ? ? ? ? 48 85 C9 74 ? ? ? ? FF 50
        }
    };

    class MouseInput
    {
    public:
        BYTE Pad000[0x38];
        int m_dX;
        int m_dY;
        int m_dZ;
    };

    class RogueClient
    {
    public:
        BYTE Pad000[0x120];
        Client* m_pClient; // 0x120
        BYTE Pad128[0x60C];
        bool m_isMouseHovering; // 0x734
        BYTE Pad735[0x13];
        bool m_isWindowFocused; // 0x748

    public:
        static RogueClient* Singleton()
        {
            return *(RogueClient**)(g_pBase + 0x4688B28); // 48 8B 0D ? ? ? ? 48 85 C9 74 ? 48 8B 89 ? ? ? ? 48 85 C9 74 ? 48 8B 49 ? EB ? 48 8B CD 48 85 C9 74
        }
    };

    class TimeModule
    {
    public:
        double* m_pLastDeltas;
        double m_TotalTimePassed;
        int m_DeltaTime;
        BYTE Pad014[0xC];
        bool m_FreezeTime;
        BYTE Pad021[0x3];
        float m_TimeScale;

    public:
        static TimeModule* Singleton()
        {
            return *(TimeModule**)(g_pBase + 0x42ADDC8); // 48 83 3D ? ? ? ? 00 75 ? B9 ? ? ? ? E8 ? ? ? ? 48 85 C0 74 ? 48 8B C8 E8 ? ? ? ? 48 89 05 ? ? ? ? EB ? 48 C7 05 ? ? ? ? 00 00 00 00 FF 0D
        }
    };

    class World
    {
    public:
        BYTE Pad000[0x2B0];
        __int64 m_pInput; // 0x2B0
        BYTE Pad02B8[0x10];
        HudSettings* m_pDoF; // 0x2C8
        CameraManager* m_pCameraManager; // 0x2D0
        BYTE Pad2D8[0xE8];
        EnvironmentManager* m_pEnvironmentManager; // 0x3C0
        BYTE Pad3C8[0x68];
        Agent** m_AgentArray;
        int m_AgentCount;
    }; // Size: 0x448

    static void ShowMouse(bool arg)
    {
        typedef __int64* (__fastcall* tGetValue)(__int64, __int64*, const char*, int);
        tGetValue GetValue = (tGetValue)(g_pBase + 0x646DF0); // B9 ? ? ? ? 48 89 5C 24 ? E8 ? ? ? ? 48 8B D8 48 85 C0 74 ? 48 8B CF
        TD::Client* pClient = TD::RogueClient::Singleton()->m_pClient;
        __int64 pValueStoreThingy = *(__int64*)((__int64)pClient + 0x38);
        __int64 donotcare = 0;
        __int64 pValueStore = *GetValue(pValueStoreThingy, &donotcare, "KB_SHOW_MOUSE", 0);

        if (pValueStore)
        {
            bool* pValue = (bool*)(pValueStore + 0xA5);
            bool* pValue2 = (bool*)(pValueStore + 0xA6);
            bool* pValue3 = (bool*)(pValueStore + 0xA7);
            *pValue = arg;
            *pValue2 = arg;
            *pValue3 = arg;
        }

        __int64 someOtherThing = *(__int64*)((__int64)pClient + 0x30);
        *(int*)(someOtherThing + 0x270) = arg;
        if (arg && *(int*)(someOtherThing + 0x288) == 0)
            *(int*)(someOtherThing + 0x288) = 0xC;
    }
}