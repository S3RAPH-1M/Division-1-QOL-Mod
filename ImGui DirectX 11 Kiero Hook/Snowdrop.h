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
    class InventoryConfig;
    class InventoryHashmap;
    class ItemDescriptor;
    class MouseInput;
    class PlayerInventory;
    class PlayerSessionState;
    class RogueClient;
    class SkinItemDescriptor;
    class World;
    class WorldConfigContainer;
    struct OwnedItemCountRecord;

    // RShared_Handle<RShared_Item> — 8-byte item handle used everywhere on the engine
    // RPC surface (RClient_Inventory*). NOT a raw Item* — the high bits are a generation
    // counter to detect stale references, the low bits index into a global handle table.
    // The handle is resolved to the live Item* through Core_HandleProxy<RShared_Item>.
    struct ItemHandle { uint64_t raw; };

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

        // ─── 0x4C0..0x5AF: anim-track ptr arrays into the game-data slab (.data
        //   region 0x1dfb74c9xxx). Each slot is an asset-record descriptor; not
        //   exposed because indexes don't have a stable semantic meaning. ────
        BYTE               Pad4C0[0x5B0 - 0x4C0]; // 0x4C0

        // ─── ★ Player Inventory component ─────────────────────────────────
        // Verified via decompile of sub_125E3B0 (RClient_InventoryItemTypeCount):
        //   World+0x2B0 → mgr; mgr+0x10 → AgentInfo; AgentInfo+0x50 → Agent
        //   (back-ref m_pAgent); Agent+0x5B0 → PlayerInventory*.
        // Live verified: Agent[0x5B0] points to a heap struct whose vtable is
        // g_pBase + 0x2EB9C68 and whose +0x08 stores this Agent* (back-ref).
        // See PlayerInventory class below for full layout.
        PlayerInventory*   m_pInventory;        // 0x5B0  ★ owned-items container

        // ─── 0x5B8..0x6DF: remainder of the asset-record slab + 2 small managed
        //   objects (16B at +0x698 vtable &unk_30D6058; 208B at +0x6B0). ────
        BYTE               Pad5B8[0x6E0 - 0x5B8]; // 0x5B8

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

        // Client-side cosmetic rogue override. Writes the two replicated rogue
        // bytes that drive the SHD watch/antenna emissive recolor (orange<->red):
        //   +0x75C m_IsRogue            (0->1 verified live in ReClass)
        //   +0x762 rogue-status mirror  (0->1 verified live in ReClass)
        // The watch/antenna prop material is data-bound to the AgentNodeRogueStatus
        // graph node, so flipping these bytes is enough to recolor with no server
        // action (Division 1 is client-authoritative, no anti-cheat). Genuine
        // rogue state is server-replicated, so when NOT actually rogue this must
        // be re-applied every frame to hold the look.
        void SetRogueVisual(bool on)
        {
            const BYTE v = on ? 1 : 0;
            *reinterpret_cast<BYTE*>(reinterpret_cast<char*>(this) + 0x75C) = v;
            *reinterpret_cast<BYTE*>(reinterpret_cast<char*>(this) + 0x762) = v;
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

    // ──────────────────────────────────────────────────────────────────────
    //  ITEM DESCRIPTORS (loaded from .mitem files at startup)
    //
    //  Every .mitem file becomes a heap-allocated descriptor instance owned
    //  by InventoryConfig. The factory sub_F48FE0 dispatches on the class
    //  tag string in the .mitem header and picks one of ~20 ctors. The base
    //  class is Item (704B, vtable g_pBase + 0x2CB9E38); ArmorItem extends
    //  it to 688B with armor metadata; WeaponItem to 1008B; SkinItem family
    //  (BackpackSkinItem / WeaponSkinItem / PatchSkinItem) to 768B.
    //
    //  These structs describe the descriptor's parsed metadata. They are
    //  shared/immutable — multiple owned-item instances share the same
    //  descriptor. The player's per-instance state (mods, rolls, sockets)
    //  lives in a separate vector that PlayerInventory's hashmaps map to.
    // ──────────────────────────────────────────────────────────────────────

    // Item base descriptor (688 bytes for ArmorItem). Verified offsets from
    // both the .mitem parser sub_F2FD40 AND live readback of 7+ descriptor
    // instances on this session (MP5 SMG, HoloScope mod, GearMod_Generated,
    // grenade, classified mask, classified backpack, incendiary ammo).
    //
    // Same struct shape applies to NPCArmorItem and BalaclavaMask (only the
    // myEquipmentSlot / myAttributeGenType / myInventoryCategory enum values
    // differ).
    //
    // Layout note: the SnowdropString at +0x18 occupies 16 bytes (ends at
    // +0x28). Empirically there is a 4-byte padding gap at +0x28..+0x2B,
    // and m_Quality starts at +0x2C. Earlier drafts of this header (and
    // CLAUDE.md table column-counts) implicitly assumed m_Quality at +0x28,
    // which was off by 4. The correct offsets are below — verified.
    class ItemDescriptor
    {
    public:
        void*           vtable;                  // 0x000  &(g_pBase + 0x2CA74A0) for ArmorItem
        uint8_t         m_Uid[16];               // 0x008  16-byte UID from < uid=… > header
        SnowdropString  m_Identifier;            // 0x018  e.g. "player_weapon_submachinegun_mp5_t1_v1"
        BYTE            Pad28[4];                // 0x028  alignment gap
        int32_t         m_Quality;               // 0x02C  myQuality enum (live: 0/2/3 commonly)
        int32_t         m_MaxPossessionCount;    // 0x030  myMaxPossessionCount (-1 = unlimited)
        int32_t         m_SocketCount;           // 0x034  mySocketCount
        int32_t         m_PrescriptedPowerLevel; // 0x038  myPrescriptedPowerLevel
        int32_t         m_Subtype;               // 0x03C  mySubtype
        int32_t         m_EquipmentSlot;         // 0x040  ★ myEquipmentSlot (verified live:
                                                 //   13=WeaponModifier, 14=WeaponSlot, 17=Marksman
                                                 //   AmmoSlot, etc.). Enum at game string 0x2CA04E0.
        int32_t         m_AttributeGenType;      // 0x044  ★ myAttributeGenType — drives renderer
                                                 //   side effects (cosmetic masks set this to Hat
                                                 //   to trigger head/hair swap)
        BYTE            Pad048[0x58 - 0x48];     // 0x048
        int32_t         m_InventoryCategory;     // 0x058  myInventoryCategory
        BYTE            Pad05C[4];               // 0x05C
        uint8_t         m_CodeVersion;           // 0x060
        uint8_t         m_DataVersion;           // 0x061
        uint8_t         m_IsLootedAutomatically; // 0x062
        uint8_t         m_IsEquippable;          // 0x063
        uint8_t         m_CanUnequip;            // 0x064
        uint8_t         m_IsUsable;              // 0x065
        uint8_t         m_IsDestroyable;         // 0x066
        uint8_t         m_CanBeSold;             // 0x067
        uint8_t         m_CanBeTraded;           // 0x068
        uint8_t         m_ShouldGenerateItem;    // 0x069
        uint8_t         m_ShouldGenerateName;    // 0x06A
        uint8_t         m_ShouldGenerateModel;   // 0x06B
        uint8_t         m_OverwriteSubtype;      // 0x06C
        uint8_t         m_IsExotic;              // 0x06D
        BYTE            Pad06E[2];               // 0x06E
        uint8_t         m_IsClassified;          // 0x070
        BYTE            Pad071[0xF];             // 0x071
        SnowdropString  m_IconImage;             // 0x080  myIconImage path
        BYTE            Pad090[0x10];            // 0x090
        SnowdropString  m_IconSprite;            // 0x098  myIconSprite ref
        BYTE            Pad0A8[0x18];            // 0x0A8
        SnowdropString  m_UIName;                // 0x0B0  myUIName (display name)
        BYTE            Pad0C0[0x30];            // 0x0C0
        SnowdropString  m_Description;           // 0x0F0  myDescription
        BYTE            Pad100[0x30];            // 0x100
        SnowdropString  m_ShortDescription;      // 0x130  myShortDescription
        BYTE            Pad140[0x20];            // 0x140
        SnowdropString  m_MaleVisualGearName;    // 0x170  ★ male .mgraphobject path
        SnowdropString  m_FemaleVisualGearName;  // 0x180  ★ female .mgraphobject path
        SnowdropString  m_ResolvedMalePath;      // 0x190  computed via sub_F00FE0 at load
        SnowdropString  m_ResolvedFemalePath;    // 0x1A0  same, female
        BYTE            Pad1B0[0x2B0 - 0x1B0];   // 0x1B0  level/power-level fields + asset refs
    }; // sizeof = 0x2B0 (688 bytes)

    // SkinItem descriptor (768 bytes) — shared by BackpackSkinItem, WeaponSkinItem,
    // PatchSkinItem. vtable g_pBase + 0x2CBA028. Layout = Item base 688B
    // + 80B of skin-specific state. Verified live against b_camo_solid_pink.
    class SkinItemDescriptor : public ItemDescriptor
    {
    public:
        SnowdropString  m_SkinTexture;           // +688 (0x2B0)  ★ mySkinTexture .dds path
        SnowdropString  m_OverrideDiffuse;       // +704 (0x2C0)  empty on default skins
        SnowdropString  m_OverrideNormal;        // +720 (0x2D0)  empty on default skins
        float           m_RotationU;             // +736 (0x2E0)  myRotationU
        float           m_RotationV;             // +740 (0x2E4)  myRotationV
        float           m_ScaleX;                // +744 (0x2E8)  myScaleX (default 1.0)
        float           m_ScaleY;                // +748 (0x2EC)  myScaleY (default 1.0)
        uint32_t        m_Color1_ARGB;           // +752 (0x2F0)  myColor1 (ARGB packed)
        uint32_t        m_Color2_ARGB;           // +756 (0x2F4)  myColor2
        uint32_t        m_Color3_ARGB;           // +760 (0x2F8)  myColor3
        uint32_t        field_2FC;               // +764 (0x2FC)
    }; // sizeof = 0x300 (768 bytes)

    // ──────────────────────────────────────────────────────────────────────
    //  INVENTORY SUBSYSTEM
    //
    //  The Division has two parallel inventory data structures:
    //
    //    1. InventoryConfig — global descriptor cache. Holds one Item* per
    //       .mitem file in the game (4634 on this build). Indexed by name
    //       and by 16-byte UID. This is what InventoryConfig::LookupByName
    //       returns; the wrapper sub_F07C40 normalizes the name first.
    //
    //    2. PlayerInventory — per-Agent owned-items store. Lives on the
    //       Agent at +0x5B0. Holds the items the player has picked up /
    //       crafted / been given. Items are keyed by their descriptor
    //       (the same Item* you'd get from InventoryConfig::LookupByName)
    //       and each key maps to a vector of instance records.
    //
    //  The MC scripting RPCs all live on these two:
    //    RClient_Inventory{ItemCount,ItemTypeCount,CategoryItem,EquipItem,
    //                      RequestEquipItem,UnEquipItem,DeleteItem,DropItem,…}
    //    → dispatched against PlayerInventory.
    //    RClient_InventoryItemPouch / pouch capacity / pouch new-items
    //    → dispatched against the per-pouch sub-container within PlayerInventory.
    //
    //  Items are passed around the engine as RShared_Handle<RShared_Item>
    //  (8-byte handle with generation counter), NOT as raw Item*. Resolve
    //  via Core_HandleProxy<RShared_Item>. See ItemHandle above.
    // ──────────────────────────────────────────────────────────────────────

    // 9-value enum used by every Inventory_* RPC. Names taken from the engine
    // string table at 0x2EBA580 / FormulaAttribute strings (DarkZoneInventoryFinal,
    // DefaultInventoryFinal, ResourceInventoryFinal, …). Numeric values are
    // *probable* — verify against engine when actually keying.
    enum class InventoryPouchType : int32_t
    {
        Default        = 0,
        Resource       = 1,
        Vanity         = 2,
        BackupSpace    = 3,
        Stash          = 4,
        DarkZone       = 5,
        DarkZoneStash  = 6,
        Mailbox        = 7,
        SurvivalStash  = 8,
    };

    // PlayerInventory partitions its contents across 8 category-tagged
    // hashmaps. The tag byte at InventoryHashmap+0x18 encodes which kind.
    // Verified live by reading the FIRST bucket entry's descriptor in
    // each map — labels are inferred from that one example per map, not
    // exhaustively confirmed.
    //
    // ★ CAVEAT: map[1] is NOT the player's actual equipped weapon set.
    // Disproven 2026-05-11 — user's real loadout (ACR / M870 / 93R / M4
    // Exotic) never appeared in map[1], and weapon swaps don't update it.
    // Map[1]'s real semantics are TBD (possibly a shooting-range loadout
    // or a script reference cache). Real equipped weapons live in
    // RShared_InventoryWeaponSet, accessed via the RClient_GetWeapon-
    // FromInventoryWeaponSet RPC — that struct has not been located yet.
    enum class PlayerInventoryCategory : int32_t
    {
        GearMods       = 0,  // GearMod_Generated_Item etc — gear sockets
        UnknownMap1    = 1,  // ★ NOT real equipped weapons; semantics TBD
        Talents        = 2,  // talent registry (e.g. "Bullseye"); .data-backed,
                             //   759 entries — this is the global pool, not owned
        Grenades       = 3,  // HE_grenade, flashbang, etc.
        Armor          = 4,  // body / face / hands / knees / thighs armor pieces
        BackpackOrSet  = 5,  // classified set pieces (back-slot armor + ?)
        Resources      = 6,  // ammo, consumables, crafting tokens
        Reserved7      = 7,  // ★ empty on this session — likely vanity / sets
    };

    // ★ Consumable stack counts AND every currency the player owns live
    // in the heap-allocated `PlayerSessionState` struct (see class below).
    // It is NOT in the PlayerInventory hashmaps or in the 6 OwnedItemInstance
    // slot records — those track distinct owned-descriptor instances, not
    // stack counts.
    //
    // PlayerSessionState is gameplay-authoritative (write-freeze verified
    // changes the actual in-game value). Roughly 10 downstream mirror
    // addresses in .data slab and other heap arenas update each frame.
    //
    // ★ VERIFIED FIELD OFFSETS (relative to PlayerSessionState base):
    //   +0x000  Grenade count
    //   +0x040  Medkit count
    //   +0x440  Premium Credits (paid currency)
    //   +0x460  Credits
    //   +0x480  Dark Zone Fund
    //   +0x4A0  Phoenix Credits
    //   +0x4C0  Target Intel
    //   +0x4E0  Dark Zone Keys
    //   +0x8C0  Global Event (GE) Credits
    //
    // The currency entries at +0x2F0..+0x7DF are 32-byte records
    // (UUID + DWORD count + 12-byte padding); see OwnedItemCountRecord.
    //
    // For mod purposes: read/write directly via PlayerSessionState::Credits()
    // etc. (inline accessors below). The struct's heap base is session-
    // specific — see PlayerSessionState class comment for rediscovery
    // fingerprint (Credits UUID at +0x450 within the struct).
    //
    // See memory file project-currency-table.md for the full layout, all
    // 7 known currencies + GE Credits, UUID family analysis, and the
    // currency-index pointer array.

    // 21 (= 0x15) string-tag dispatched item classes used by the .mitem item
    // factory sub_F48FE0 (descriptor parser, not the player's inventory).
    // Each tag selects one of ~20 ctors (see CLAUDE.md "Item / AssetRecord
    // class hierarchy" for sizes + ctor addresses).
    enum class ItemFactoryClass : int32_t
    {
        ArmorItem       = 0,
        WeaponItem      = 1,
        ConsumableItem  = 2,
        ModItem         = 3,
        CraftingItem    = 4,
        QuestItem       = 5,
        BundleItem      = 6,
        MysteryBoxItem  = 7,
        CurrencyItem    = 8,
        NPCArmorItem    = 9,
        WeaponSkinItem  = 10,
        BackpackSkinItem= 11,
        PatchSkinItem   = 12,
        // values 13..20 cover HVTContractItem, Loadout, etc.
    };

    // Hashmap<Item*, Vector<ItemHandle>> — one entry per item descriptor the
    // player owns; the value vector lists the live instance handles. The
    // hashmap struct is 0x3A0 bytes (verified by stride between consecutive
    // maps in PlayerInventory: 0x21fc6d52b80 → 0x21fc6d52f20 → 0x21fc6d532c0
    // → … all spaced 0x3A0 apart).
    //
    // The 8 hashmaps that PlayerInventory owns are tagged 0..7 in the
    // PlayerInventoryCategory enum byte at +0x18 — that's how the engine
    // knows which category each map represents.
    //
    // Bucket layout (24 bytes per entry, verified from sub_F08450 hash walk
    // and confirmed live on every map's first entry):
    //   +0x00 Item*    m_KeyDescriptor    (.mitem descriptor — InventoryConfig entry)
    //   +0x08 void*    m_ValuePtr         (heap array of instance records or handles)
    //   +0x10 uint32_t m_ValueCount       (entries in the value array)
    //   +0x14 uint32_t m_ValueCapacity    (allocated cap)
    //
    // After cap*24 bytes of entries, the buckets array continues with a
    // chain table of int[2*cap] used as a linked-list-of-next-indices for
    // hash-collision resolution. Hash function is Fibonacci-mix
    // (2135587865 * key XOR (0x9E3779B97F4A7C19 * key >> 32)) mod (2*cap-1).
    //
    // Probing: sub_F08450(playerInventory, descriptor, &outVec) iterates
    // exactly 4 hashmaps at PI+0x158..+0x170 (i.e. the first half of the
    // category array — categories 0..3). The remaining maps at +0x178..+0x190
    // are walked by similar functions specialized for the higher categories.
    class InventoryHashmap
    {
    public:
        void*                    vtable;        // 0x000  &(g_pBase + 0x2DC8FB0)
        PlayerInventory*         m_pOwner;      // 0x008  back-ref to enclosing PlayerInventory
        void*                    m_pAllocator;  // 0x010  shared allocator handle
        PlayerInventoryCategory  m_CategoryId;  // 0x018  ★ which category this map holds
        int32_t                  field_1C;      // 0x01C
        void*                    m_Buckets;     // 0x020  array of 24-byte entries (see above)
        uint32_t                 m_Count;       // 0x028  live entries
        uint32_t                 m_Capacity;    // 0x02C
        void*                    m_pAuxArray;   // 0x030  sometimes secondary index
        // remaining ~0x370 bytes = scratch/sort buffers; not exposed.
    }; // sizeof ≈ 0x3A0

    // ★ PlayerInventory — per-Agent owned-items container.
    //
    // Reached via Agent::m_pInventory (Agent + 0x5B0). Verified live:
    //   vtable = g_pBase + 0x2EB9C68
    //   +0x008 = the owning Agent* (e.g. the local player)
    //   +0x158..+0x190 = eight inventory hashmaps (see m_CategoryMaps below;
    //                    sub_F08450 only walks the first four)
    //
    // Use sub_F08450(playerInventory, item_descriptor, &out_vec) to look up
    // every owned instance for a given descriptor across the first four
    // hashmaps. Use the engine RClient_* RPCs (registered in sub_1397E90)
    // for higher-level queries — see CLAUDE.md "Inventory / Item-descriptor
    // / Equip pipeline" for the full RPC surface.
    //
    // Internal pools at +0x080..+0x0A8 (6 ptrs) and +0x128..+0x148 (5–6 ptrs)
    // point into the .data slab 0x19fd2b1xxx — they're OwnedItemInstance
    // records (96 bytes; see project_player_inventory.md memory note).
    // The actual ownership tables are the 8 hashmaps below.
    class PlayerInventory
    {
    public:
        void*               vtable;                 // 0x000  &(g_pBase + 0x2EB9C68)
        Agent*              m_pOwner;               // 0x008  ★ owning Agent (player or NPC)
        void*               m_pNetMgr;              // 0x010  network/sync subsystem
        void*               m_pSboBuffer;           // 0x018  pre-allocated working buffer
        void*               m_pSlotTable1;          // 0x020  equipped-slot table (weapons / armor slots)
        void*               m_pSlotTable2;          // 0x028
        void*               m_pSlotTable3;          // 0x030
        void*               m_pSlotTable4;          // 0x038
        void*               m_pSlotTable5;          // 0x040
        void*               m_pStorage1;            // 0x048
        void*               m_pStorage2;            // 0x050
        void*               m_pStorage3;            // 0x058
        uint32_t            m_Flags;                // 0x060  packed flags (live 0x123D0101)
        uint32_t            field_64;               // 0x064
        uint32_t            m_TotalItemCount;       // 0x068  live: ~276
        uint32_t            m_TotalItemCapacity;    // 0x06C  live: ~289
        uint64_t            m_UpdateSequence;       // 0x070  bumped on every change; drives
                                                    //         RClient_GetInventoryUpdateCount

        BYTE                Pad078[0x80 - 0x78];    // 0x078

        // Category descriptor slots (6 entries — likely matches the 6 visible
        // pouches in the UI). Each points into .data at 0x19fd2b1xxx.
        void*               m_pCategorySlots[6];    // 0x080..0x0AF

        BYTE                Pad0B0[0x158 - 0x0B0];  // 0x0B0  3 more sparse slots + sort caches

        // ★ The 8 category-partitioned inventory hashmaps.
        //
        // Each map is tagged with its category-id at InventoryHashmap+0x18
        // (verified live: maps[0..7] have ids 0..7 stored there). The 8 maps
        // are co-allocated in one slab at stride 0x3A0, so consecutive
        // m_CategoryMaps[i] pointers walk through a contiguous range.
        //
        // Index → category (sampled by one descriptor per map; labels
        // inferred from that single example except where noted):
        //   [0] GearMods         live: 30 entries  (e.g. Player_GearMod_Generated_Item x12)
        //   [1] ★ TBD            live: 2  entries  (MP5 SMG + HoloScope mod) —
        //                                      NOT the real equipped weapons.
        //                                      Disproven by weapon-swap diff test.
        //   [2] Talents          live: 759 entries — GLOBAL talent registry, not owned;
        //                                      buckets are in the game module .data
        //                                      segment (~0x1dfbba15800), read-only.
        //   [3] Grenades         live: 13 entries  (HE/flashbang/etc.)
        //   [4] Armor            live: 88 entries  (face/chest/hands/knees/thighs/back)
        //   [5] BackpackOrSet    live: 18 entries  (classified set pieces, back-armor)
        //   [6] Resources        live: 14 entries  (ammo / consumables)
        //   [7] Reserved7        live: 0  entries  (NULL buckets — unused this session)
        //
        // sub_F08450(pi, descriptor, &outVec) iterates exactly the first 4
        // (maps[0..3]). Higher categories have their own walkers — the access
        // pattern is the same: hash → probe → push (value_ptr, value_count).
        //
        // ★ Hashmap bucket counts are NOT consumable stack counts. The
        // bucket's (count, cap) at +0x10/+0x14 records the number of
        // distinct owned instances of that descriptor (e.g. 1 medkit
        // instance with stack=4 → bucket count=1, not 4). The actual stack
        // count lives in a heap-allocated consumable-state struct
        // (grenade@+0x00, medkit@+0x40) — see PlayerInventoryCategory enum
        // comment above and project_consumable_status.md memory note.
        InventoryHashmap*   m_CategoryMaps[8];      // 0x158..0x190

        void*               m_pLoadoutTable;        // 0x198  vector<Loadout*>
        BYTE                Pad1A0[8];              // 0x1A0
        void*               m_pPouchStateArray;     // 0x1A8  per-pouch state (capacity, dirty flag)

        // 6 ptrs into .data — string interning table for pouch names.
        void*               m_pPouchNameTable[6];   // 0x1B0..0x1DF

        // The struct continues beyond 0x1E0 with per-pouch state buffers,
        // sort indices, and the new-item bitfield. Not yet fully mapped.
        BYTE                Pad1E0[0x100];          // 0x1E0  reserve (verify when extending)
    };

    // ──────────────────────────────────────────────────────────────────────
    //  PLAYER SESSION STATE — consumables, currencies, GE Credits, and a
    //  generic owned-item-count table.
    //
    //  Single heap-allocated struct holding everything the player session
    //  exposes as a "current quantity" — grenade/medkit stacks, all
    //  currencies (Credits, DZ Fund, Phoenix, GE, Premium, Target Intel,
    //  Dark Zone Keys), perk-progression tokens, season caches, and many
    //  other (UUID, count) records.
    //
    //  ★ Confirmed gameplay-authoritative master via write-freeze:
    //  freezing any field here actually changes the in-game value and
    //  propagates to ~10 downstream mirror addresses (engine .data
    //  caches + UI list-stores).
    //
    //  Layout reverse-engineered against TheDivision.exe live session
    //  2026-05-11. Offsets relative to the grenade-count field which
    //  appears to be the struct base.
    //
    //  REDISCOVERY: this struct is reachable from PlayerInventory but the
    //  exact pointer slot has not been pinned yet. Fingerprint for
    //  scanning: at +0x460 from any heap allocation > 0x900 bytes, expect
    //  Credits UUID `9c bf 9e 53 a2 de 5d 4f 3b 62 76 f1 54 15 00 00` 16
    //  bytes earlier (= +0x450). Or use the verified Credits-master
    //  address from Cheat Engine, then subtract 0x460 to get the base.
    //
    //  Use the inline GetPlayerSessionState() helper below once you have
    //  a way to walk to this struct from a stable pointer.
    // ──────────────────────────────────────────────────────────────────────

    // Generic 32-byte record in the OwnedItemCount table.
    // Indexed by item UUID (which matches the `< uid=… >` in .mitem files).
    // Currencies, perk tokens, keys, vanity unlocks, season caches all use
    // this same shape.
    struct OwnedItemCountRecord
    {
        uint8_t  m_Uid[16];      // 0x00  16-byte item UUID
        uint32_t m_Count;        // 0x10  owned amount / count
        BYTE     Pad14[0x0C];    // 0x14  padding / reserved (always zero observed)
    }; // sizeof = 0x20 (32 bytes)

    // ★ PlayerSessionState — heap-allocated, gameplay-authoritative.
    //
    // Reachable from PlayerInventory (`Agent+0x5B0`) but the specific
    // pointer slot is not yet identified. Once located, use the offsets
    // below to read/write any player resource without scanning.
    class PlayerSessionState
    {
    public:
        // ── Consumable stack counts ─────────────────────────────────────
        uint32_t m_GrenadeCount;          // 0x000  ★ verified master
        BYTE     Pad004[0x03C];           // 0x004
        uint32_t m_MedkitCount;           // 0x040  ★ verified master
        BYTE     Pad044[0x2AC];           // 0x044  (TBD: signature ammo,
                                          //   armor kits, skill resources,
                                          //   weapon perk stacks, XP, etc.)

        // ── OwnedItemCount table at +0x2F0 ──────────────────────────────
        // ~32 records of (UUID, count) at +0x2F0..+0x7DF (and possibly
        // beyond). Walk by stride 0x20. The currency / token entries
        // below have verified UUID→meaning mappings.
        // Access via FindByUid() helper, OwnedItemTable(), or the named
        // accessors. No member declared here to avoid a zero-sized-array
        // extension warning.

        // ── Named currency records (all verified live by user) ─────────
        // Each is at a fixed offset within m_OwnedItemTable.
        // Layout per slot: UUID@+0x00, count@+0x10 (use the +N below
        // for the COUNT directly):
        //   Premium Credits  — paid currency, UUID family `e2 01 58 07`
        //   Credits          — main currency, UUID family `a2 de 5d 4f`
        //   DZ Fund          — Dark Zone currency, same family
        //   Phoenix Credits  — endgame currency, family `e2 01 58 07`
        //   Target Intel     — high-value-target tokens, family `a2 de 5d 4f`
        //   Dark Zone Keys   — DZ chest-unlock keys, family `dc c9 42 c7`
        //   GE Credits       — Global Event currency, standalone DWORD
        // (Two unused slots with UUID family `00 26 31 5d` precede these;
        //  they're zero-valued event-currency placeholders.)

        // Base pointer to the OwnedItemCount table (records at stride 0x20).
        OwnedItemCountRecord* OwnedItemTable() {
            return reinterpret_cast<OwnedItemCountRecord*>(
                reinterpret_cast<BYTE*>(this) + 0x2F0);
        }

        // Inline accessors — convenient direct read/write
        uint32_t& PremiumCredits()  { return *(uint32_t*)((BYTE*)this + 0x440); }
        uint32_t& Credits()         { return *(uint32_t*)((BYTE*)this + 0x460); }
        uint32_t& DZFund()          { return *(uint32_t*)((BYTE*)this + 0x480); }
        uint32_t& PhoenixCredits()  { return *(uint32_t*)((BYTE*)this + 0x4A0); }
        uint32_t& TargetIntel()     { return *(uint32_t*)((BYTE*)this + 0x4C0); }
        uint32_t& DZKeys()          { return *(uint32_t*)((BYTE*)this + 0x4E0); }
        uint32_t& GECredits()       { return *(uint32_t*)((BYTE*)this + 0x8C0); }
        uint32_t& Grenades()        { return *(uint32_t*)((BYTE*)this + 0x000); }
        uint32_t& Medkits()         { return *(uint32_t*)((BYTE*)this + 0x040); }

        // The struct continues past +0x8D0 (another nested object with
        // vtable g_pBase + 0x5491980 — unmapped). Total size estimated
        // > 0x900 bytes.

        // Lookup a record by 16-byte UUID. Walks the table forwards until
        // it finds a matching UUID or runs out. Useful for currencies /
        // tokens whose offset we haven't pinned.
        OwnedItemCountRecord* FindByUid(const uint8_t uid[16])
        {
            OwnedItemCountRecord* rec = OwnedItemTable();
            for (int i = 0; i < 64; ++i, ++rec)  // safety cap at 64 records (2 KB)
            {
                bool match = true;
                for (int b = 0; b < 16; ++b)
                {
                    if (rec->m_Uid[b] != uid[b]) { match = false; break; }
                }
                if (match) return rec;
                // Stop if we hit an unallocated zero-UUID record after a non-zero
                // — uninitialized memory check.
                uint64_t* u64 = (uint64_t*)rec->m_Uid;
                if (i > 8 && u64[0] == 0 && u64[1] == 0) return nullptr;
            }
            return nullptr;
        }
    };

    // ★ Known currency UUIDs (verified 2026-05-11 against .mitem descriptors).
    // Stable across sessions — they don't change unless the game's item
    // database is rebuilt. Use with PlayerSessionState::FindByUid().
    namespace CurrencyUid
    {
        // family `a2 de 5d 4f` — earned in-game (cash + similar)
        static constexpr uint8_t Credits[16] =
            { 0x9c, 0xbf, 0x9e, 0x53, 0xa2, 0xde, 0x5d, 0x4f,
              0x3b, 0x62, 0x76, 0xf1, 0x54, 0x15, 0x00, 0x00 };
        static constexpr uint8_t DZFund[16] =
            { 0xc8, 0xef, 0x9e, 0x53, 0xa2, 0xde, 0x5d, 0x4f,
              0x76, 0x4f, 0xbb, 0x2c, 0x8b, 0x03, 0x00, 0x00 };
        static constexpr uint8_t TargetIntel[16] =
            { 0x89, 0x4a, 0x98, 0x53, 0xa2, 0xde, 0x5d, 0x4f,
              0xf5, 0x06, 0x61, 0x30, 0x34, 0x16, 0x00, 0x00 };
        // family `e2 01 58 07` — token currencies
        static constexpr uint8_t PremiumCredits[16] =
            { 0x75, 0xc6, 0x25, 0x58, 0xe2, 0x01, 0x58, 0x07,
              0x46, 0x97, 0xf9, 0xa2, 0xb8, 0x12, 0x00, 0x00 };
        static constexpr uint8_t PhoenixCredits[16] =
            { 0x1b, 0x92, 0xd4, 0x55, 0xe2, 0x01, 0x58, 0x07,
              0xe9, 0xf3, 0xf1, 0x5d, 0x62, 0x0a, 0x01, 0x00 };
        // family `dc c9 42 c7` — special-unlock items
        static constexpr uint8_t DZKeys[16] =
            { 0xff, 0x32, 0xf8, 0x54, 0xdc, 0xc9, 0x42, 0xc7,
              0x57, 0x4e, 0x50, 0x21, 0x6c, 0x88, 0x00, 0x00 };
    }

    // ★ InventoryConfig — global .mitem descriptor cache.
    //
    // Reached via World::m_pConfigContainer + 0xD8, or equivalently
    // Client+0x18 → +0xD8 (same address). Built once at startup by
    // sub_F49920, which globs rogue/game system data/juice/item/*.mitem
    // and registers each through sub_F7D9A0 (which calls sub_E5EF10 to
    // insert into the by-name map and sub_E660C0 for by-UID).
    //
    // Live on this build: 4634 descriptors / 5394 cap, by-name and by-UID
    // hashmaps both 4634/8191. Sub-arrays at +0x30..+0x60 partition the
    // descriptors by class (verified by counts):
    //   +0x30  Item*[]  count 89   (likely ArmorItem)
    //   +0x40  Item*[]  count 25   (WeaponItem)
    //   +0x50  Item*[]  count 5    (small specialized class)
    //   +0x60  Item*[]  count 19   (etc.)
    //
    // Lookup engine functions (g_pBase + offset):
    //   0xF04270 sub_F04270(hashmap, name)        → slot index (-1 if not found)
    //   0xF07C40 sub_F07C40(cfg, name)            → Item* (normalizes name)
    //   0xCEA470 sub_CEA470(parent, name)         → wrapper: reads cfg from parent+0xD8
    //   0xF49920 sub_F49920                       → bulk loader (startup)
    //   0xF7D9A0 sub_F7D9A0(cfg, desc, summary)   → register one descriptor
    class InventoryConfig
    {
    public:
        void*       vtable;                  // 0x000  &(g_pBase + 0x306A578)
        void*       vtable2;                 // 0x008  paired vtable (multi-inheritance)
        void*       m_pAllocator;            // 0x010
        int32_t     field_18;                // 0x018  2
        int32_t     field_1C;                // 0x01C  2

        // ── Main descriptor array — every .mitem loaded ─────────────────
        void**      m_pDescriptorArray;      // 0x020  Item*[]  live: 4634 entries
        uint32_t    m_DescriptorCount;       // 0x028
        uint32_t    m_DescriptorCapacity;    // 0x02C

        // ── Category sub-arrays — same descriptors partitioned by class.
        //    Each is (ptr, count, cap). Six in total at +0x30..+0x70.
        void**      m_pCategory0_Ptr;        // 0x030  live: 89 entries
        uint32_t    m_Category0_Count;       // 0x038
        uint32_t    m_Category0_Cap;         // 0x03C
        void**      m_pCategory1_Ptr;        // 0x040  live: 25 entries
        uint32_t    m_Category1_Count;       // 0x048
        uint32_t    m_Category1_Cap;         // 0x04C
        void**      m_pCategory2_Ptr;        // 0x050  live: 5 entries
        uint32_t    m_Category2_Count;       // 0x058
        uint32_t    m_Category2_Cap;         // 0x05C
        void**      m_pCategory3_Ptr;        // 0x060  live: 19 entries
        uint32_t    m_Category3_Count;       // 0x068
        uint32_t    m_Category3_Cap;         // 0x06C
        int32_t     m_Category4_Count;       // 0x070
        int32_t     field_74;                // 0x074
        void**      m_pCategory4_Ptr;        // 0x078

        // ── 5 sub-pools at +0x80..+0xA0 (5 contiguous pointers) — these are
        //    template/parent-descriptor groups (e.g. armor_template, weapon_template).
        void*       m_pTemplateGroups[5];    // 0x080..0x0A7
        void*       m_pIconTable;            // 0x0A8

        BYTE        Pad0B0[0xC0 - 0xB0];     // 0x0B0

        // Small lookup tables — used by minor sub-systems. Each (ptr, count, cap).
        void**      m_pTable_C8_Ptr;         // 0x0C8  live: count 7 / cap 8
        uint32_t    m_Table_C8_Count;        // 0x0D0
        uint32_t    m_Table_C8_Cap;          // 0x0D4
        void**      m_pTable_D8_Ptr;         // 0x0D8  live: count 10 / cap 12
        uint32_t    m_Table_D8_Count;        // 0x0E0
        uint32_t    m_Table_D8_Cap;          // 0x0E4

        BYTE        Pad0E8[0x128 - 0x0E8];   // 0x0E8

        void**      m_pTable_128_Ptr;        // 0x128  live: count 44 / cap 62
        uint32_t    m_Table_128_Count;       // 0x130
        uint32_t    m_Table_128_Cap;         // 0x134
        void**      m_pTable_138_Ptr;        // 0x138

        BYTE        Pad140[0x170 - 0x140];   // 0x140

        // ★ Primary lookup: by-name hashmap.
        // sub_F07C40(cfg, name) calls sub_F04270 against this and returns
        // *(QWORD*)(buckets + 24*slot + 16) on hit (descriptor pointer is
        // at offset +16 of each entry, key string at +0..+8 / +0..+15).
        void*       m_NameHashmap_Buckets;   // 0x170
        uint32_t    m_NameHashmap_Count;     // 0x178  live: 4634
        uint32_t    m_NameHashmap_Capacity;  // 0x17C  live: 8191

        // Secondary hashmaps (verified live):
        void*       m_Hashmap_180_Buckets;   // 0x180  count 89 / cap 127
        uint32_t    m_Hashmap_180_Count;     // 0x188
        uint32_t    m_Hashmap_180_Capacity;  // 0x18C
        void*       m_Hashmap_190_Buckets;   // 0x190  count 25 / cap 31
        uint32_t    m_Hashmap_190_Count;     // 0x198
        uint32_t    m_Hashmap_190_Capacity;  // 0x19C

        // ★ Secondary lookup: by 16-byte UID hashmap.
        // Built by sub_E660C0 during sub_F7D9A0. Entry key = item's UID
        // (from < uid=XXXX > in the .mitem header, stored at descriptor +8).
        void*       m_UidHashmap_Buckets;    // 0x1A0
        uint32_t    m_UidHashmap_Count;      // 0x1A8  live: 4634
        uint32_t    m_UidHashmap_Capacity;   // 0x1AC  live: 8191

    public:
        // Resolve an item name to its Item* descriptor. Wraps sub_F07C40.
        // The wrapper auto-normalizes the name (lowercased via sub_16D9F0)
        // so case in the input doesn't matter.
        void* LookupByName(const char* itemName)
        {
            typedef void* (__fastcall* tLookup)(InventoryConfig*, const char*);
            tLookup Lookup = (tLookup)(g_pBase + 0xF07C40);
            return Lookup(this, itemName);
        }
    };

    // The struct stored at *(QWORD*)(World + 0x138). Owns InventoryConfig at
    // +0xD8 and many other engine config tables. The hot field for us is the
    // InventoryConfig pointer. Other slots are session-internal subsystems
    // that we don't need to expose. Reached identically via Client+0x18.
    class WorldConfigContainer
    {
    public:
        void*               vtable;                 // 0x000
        BYTE                Pad008[0xD0];           // 0x008  session subsystem pointers
        InventoryConfig*    m_pInventoryConfig;     // 0x0D8  ★ descriptor cache
        // … many more config-table pointers continue past 0xE0; not exposed.
    };

    class World
    {
    public:
        BYTE Pad000[0x138];
        // ★ World+0x138 is a back-reference to the World's config container.
        // Verified via decompile of sub_125E3B0:
        //   v15 = sub_CEA470(*(QWORD*)(World + 0x138), itemName)
        // where sub_CEA470(parent, name) reads parent+0xD8 as InventoryConfig*
        // and calls sub_F07C40 on it. So this field is the parent struct that
        // owns InventoryConfig + many other engine config tables.
        // (Live: also reachable via Client+0x18 — same address.)
        WorldConfigContainer* m_pConfigContainer; // 0x138
        BYTE Pad140[0x2B0 - 0x140];
        __int64 m_pInput; // 0x2B0
        BYTE Pad02B8[0x10];
        HudSettings* m_pDoF; // 0x2C8
        CameraManager* m_pCameraManager; // 0x2D0
        BYTE Pad2D8[0xE8];
        EnvironmentManager* m_pEnvironmentManager; // 0x3C0
        BYTE Pad3C8[0x68];
        Agent** m_AgentArray; // 0x430
        int m_AgentCount;     // 0x438
    }; // Size: 0x448

    // Definition for the GetInventoryConfig forward-declaration above.
    // Walks the verified chain: RogueClient → +0x120 Client → +0x28 World
    // → +0x138 WorldConfigContainer → +0xD8 InventoryConfig.
    inline InventoryConfig* GetInventoryConfig()
    {
        RogueClient* rc = RogueClient::Singleton();
        if (!rc) return nullptr;
        Client* client = rc->m_pClient;
        if (!client) return nullptr;
        World* world = client->m_pWorld;
        if (!world) return nullptr;
        WorldConfigContainer* cfgContainer = world->m_pConfigContainer;
        if (!cfgContainer) return nullptr;
        return cfgContainer->m_pInventoryConfig;
    }

    // Convenience: get the local player's inventory. Walks World::m_AgentArray[0]
    // (the first agent is conventionally the local player; m_EntityType==1).
    inline PlayerInventory* GetLocalPlayerInventory()
    {
        RogueClient* rc = RogueClient::Singleton();
        if (!rc) return nullptr;
        Client* client = rc->m_pClient;
        if (!client) return nullptr;
        World* world = client->m_pWorld;
        if (!world || world->m_AgentCount <= 0) return nullptr;
        Agent* player = world->m_AgentArray[0];
        if (!player) return nullptr;
        return player->m_pInventory;
    }

    // Convenience: get the local player's Agent. Walks World::m_AgentArray[0]
    // (first agent is conventionally the local player; m_EntityType==1).
    inline Agent* GetLocalPlayerAgent()
    {
        RogueClient* rc = RogueClient::Singleton();
        if (!rc) return nullptr;
        Client* client = rc->m_pClient;
        if (!client) return nullptr;
        World* world = client->m_pWorld;
        if (!world || world->m_AgentCount <= 0) return nullptr;
        return world->m_AgentArray[0];
    }

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