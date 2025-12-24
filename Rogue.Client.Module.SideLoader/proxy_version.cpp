#include <windows.h>
#include "pch.h"
#include <stdio.h>

HMODULE g_hversion;

// ================= Typedefs =================
using GetFileVersionInfoAFn = BOOL(__stdcall*)(void*);
using GetFileVersionInfoByHandleFn = BOOL(__stdcall*)(void*);
using GetFileVersionInfoExAFn = BOOL(__stdcall*)(void*);
using GetFileVersionInfoExWFn = BOOL(__stdcall*)(void*);
using GetFileVersionInfoSizeAFn = BOOL(__stdcall*)(void*);
using GetFileVersionInfoSizeExAFn = BOOL(__stdcall*)(void*);
using GetFileVersionInfoSizeExWFn = BOOL(__stdcall*)(void*);
using GetFileVersionInfoSizeWFn = BOOL(__stdcall*)(void*);
using GetFileVersionInfoWFn = BOOL(__stdcall*)(void*);
using VerFindFileAFn = BOOL(__stdcall*)(void*);
using VerFindFileWFn = BOOL(__stdcall*)(void*);
using VerInstallFileAFn = BOOL(__stdcall*)(void*);
using VerInstallFileWFn = BOOL(__stdcall*)(void*);
using VerLanguageNameAFn = BOOL(__stdcall*)(void*);
using VerLanguageNameWFn = BOOL(__stdcall*)(void*);
using VerQueryValueAFn = BOOL(__stdcall*)(void*);
using VerQueryValueWFn = BOOL(__stdcall*)(void*);

// ================= Original pointers =================
GetFileVersionInfoAFn oGetFileVersionInfoA;
GetFileVersionInfoByHandleFn oGetFileVersionInfoByHandle;
GetFileVersionInfoExAFn oGetFileVersionInfoExA;
GetFileVersionInfoExWFn oGetFileVersionInfoExW;
GetFileVersionInfoSizeAFn oGetFileVersionInfoSizeA;
GetFileVersionInfoSizeExAFn oGetFileVersionInfoSizeExA;
GetFileVersionInfoSizeExWFn oGetFileVersionInfoSizeExW;
GetFileVersionInfoSizeWFn oGetFileVersionInfoSizeW;
GetFileVersionInfoWFn oGetFileVersionInfoW;
VerFindFileAFn oVerFindFileA;
VerFindFileWFn oVerFindFileW;
VerInstallFileAFn oVerInstallFileA;
VerInstallFileWFn oVerInstallFileW;
VerLanguageNameAFn oVerLanguageNameA;
VerLanguageNameWFn oVerLanguageNameW;
VerQueryValueAFn oVerQueryValueA;
VerQueryValueWFn oVerQueryValueW;

// ================= Hooks =================
extern "C"
{
	#pragma comment(linker, "/export:GetFileVersionInfoA=hkGetFileVersionInfoA,@1")
	BOOL __stdcall hkGetFileVersionInfoA(void* a1)
	{
		auto result = oGetFileVersionInfoA(a1);
		printf_s("GetFileVersionInfoA called\n");
		return result;
	}

	#pragma comment(linker, "/export:GetFileVersionInfoByHandle=hkGetFileVersionInfoByHandle,@2")
	BOOL __stdcall hkGetFileVersionInfoByHandle(void* a1)
	{
		auto result = oGetFileVersionInfoByHandle(a1);
		printf_s("GetFileVersionInfoByHandle called\n");
		return result;
	}

	#pragma comment(linker, "/export:GetFileVersionInfoExA=hkGetFileVersionInfoExA,@3")
	BOOL __stdcall hkGetFileVersionInfoExA(void* a1)
	{
		auto result = oGetFileVersionInfoExA(a1);
		printf_s("GetFileVersionInfoExA called\n");
		return result;
	}

	#pragma comment(linker, "/export:GetFileVersionInfoExW=hkGetFileVersionInfoExW,@4")
	BOOL __stdcall hkGetFileVersionInfoExW(void* a1)
	{
		auto result = oGetFileVersionInfoExW(a1);
		printf_s("GetFileVersionInfoExW called\n");
		return result;
	}

	#pragma comment(linker, "/export:GetFileVersionInfoSizeA=hkGetFileVersionInfoSizeA,@5")
	BOOL __stdcall hkGetFileVersionInfoSizeA(void* a1)
	{
		auto result = oGetFileVersionInfoSizeA(a1);
		printf_s("GetFileVersionInfoSizeA called\n");
		return result;
	}

	#pragma comment(linker, "/export:GetFileVersionInfoSizeExA=hkGetFileVersionInfoSizeExA,@6")
	BOOL __stdcall hkGetFileVersionInfoSizeExA(void* a1)
	{
		auto result = oGetFileVersionInfoSizeExA(a1);
		printf_s("GetFileVersionInfoSizeExA called\n");
		return result;
	}

	#pragma comment(linker, "/export:GetFileVersionInfoSizeExW=hkGetFileVersionInfoSizeExW,@7")
	BOOL __stdcall hkGetFileVersionInfoSizeExW(void* a1)
	{
		auto result = oGetFileVersionInfoSizeExW(a1);
		printf_s("GetFileVersionInfoSizeExW called\n");
		return result;
	}

	#pragma comment(linker, "/export:GetFileVersionInfoSizeW=hkGetFileVersionInfoSizeW,@8")
	BOOL __stdcall hkGetFileVersionInfoSizeW(void* a1)
	{
		auto result = oGetFileVersionInfoSizeW(a1);
		printf_s("GetFileVersionInfoSizeW called\n");
		return result;
	}

	#pragma comment(linker, "/export:GetFileVersionInfoW=hkGetFileVersionInfoW,@9")
	BOOL __stdcall hkGetFileVersionInfoW(void* a1)
	{
		auto result = oGetFileVersionInfoW(a1);
		printf_s("GetFileVersionInfoW called\n");
		return result;
	}

	#pragma comment(linker, "/export:VerFindFileA=hkVerFindFileA,@10")
	BOOL __stdcall hkVerFindFileA(void* a1)
	{
		auto result = oVerFindFileA(a1);
		printf_s("VerFindFileA called\n");
		return result;
	}

	#pragma comment(linker, "/export:VerFindFileW=hkVerFindFileW,@11")
	BOOL __stdcall hkVerFindFileW(void* a1)
	{
		auto result = oVerFindFileW(a1);
		printf_s("VerFindFileW called\n");
		return result;
	}

	#pragma comment(linker, "/export:VerInstallFileA=hkVerInstallFileA,@12")
	BOOL __stdcall hkVerInstallFileA(void* a1)
	{
		auto result = oVerInstallFileA(a1);
		printf_s("VerInstallFileA called\n");
		return result;
	}

	#pragma comment(linker, "/export:VerInstallFileW=hkVerInstallFileW,@13")
	BOOL __stdcall hkVerInstallFileW(void* a1)
	{
		auto result = oVerInstallFileW(a1);
		printf_s("VerInstallFileW called\n");
		return result;
	}

	#pragma comment(linker, "/export:VerLanguageNameA=hkVerLanguageNameA,@14")
	BOOL __stdcall hkVerLanguageNameA(void* a1)
	{
		auto result = oVerLanguageNameA(a1);
		printf_s("VerLanguageNameA called\n");
		return result;
	}

	#pragma comment(linker, "/export:VerLanguageNameW=hkVerLanguageNameW,@15")
	BOOL __stdcall hkVerLanguageNameW(void* a1)
	{
		auto result = oVerLanguageNameW(a1);
		printf_s("VerLanguageNameW called\n");
		return result;
	}

	#pragma comment(linker, "/export:VerQueryValueA=hkVerQueryValueA,@16")
	BOOL __stdcall hkVerQueryValueA(void* a1)
	{
		auto result = oVerQueryValueA(a1);
		printf_s("VerQueryValueA called\n");
		return result;
	}

	#pragma comment(linker, "/export:VerQueryValueW=hkVerQueryValueW,@17")
	BOOL __stdcall hkVerQueryValueW(void* a1)
	{
		auto result = oVerQueryValueW(a1);
		printf_s("VerQueryValueW called\n");
		return result;
	}
}

