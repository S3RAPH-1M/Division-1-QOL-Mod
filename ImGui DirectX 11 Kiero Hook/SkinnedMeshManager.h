#pragma once
#include "IniWriter.h"
#include "IniReader.h"
#include "KeybindHelper.h"

class SkinnedMeshManager
{

public:
    enum class GearType
    {
        Backpack,
        Vest,
        Gloves,
        Holster,
        Kneepads,
        Respirator,
        Hats,
        Shirts,
        Jackets,
        Pants,
        Scarfs,
        Shoes,
        Masks,
        Patches
    };

    struct ModelSwapEntry
    {
        const char* displayName;   // What the user sees
        const char* assetPath;     // Internal model path
    };

    SkinnedMeshManager();
    ~SkinnedMeshManager();


    void ApplyGearSwap(GearType type, const std::string& fromAsset, const std::string& toAsset);
    void ScanForStringThreaded(const std::string& target, SkinnedMeshManager::GearType type);
    static void ScanMemoryChunk(char* chunkStart, size_t len, const std::string& target, SkinnedMeshManager::GearType type);
    void BackpackUI();
    void DrawUI();
public:
    SkinnedMeshManager(SkinnedMeshManager const&) = delete;
    void operator=(SkinnedMeshManager const&) = delete;
};

