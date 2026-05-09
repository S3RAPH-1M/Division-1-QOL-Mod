#include "HeadManager.h"
#include "Main.h"
#include "Snowdrop.h"
#include "imgui/imgui.h"
#include <DirectXMath.h>
#include <iostream>
#include <atomic>
#include <vector>

using namespace DirectX;

static constexpr DWORD HEAD_BONE_ID = 0x64616548;
static constexpr int   MAX_ENTITIES = 32;

HeadManager* g_pHeadManager = nullptr;

typedef __int64(__fastcall* tBoneTransform)(__int64 a1, __int64 a2);
static tBoneTransform oBoneTransform = nullptr;

static std::atomic<int>       s_hookCallCount{ 0 };
static std::atomic<int>       s_scaleSuccessCount{ 0 };
static std::atomic<__int64>   s_playerAgent{ 0 };
static std::atomic<__int64>   s_playerHookV3{ 0 };  // locked player render skeleton
static std::vector<uintptr_t> s_patchedSlots;

// ─── per-entity table ─────────────────────────────────────────────────────────

struct EntityEntry
{
    std::atomic<__int64> hookV3{ 0 };
    float scale   = 1.0f;
    bool  enabled = false;
};

static EntityEntry      s_entities[MAX_ENTITIES];
static std::atomic<int> s_entityCount{ 0 };

static EntityEntry* FindOrInsert(__int64 v3)
{
    int n = s_entityCount.load(std::memory_order_acquire);
    if (n > MAX_ENTITIES) n = MAX_ENTITIES;
    for (int i = 0; i < n; ++i)
        if (s_entities[i].hookV3.load(std::memory_order_relaxed) == v3)
            return &s_entities[i];
    int slot = s_entityCount.fetch_add(1, std::memory_order_acq_rel);
    if (slot >= MAX_ENTITIES)
    {
        s_entityCount.store(MAX_ENTITIES, std::memory_order_relaxed);
        return nullptr;
    }
    s_entities[slot].hookV3.store(v3, std::memory_order_release);
    return &s_entities[slot];
}

// ─── bone scaling ────────────────────────────────────────────────────────────

enum class EarlyOut
{
    None = 0,
    BranchCheck, NoPPtr, NoV3, NoV2,
    BadBoneCount, NoBoneBase, NoMetaTable, HeadNotFound,
};

static EarlyOut ScaleHeadBone(__int64 a1, __int64 a2, float scale)
{
    if (!*(__int64*)(a2 + 0x50))        return EarlyOut::BranchCheck;
    __int64 pPtr = *(__int64*)(a2 + 0x840); if (!pPtr) return EarlyOut::NoPPtr;
    __int64 v3   = *(__int64*)(pPtr);        if (!v3)   return EarlyOut::NoV3;
    __int64 v2   = *(__int64*)(a1 + 0x58);  if (!v2)   return EarlyOut::NoV2;
    int boneCount = *(int*)(v2 + 0x38);
    if (boneCount <= 0 || boneCount > 512)  return EarlyOut::BadBoneCount;
    __int64 boneBase  = *(__int64*)(v3 + 0x10); if (!boneBase)  return EarlyOut::NoBoneBase;
    __int64 metaTable = *(__int64*)(v3 + 0x20); if (!metaTable) return EarlyOut::NoMetaTable;

    XMVECTOR scaleVec = XMVectorSet(scale, scale, scale, 1.0f);
    for (int slot = 0; slot < boneCount; ++slot)
    {
        if (*(DWORD*)(metaTable + (UINT64)slot * 0x10) != HEAD_BONE_ID) continue;
        XMMATRIX* pMat = (XMMATRIX*)(boneBase + (UINT64)slot * 0x40);
        pMat->r[0] = XMVectorMultiply(pMat->r[0], scaleVec);
        pMat->r[1] = XMVectorMultiply(pMat->r[1], scaleVec);
        pMat->r[2] = XMVectorMultiply(pMat->r[2], scaleVec);
        ++s_scaleSuccessCount;
        return EarlyOut::None;
    }
    return EarlyOut::HeadNotFound;
}

// ─── hook ────────────────────────────────────────────────────────────────────

