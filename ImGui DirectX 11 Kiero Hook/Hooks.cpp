#include "Main.h"
#include "Snowdrop.h"
#include "Util.h"
#include <Windows.h>
#include <iostream>

using namespace util;

typedef HRESULT(WINAPI* tD3D11Present)(IDXGISwapChain*, UINT, UINT);
typedef int(__fastcall* tCameraUpdate)(__int64, __int64);
typedef int(__fastcall* tDOFUpdate)(__int64, __int64);
typedef __int64(__fastcall* tUIRootUpdate)(__int64, __int64);
typedef void(__fastcall* tRClientUpdate)(__int64);
typedef char(__fastcall* tRendererResize)(__int64 a1);

tCameraUpdate oCameraUpdate = nullptr;
tCameraUpdate oCameraUpdate2 = nullptr;
tD3D11Present oD3D11Present = nullptr;
tUIRootUpdate oUIRootUpdate = nullptr;
tDOFUpdate oDOFUpdate = nullptr;
tRClientUpdate oRClientUpdate = nullptr;
tRendererResize oRendererResize = nullptr;


int __fastcall hCameraUpdate(__int64 pCamera, __int64 pTransform)
{
    int result = oCameraUpdate(pCamera, pTransform);
    g_mainHandle->GetCameraManager()->CameraHook(pCamera);
    return result;
}

int __fastcall hCameraUpdate2(__int64 pCamera, __int64 pTransform)
{
    int result = oCameraUpdate2(pCamera, pTransform);
    g_mainHandle->GetCameraManager()->CameraHook(pCamera);
    return result;
}

void __fastcall hRClientUpdate(__int64 a1)
{
    TD::RogueClient* pRClient = TD::RogueClient::Singleton();

    if (menu_key_pressed)
    {
        TD::ShowMouse(true);
    }

    oRClientUpdate(a1);
}

__int64 __fastcall hUIRootUpdate(__int64 a1, __int64 a2)
{
    if (g_gameUIDisabled)
    {
        return 0;
    }
   
    return oUIRootUpdate(a1, a2);
}

namespace
{
    struct VirtualHook
    {
        PVOID oFunc{ nullptr };
        PVOID hFunc{ nullptr };
        PVOID* ppFunc{ nullptr };
    };

    std::vector<VirtualHook> m_hooks;

    template <typename T1, typename T2>
    void HookVTableFunction(T1* ppVtable, unsigned int index, PVOID hook, T2& original)
    {
        PVOID* pVtable = *(PVOID**)ppVtable;
        LPVOID ppFunction = (LPVOID)(&pVtable[index]);

        DWORD dwProtection = 0;
        BOOL result = VirtualProtect(ppFunction, 0x8, PAGE_EXECUTE_READWRITE, &dwProtection);
        if (!result)
        {
            return;
        }

        original = (T2)pVtable[index];
        pVtable[index] = hook;

        VirtualProtect(ppFunction, 0x8, dwProtection, &dwProtection);

        VirtualHook newHook;
        newHook.ppFunc = &pVtable[index];
        newHook.oFunc = original;
        newHook.hFunc = hook;

        m_hooks.push_back(newHook);
        return;
    }

}


void hooks::Init()
{
    std::cout << "Initializing Hooks...\n";

    TD::GameCamera* pGameCamera = TD::RogueClient::Singleton()->m_pClient->m_pWorld->m_pCameraManager->m_pCamera1;
    TD::GameCamera* pGameCamera2 = TD::RogueClient::Singleton()->m_pClient->m_pWorld->m_pCameraManager->m_pCamera2;
    __int64 pUIRootVTable = g_pBase + 0x33731C8;


    HookVTableFunction(pGameCamera, 4, hCameraUpdate, oCameraUpdate);
    std::cout << "cameraUpdate is Hooked!\n";
    HookVTableFunction(pGameCamera2, 4, hCameraUpdate2, oCameraUpdate2);
    std::cout << "cameraUpdate2 is Hooked!\n";
    HookVTableFunction(TD::RogueClient::Singleton(), 5, hRClientUpdate, oRClientUpdate);
    std::cout << "rClientUpdate is Hooked!\n";
    HookVTableFunction(&pUIRootVTable, 0, hUIRootUpdate, oUIRootUpdate);
    std::cout << "uiRootUpdate is Hooked!\n";
}

void hooks::DisableHooks()
{
    for (auto itr = m_hooks.begin(); itr != m_hooks.end(); ++itr)
    {
        *itr->ppFunc = itr->oFunc;
    }
}

void hooks::EnableHooks()
{
    for (auto itr = m_hooks.begin(); itr != m_hooks.end(); ++itr)
    {
        *itr->ppFunc = itr->hFunc;
    }
}