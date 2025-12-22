#pragma once
#include <d3d11.h>
#include <DirectXMath.h>
#include <dxgi.h>
#include <Windows.h>

#include "Util.h"

using namespace DirectX;

extern uintptr_t g_pBase;

namespace TD
{
    class Agent;
    class AgentInfo;
    class CameraManager;
    class Client;
    class EnvironmentManager;
    class GameCamera;
    class GameRenderer;
    class MouseInput;
    class RogueClient;
    class World;

    class Agent
    {
    public:
        BYTE Pad000[0x28];
        AgentInfo* m_Info;
        BYTE Pad030[0x10];
        XMMATRIX m_Transform;

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
    };

    class AgentInfo
    {
    public:
        BYTE Pad000[0x10];
        char m_Name;
    };

    class CameraManager
    {
    public:
        BYTE Pad000[0x18];
        GameCamera* m_pCamera1;
        GameCamera* m_pCamera2;
    };

    class Client
    {
    public:
        BYTE Pad000[0x28];
        World* m_pWorld;
        MouseInput* m_pMouseInput;
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
        tGetValue GetValue = (tGetValue)(g_pBase + 0xC8DC8F); // B9 ? ? ? ? 48 89 5C 24 ? E8 ? ? ? ? 48 8B D8 48 85 C0 74 ? 48 8B CF
        TD::Client* pClient = TD::RogueClient::Singleton()->m_pClient;
        __int64 pValueStoreThingy = *(__int64*)((__int64)pClient + 0x38);
        __int64 donutcare = 0;
        __int64 pValueStore = *GetValue(pValueStoreThingy, &donutcare, "KB_SHOW_MOUSE", 0);

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