#include "SkinnedMeshManager.h"
#include "Util.h"
#include "IniWriter.h"
#include "IniReader.h"
#include "Main.h"
#include "attributes.h"
#include "configGlobals.h"
#include "KeybindHelper.h"
#include "CameraManager.h"
#include "imgui/imgui.h"
#include "XorStr.hpp"
#include <Psapi.h>
#include <string>
#include <thread>
#include <mutex>
#include <windows.h>
#include <vector>
#include <atomic>
#include <algorithm>
#include <cstdio>
#include <unordered_map>

struct FoundString
{
    char* address;
    size_t length;
};

SkinnedMeshManager::SkinnedMeshManager()
{
}

SkinnedMeshManager::~SkinnedMeshManager()
{

}

static SkinnedMeshManager::ModelSwapEntry backpackModels[] =
{
    { "Ninjabike Messenger Bag", "rogue/graph objects/gear/ca_cm_b_sv_set01.mgraphobject" },
    { "Striker's Battlegear",                 "rogue/graph objects/gear/CA_CM_B_T7_R_DLC1.mgraphobject" },
    { "Striker's Battlegear Classified",      "rogue/graph objects/gear/ca_cm_b_mm_st.mgraphobject" },
    { "Predator's Mark Classified",         "rogue/graph objects/gear/ca_cm_b_pa_pr.mgraphobject" },
    { "Hunters Faith Classified",           "rogue/graph objects/gear/ca_cm_b_rt_hf.mgraphobject" },
    { "Nomad Classified",           "rogue/graph objects/gear/ca_cm_b_uc_pn.mgraphobject" },
    { "D3-FNC Classified",           "rogue/graph objects/gear/ca_cm_b_pa_d3.mgraphobject" },
    { "Lone Star Classified",           "rogue/graph objects/gear/ca_cm_b_tt_ls.mgraphobject" },
    { "Lone Star",                      "rogue/graph objects/gear/CA_CM_B_Set03_BG.mgraphobject" },
    { "Banshee Classified",      "rogue/graph objects/gear/ca_cm_b_pa_ba.mgraphobject" },
    { "Banshee",                 "rogue/graph objects/gear/ca_cm_b_uw_dar.mgraphobject" },
    { "DeadEYE Classified",      "rogue/graph objects/gear/ca_cm_b_tt_de.mgraphobject" },
    { "Sentry Call Classified",  "rogue/graph objects/gear/ca_cm_b_as_sc.mgraphobject" },
    { "Alphabridge Classified",  "rogue/graph objects/gear/ca_cm_b_rt_ab.mgraphobject" },
    { "Reclaimer Classified",    "rogue/graph objects/gear/ca_cm_b_tt_rc.mgraphobject" },
    { "FireCrest Classified",           "rogue/graph objects/gear/ca_cm_b_rt_fc.mgraphobject" },
    { "FireCrest",                      "rogue/graph objects/gear/CA_CM_B_GS_UW.mgraphobject" },
    { "Spec-ops pack",                      "rogue/graph objects/gear/CA_CM_B_T7_L.mgraphobject" },
    { "Urban assault pack",                      "rogue/graph objects/gear/CA_CM_B_T7_E.mgraphobject" },
    { "Security pack",                      "rogue/graph objects/gear/CA_CM_B_T1_R.mgraphobject" },
    { "Safety bag",                      "rogue/graph objects/gear/CA_CM_B_T1_U.mgraphobject" }
    //{ "",                      "" },
};


std::vector<FoundString> g_FoundGearStrings[static_cast<size_t>(SkinnedMeshManager::GearType::Patches) + 1];
std::mutex g_FoundGearMutex[static_cast<size_t>(SkinnedMeshManager::GearType::Patches) + 1];


std::atomic<size_t> g_RegionsTotal{ 0 };
std::atomic<size_t> g_RegionsScanned{ 0 };
std::atomic<bool>   g_ScanRunning{ false };

void LogFoundString(const char* addr, const std::string& target)
{
    printf_s(
        "[SkinnedMesh] Found \"%s\" at 0x%p (RVA: 0x%llX)\n",
        target.c_str(),
        addr,
        static_cast<unsigned long long>((uintptr_t)addr - g_pBase)
    );
}

static inline unsigned char ToLowerFast(unsigned char c)
{
    return (c >= 'A' && c <= 'Z') ? (c + 32) : c;
}

static bool IsValidAsciiString(const char* str, size_t maxLen = 512)
{
    for (size_t i = 0; i < maxLen; ++i)
    {
        unsigned char c = (unsigned char)str[i];
        if (c == '\0')
            return true; // allow early end
        if (c < 0x20 || c > 0x7E)
            return false;
    }
    return true;
}