void init_original_version()
{
	if (g_hversion) return;

	g_hversion = LoadLibraryA("C:\\Windows\\System32\\version.dll");
	if (!g_hversion) return;

	oGetFileVersionInfoA = reinterpret_cast<GetFileVersionInfoAFn>(
		GetProcAddress(g_hversion, "GetFileVersionInfoA")
	);

	oGetFileVersionInfoByHandle = reinterpret_cast<GetFileVersionInfoByHandleFn>(
		GetProcAddress(g_hversion, "GetFileVersionInfoByHandle")
	);

	oGetFileVersionInfoExA = reinterpret_cast<GetFileVersionInfoExAFn>(
		GetProcAddress(g_hversion, "GetFileVersionInfoExA")
	);

	oGetFileVersionInfoExW = reinterpret_cast<GetFileVersionInfoExWFn>(
		GetProcAddress(g_hversion, "GetFileVersionInfoExW")
	);

	oGetFileVersionInfoSizeA = reinterpret_cast<GetFileVersionInfoSizeAFn>(
		GetProcAddress(g_hversion, "GetFileVersionInfoSizeA")
	);

	oGetFileVersionInfoSizeExA = reinterpret_cast<GetFileVersionInfoSizeExAFn>(
		GetProcAddress(g_hversion, "GetFileVersionInfoSizeExA")
	);

	oGetFileVersionInfoSizeExW = reinterpret_cast<GetFileVersionInfoSizeExWFn>(
		GetProcAddress(g_hversion, "GetFileVersionInfoSizeExW")
	);

	oGetFileVersionInfoSizeW = reinterpret_cast<GetFileVersionInfoSizeWFn>(
		GetProcAddress(g_hversion, "GetFileVersionInfoSizeW")
	);

	oGetFileVersionInfoW = reinterpret_cast<GetFileVersionInfoWFn>(
		GetProcAddress(g_hversion, "GetFileVersionInfoW")
	);

	oVerFindFileA = reinterpret_cast<VerFindFileAFn>(
		GetProcAddress(g_hversion, "VerFindFileA")
	);

	oVerFindFileW = reinterpret_cast<VerFindFileWFn>(
		GetProcAddress(g_hversion, "VerFindFileW")
	);

	oVerInstallFileA = reinterpret_cast<VerInstallFileAFn>(
		GetProcAddress(g_hversion, "VerInstallFileA")
	);

	oVerInstallFileW = reinterpret_cast<VerInstallFileWFn>(
		GetProcAddress(g_hversion, "VerInstallFileW")
	);

	oVerLanguageNameA = reinterpret_cast<VerLanguageNameAFn>(
		GetProcAddress(g_hversion, "VerLanguageNameA")
	);

	oVerLanguageNameW = reinterpret_cast<VerLanguageNameWFn>(
		GetProcAddress(g_hversion, "VerLanguageNameW")
	);

	oVerQueryValueA = reinterpret_cast<VerQueryValueAFn>(
		GetProcAddress(g_hversion, "VerQueryValueA")
	);

	oVerQueryValueW = reinterpret_cast<VerQueryValueWFn>(
		GetProcAddress(g_hversion, "VerQueryValueW")
	);
}