static __int64 __fastcall hBoneTransform(__int64 a1, __int64 a2)
{
    __int64 result = oBoneTransform(a1, a2);
    s_hookCallCount.fetch_add(1, std::memory_order_relaxed);

    if (!g_pHeadManager || !g_pHeadManager->m_headShrinkEnabled)
        return result;

    __int64 hookV3 = 0;
    __try
    {
        __int64 pPtr = *(__int64*)(a2 + 0x840);
        if (pPtr) hookV3 = *(__int64*)(pPtr);
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {}

    if (!hookV3) return result;

    // If player is locked, only scale that entity
    __int64 playerV3 = s_playerHookV3.load(std::memory_order_relaxed);
    if (playerV3)
    {
        if (hookV3 != playerV3) return result;
        __try { ScaleHeadBone(a1, a2, g_pHeadManager->m_headScale); }
        __except (EXCEPTION_EXECUTE_HANDLER) {}
        return result;
    }

    // Manual identification mode: per-entity enabled sliders
    EntityEntry* ent = FindOrInsert(hookV3);
    if (!ent || !ent->enabled) return result;

    __try { ScaleHeadBone(a1, a2, ent->scale); }
    __except (EXCEPTION_EXECUTE_HANDLER) {}

    return result;
}

// ─── HeadManager ─────────────────────────────────────────────────────────────

HeadManager::HeadManager()
    : m_headShrinkEnabled(false)
    , m_headScale(0.5f)
{
    g_pHeadManager = this;
    InstallHook();
}

HeadManager::~HeadManager()
{
    void* orig = (void*)(g_pBase + 0x53C760);
    for (uintptr_t slot : s_patchedSlots)
    {
        void** pSlot = (void**)slot;
        DWORD oldProt = 0;
        if (VirtualProtect(pSlot, 8, PAGE_READWRITE, &oldProt))
        {
            *pSlot = orig;
            VirtualProtect(pSlot, 8, oldProt, &oldProt);
        }
    }
    s_patchedSlots.clear();
    g_pHeadManager = nullptr;
}

void HeadManager::InstallHook()
{
    oBoneTransform = (tBoneTransform)(g_pBase + 0x53C760);
    std::cout << "[HeadManager] sub_53C760 at 0x"
              << std::hex << (uintptr_t)oBoneTransform << std::dec << "\n";
    std::cout << "[HeadManager] Vtable scan deferred to first Update() call.\n";
}

void HeadManager::Update()
{
    // ── one-time vtable scan ──────────────────────────────────────────────────
    static bool s_scanDone = false;
    if (!s_scanDone)
    {
        s_scanDone = true;
        uintptr_t targetFn = g_pBase + 0x53C760;
        int hitCount = 0;

        std::cout << "[HeadManager] Scanning for ptr 0x" << std::hex << targetFn << std::dec << "\n";

        uintptr_t addr = g_pBase;
        MEMORY_BASIC_INFORMATION mbi{};
        while (VirtualQuery((LPCVOID)addr, &mbi, sizeof(mbi)))
        {
            uintptr_t regionBase = (uintptr_t)mbi.BaseAddress;
            uintptr_t regionEnd  = regionBase + mbi.RegionSize;
            if ((uintptr_t)mbi.AllocationBase == g_pBase && mbi.State == MEM_COMMIT)
            {
                uintptr_t p = (regionBase + 7) & ~7ull;
                for (; p + 8 <= regionEnd; p += 8)
                {
                    uintptr_t val;
                    __try { val = *(uintptr_t*)p; }
                    __except (EXCEPTION_EXECUTE_HANDLER) { continue; }
                    if (val != targetFn) continue;
                    ++hitCount;
                    DWORD oldProt = 0;
                    if (VirtualProtect((LPVOID)p, 8, PAGE_READWRITE, &oldProt))
                    {
                        *(void**)p = (void*)hBoneTransform;
                        VirtualProtect((LPVOID)p, 8, oldProt, &oldProt);
                        s_patchedSlots.push_back(p);
                        std::cout << "[HeadManager] Patched 0x" << std::hex << p << std::dec << "\n";
                    }
                }
            }
            if (regionEnd <= addr) break;
            addr = regionEnd;
        }
        std::cout << "[HeadManager] Scan done. Hits=" << hitCount
                  << " Patched=" << (int)s_patchedSlots.size() << "\n";
    }

    // ── per-frame: find and cache player agent ────────────────────────────────
    __try
    {
        auto* pCamMgr = g_mainHandle ? g_mainHandle->GetCameraManager() : nullptr;
        if (pCamMgr)
        {
            for (auto* pAgent : pCamMgr->m_pAgents)
            {
                if (!pAgent) continue;
                int type = *(int*)((__int64)pAgent + 0x3A4);
                if (type != 1 && type != 7) continue;
                __int64 br = *(__int64*)((__int64)pAgent + 0x1D0);
                if (!br) continue;

                static bool s_agentLogged = false;
                if (!s_agentLogged)
                {
                    s_agentLogged = true;
                    std::cout << "[HeadManager] Player agent: 0x" << std::hex
                              << (__int64)pAgent << std::dec << "\n";
                }

                s_playerAgent.store((__int64)pAgent, std::memory_order_release);
                break;
            }
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {}
}

void HeadManager::DrawUI()
{
    ImGui::Text("Hook calls:        %d", s_hookCallCount.load());
    ImGui::Text("Successful scales: %d", s_scaleSuccessCount.load());
    ImGui::Text("Vtable slots:      %d", (int)s_patchedSlots.size());

    __int64 pagent = s_playerAgent.load();
    if (pagent) ImGui::Text("Player agent:      0x%llX", (unsigned long long)pagent);
    else        ImGui::Text("Player agent:      not found");

    __int64 plocked = s_playerHookV3.load();
    if (plocked)
        ImGui::TextColored(ImVec4(0.2f,1.0f,0.2f,1.0f),
            "Locked hookV3:     0x%llX", (unsigned long long)plocked);
    else
        ImGui::TextColored(ImVec4(1.0f,0.8f,0.2f,1.0f),
            "Locked hookV3:     none (use sliders below)");

    ImGui::Separator();
    ImGui::Checkbox("Head Scale", &m_headShrinkEnabled);
    if (!m_headShrinkEnabled) return;

    // ── locked mode ──────────────────────────────────────────────────────────
    if (plocked)
    {
        ImGui::SliderFloat("Head Scale##global", &m_headScale, 0.01f, 2.0f, "%.2f");
        ImGui::TextDisabled("< 1.0 = smaller  |  > 1.0 = larger  |  1.0 = normal");
        if (ImGui::Button("Unlock Player"))
        {
            s_playerHookV3.store(0, std::memory_order_release);
            s_scaleSuccessCount.store(0, std::memory_order_relaxed);
        }
        return;
    }

    // ── identification mode: per-entity sliders ───────────────────────────────
    int n = s_entityCount.load(std::memory_order_acquire);
    if (n > MAX_ENTITIES) n = MAX_ENTITIES;

    ImGui::SameLine();
    if (ImGui::SmallButton("Clear List"))
    {
        for (int i = 0; i < MAX_ENTITIES; ++i)
        {
            s_entities[i].hookV3.store(0, std::memory_order_relaxed);
            s_entities[i].scale   = 1.0f;
            s_entities[i].enabled = false;
        }
        s_entityCount.store(0, std::memory_order_release);
        s_scaleSuccessCount.store(0, std::memory_order_relaxed);
        n = 0;
    }

    ImGui::TextDisabled("Enable one slider, drag it, watch whose head changes.");
    ImGui::TextDisabled("Then click 'Lock' on that row to activate player-only mode.");
    ImGui::Spacing();

    if (n == 0) { ImGui::TextDisabled("(no entities seen yet)"); return; }

    for (int i = 0; i < n; ++i)
    {
        __int64 v3 = s_entities[i].hookV3.load(std::memory_order_relaxed);
        if (!v3) continue;

        ImGui::PushID(i);

        // Lock button
        if (ImGui::SmallButton("Lock"))
        {
            s_playerHookV3.store(v3, std::memory_order_release);
            m_headScale = s_entities[i].scale;
            std::cout << "[HeadManager] Player locked: hookV3=0x"
                      << std::hex << v3 << std::dec << "\n";
        }
        ImGui::SameLine();

        // Enable checkbox
        ImGui::Checkbox("##en", &s_entities[i].enabled);
        ImGui::SameLine();

        // Slider labelled with address
        char lbl[48];
        snprintf(lbl, sizeof(lbl), "0x%llX", (unsigned long long)v3);
        ImGui::SetNextItemWidth(180.f);
        ImGui::SliderFloat(lbl, &s_entities[i].scale, 0.01f, 2.0f, "%.2f");

        ImGui::PopID();
    }
}