void SkinnedMeshManager::ScanMemoryChunk(char* chunkStart, size_t len, const std::string& target, SkinnedMeshManager::GearType type)
{
    const size_t patLen = target.size();
    if (len < patLen)
        return;

    for (size_t i = 0; i <= len - patLen; ++i)
    {
        if (_strnicmp(chunkStart + i, target.c_str(), patLen) == 0)
        {
            if (!IsValidAsciiString(chunkStart + i))
                continue;

            std::lock_guard<std::mutex> lock(g_FoundGearMutex[static_cast<size_t>(type)]);

            auto& foundStrings = g_FoundGearStrings[static_cast<size_t>(type)];

            // Deduplicate.
            bool alreadyFound = false;
            for (auto& f : foundStrings)
            {
                if (f.address == chunkStart + i)
                {
                    alreadyFound = true;
                    break;
                }
            }
            if (alreadyFound)
                continue;

            foundStrings.push_back({ chunkStart + i, patLen });
        }
    }
}


void SkinnedMeshManager::ScanForStringThreaded(const std::string& target, SkinnedMeshManager::GearType type)
{
    static std::thread controller;
    if (controller.joinable())
        return;

    controller = std::thread([target, type]()
        {
            if (target.empty())
                return;

            g_ScanRunning = true;
            g_RegionsScanned = 0;

            // Clear the found strings for this gear type
            {
                std::lock_guard<std::mutex> lock(g_FoundGearMutex[static_cast<size_t>(type)]);
                g_FoundGearStrings[static_cast<size_t>(type)].clear();
            }

            auto startTime = std::chrono::steady_clock::now();

            SYSTEM_INFO sysInfo{};
            GetSystemInfo(&sysInfo);

            char* addr = (char*)sysInfo.lpMinimumApplicationAddress;
            char* end = (char*)sysInfo.lpMaximumApplicationAddress;

            MEMORY_BASIC_INFORMATION mbi{};
            std::vector<std::pair<char*, size_t>> regions;

            while (addr < end)
            {
                if (VirtualQuery(addr, &mbi, sizeof(mbi)) != sizeof(mbi))
                    break;

                if (mbi.State == MEM_COMMIT &&
                    !(mbi.Protect & PAGE_GUARD) &&
                    !(mbi.Protect & PAGE_NOACCESS) &&
                    (mbi.Protect & (PAGE_READONLY | PAGE_READWRITE | PAGE_WRITECOPY |
                        PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE |
                        PAGE_EXECUTE_WRITECOPY)))
                {
                    if ((HMODULE)mbi.AllocationBase != g_ModModule)
                    {
                        regions.emplace_back((char*)mbi.BaseAddress, mbi.RegionSize);
                    }
                }

                addr += mbi.RegionSize;
            }

            g_RegionsTotal = regions.size();

            // --------------------------------------------------------
            // Monitor thread
            // --------------------------------------------------------
            std::thread monitor([startTime]()
                {
                    while (g_ScanRunning.load())
                    {
                        size_t done = g_RegionsScanned.load();
                        size_t total = g_RegionsTotal.load();

                        auto now = std::chrono::steady_clock::now();
                        auto sec = std::chrono::duration_cast<std::chrono::seconds>(now - startTime).count();

                        if (total > 0)
                        {
                            int percent = int((done * 100) / total);
                            printf_s("\r[SkinnedMesh] %3d%% | %zu/%zu regions | %llds",
                                percent, done, total, (long long)sec);
                            fflush(stdout);
                        }

                        Sleep(500);
                    }
                });

            // --------------------------------------------------------
            // Worker threads
            // --------------------------------------------------------
            const size_t chunkSize = 1 << 20;
            const size_t overlap = target.size() - 1;

            std::atomic<size_t> regionIndex{ 0 };

            unsigned int threadCount = std::thread::hardware_concurrency();
            if (threadCount == 0)
                threadCount = 1;

            std::vector<std::thread> workers;
            workers.reserve(threadCount);

            for (unsigned int t = 0; t < threadCount; ++t)
            {
                workers.emplace_back([&regions, &regionIndex, chunkSize, overlap, target, type]()
                    {
                        while (true)
                        {
                            size_t index = regionIndex.fetch_add(1);
                            if (index >= regions.size())
                                break;

                            char* base = regions[index].first;
                            size_t size = regions[index].second;

                            for (size_t offset = 0; offset < size; offset += chunkSize - overlap)
                            {
                                size_t len = chunkSize;
                                if (offset + len > size)
                                    len = size - offset;

                                __try
                                {
                                    SkinnedMeshManager::ScanMemoryChunk(base + offset, len, target, type);
                                }
                                __except (EXCEPTION_EXECUTE_HANDLER)
                                {
                                    //printf_s("\nMemory Unreadable! Skipping...");
                                }
                            }

                            g_RegionsScanned.fetch_add(1);
                        }
                    });
            }

            for (auto& w : workers)
                w.join();

            g_ScanRunning = false;
            monitor.join();

            printf_s("\n");

            // Log found strings for this gear type
            {
                std::lock_guard<std::mutex> lock(g_FoundGearMutex[static_cast<size_t>(type)]);
                auto& foundStrings = g_FoundGearStrings[static_cast<size_t>(type)];

                for (auto& f : foundStrings)
                    LogFoundString(f.address, target);

                auto endTime = std::chrono::steady_clock::now();
                auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();

                printf_s("[SkinnedMesh] Scan complete in %lld ms. %zu result(s) found.\n",
                    (long long)ms, foundStrings.size());
            }
        });

    controller.detach();
}


