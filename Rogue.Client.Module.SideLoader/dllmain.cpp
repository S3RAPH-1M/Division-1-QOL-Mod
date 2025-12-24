// dllmain.cpp : Defines the entry point for the DLL application.
#include "pch.h"
#include "proxy_version.hpp"
#include <string>

DWORD WINAPI LoadModuleThread(LPVOID param)
{
    HMODULE hModule = static_cast<HMODULE>(param);

    char path[MAX_PATH];
    GetModuleFileNameA(hModule, path, MAX_PATH);

    std::string dllPath(path);
    size_t pos = dllPath.find_last_of("\\/");
    dllPath = dllPath.substr(0, pos + 1);

    dllPath += "Division_QOL_Tools.dll";

    LoadLibraryA(dllPath.c_str());
    return 0;
}

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                     )
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:

       init_original_version();
       CreateThread(nullptr, 0, LoadModuleThread, hModule, 0, nullptr);

    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}

