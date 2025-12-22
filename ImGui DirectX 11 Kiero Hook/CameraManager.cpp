#include "CameraManager.h"
#include "Snowdrop.h"
#include "Main.h"
#include "Util.h"
#include <stdio.h>
#include <iostream>
#include <iomanip>
#include "imgui/imgui.h"
#include "XorStr.hpp"

CameraManager::CameraManager()
{
    m_cameraEnabled = false;
    m_firstEnable = true;
    m_camera.pitch = 0;
    m_camera.yaw = 0;
    m_camera.roll = 0;


    CameraTrack defaultTrack;
    defaultTrack.name = "Track #1";
    defaultTrack.nodes.clear();
    m_runningId = 2;

    m_tracks.push_back(defaultTrack);
    m_trackNameList = new const char* [1];
    m_trackNameList[0] = (const char*)m_tracks[0].name.c_str();
    m_selectedTrackIndex = 0;

    m_lockToPlayer = false;
    m_selectedPlayerIndex = 0;

    m_shakeInfo.qRotations[0] = m_shakeInfo.qRotations[1] = XMQuaternionIdentity();
}

CameraManager::~CameraManager()
{
}

float quicklerp(float a, float b, float t)
{
    return a + (b - a) * t;
}

float CurrentFOV = 0.0f;
void CameraManager::CameraHook(__int64 pCamera)
{
    // Get Object Reference To the Game Camera
    TD::GameCamera* pGameCamera = (TD::GameCamera*)pCamera;

    if (CurrentFOV == 0.0f)
    {
        CurrentFOV = static_cast<float>(FovAmount);
    }

    bool isAiming = false;
    if (UseFOVZoom)
    {
        if ((GetAsyncKeyState(VK_RBUTTON) & 0x8000))
        {
            isAiming = true;
        }
    }

    float targetFOV = isAiming ? ZoomFovAmount : static_cast<float>(FovAmount);
    float deltaTime = 1.0f / 60.0f;
    float speed = static_cast<float>(ZoomSpeed) / 3;
    
    
    // FOV Smoothing
    CurrentFOV += (targetFOV - CurrentFOV) * (1.0f - expf(-speed * deltaTime));

    pGameCamera->m_FieldOfView = XMConvertToRadians(static_cast<float>(CurrentFOV));
}

void CameraManager::DrawUI()
{
    ImGui::Checkbox(xor ("FOV"), &UseFOV);
    if (UseFOV) {
        ImGui::SameLine(); ImGui::Checkbox(xor ("Zoom When Aiming"), &UseFOVZoom);
        if (UseFOVZoom) {
            ImGui::SliderInt(xor ("Zoom Speed"), &ZoomSpeed, 1, 10);
            ImGui::SliderInt(xor ("Zoom Field Of View"), &ZoomFovAmount, 1, 180);
        }

        ImGui::SliderInt(xor ("Field Of View"), &FovAmount, 55, 180);
    }
}