void SkinnedMeshManager::ApplyGearSwap(GearType type, const std::string& fromAsset, const std::string& toAsset)
{
    // Cast GearType to size_t for indexing arrays
    size_t typeIndex = static_cast<size_t>(type);

    std::lock_guard<std::mutex> lock(g_FoundGearMutex[typeIndex]);
    auto& foundStrings = g_FoundGearStrings[typeIndex];

    if (foundStrings.empty())
    {
        printf_s("[SkinnedMesh] No strings found to replace for \"%s\"\n", fromAsset.c_str());
        return;
    }

    size_t replacedCount = 0;

    for (auto& found : foundStrings)
    {
        // Only replace if the memory matches the fromAsset
        if (_strnicmp(found.address, fromAsset.c_str(), found.length) != 0)
            continue;

        size_t oldLen = found.length + 1; // include null terminator
        size_t newLen = toAsset.size() + 1;

        // Change memory protection to allow writing
        DWORD oldProtect;
        if (!VirtualProtect(found.address, oldLen, PAGE_EXECUTE_READWRITE, &oldProtect))
        {
            printf_s("[SkinnedMesh] Failed to change memory protection at 0x%p\n", found.address);
            continue;
        }

        // Allocate new memory block for resized string
        char* newMem = (char*)VirtualAlloc(nullptr, newLen, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        if (!newMem)
        {
            printf_s("[SkinnedMesh] Failed to allocate memory for \"%s\"\n", toAsset.c_str());
            VirtualProtect(found.address, oldLen, oldProtect, &oldProtect);
            continue;
        }

        // Copy the new string into the new memory
        strcpy_s(newMem, newLen, toAsset.c_str());

        // Overwrite original memory with new string
        // Copy min(oldLen, newLen) first
        size_t copyLen = oldLen < newLen ? oldLen : newLen;
        memcpy(found.address, newMem, copyLen);

        // If new string is bigger than old memory, overwrite remaining bytes
        if (newLen > oldLen)
        {
            memcpy(found.address, newMem, newLen);
        }

        VirtualProtect(found.address, oldLen, oldProtect, &oldProtect);

        replacedCount++;
        printf_s("[SkinnedMesh] Resized and replaced \"%s\" with \"%s\" at 0x%p\n",
            fromAsset.c_str(), toAsset.c_str(), found.address);

        // VirtualFree(newMem, 0, MEM_RELEASE);
    }

    if (replacedCount == 0)
        printf_s("[SkinnedMesh] No matches found to replace for \"%s\"\n", fromAsset.c_str());
    else
        printf_s("[SkinnedMesh] Successfully replaced %zu string(s).\n", replacedCount);
}



void SkinnedMeshManager::BackpackUI()
{
    static int fromIndex = 0;
    static int toIndex = 1;

    ImGui::Text("Backpack Skin");

    ImGui::PushItemWidth(260);

    if (ImGui::BeginCombo("##BackpackFrom", backpackModels[fromIndex].displayName))
    {
        for (int i = 0; i < IM_ARRAYSIZE(backpackModels); i++)
        {
            bool selected = (fromIndex == i);
            if (ImGui::Selectable(backpackModels[i].displayName, selected))
                fromIndex = i;

            if (selected)
                ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    ImGui::SameLine();
    ImGui::Text(">>");
    ImGui::SameLine();

    if (ImGui::BeginCombo("##BackpackTo", backpackModels[toIndex].displayName))
    {
        for (int i = 0; i < IM_ARRAYSIZE(backpackModels); i++)
        {
            bool selected = (toIndex == i);
            if (ImGui::Selectable(backpackModels[i].displayName, selected))
                toIndex = i;

            if (selected)
                ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    ImGui::PopItemWidth();

    if (fromIndex == toIndex)
    {
        ImGui::SameLine();
        ImGui::TextDisabled("(same item)");
    }

    static bool scanned = false;

    if (ImGui::Button("Apply Swap"))
    {
        const auto& from = backpackModels[fromIndex];
        const auto& to = backpackModels[toIndex];

        if (!scanned)
        {
            scanned = true;
            ScanForStringThreaded(from.assetPath, GearType::Backpack);
        }
    }

    // Poll scan status every frame
    if (scanned && !g_ScanRunning && !g_FoundGearStrings[static_cast<size_t>(GearType::Backpack)].empty())
    {
        const auto& from = backpackModels[fromIndex];
        const auto& to = backpackModels[toIndex];

        ApplyGearSwap(GearType::Backpack, from.assetPath, to.assetPath);

        scanned = false;
    }
    else if (scanned && g_ScanRunning)
    {
        ImGui::Text("Scanning memory, please wait...");
    }


}

void SkinnedMeshManager::DrawUI()
{
    BackpackUI();
}




