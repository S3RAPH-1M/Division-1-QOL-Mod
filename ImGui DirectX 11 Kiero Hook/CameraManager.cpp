#include "CameraManager.h"
#include "Snowdrop.h"
#include "Main.h"
#include "Util.h"
#include <stdio.h>
#include <iostream>
#include <iomanip>
#include "imgui/imgui.h"
#include "XorStr.hpp"
#include "KeybindHelper.h"

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

void CameraManager::UpdatePlayerList()
{
    m_pAgents.clear();

    TD::World* pWorld = TD::RogueClient::Singleton()->m_pClient->m_pWorld;
    if (pWorld)
    {
        if (pWorld->m_AgentArray && pWorld->m_AgentCount > 0)
        {
            for (int i = 0; i < pWorld->m_AgentCount; ++i)
            {
                TD::Agent* pAgent = pWorld->m_AgentArray[i];
                m_pAgents.push_back(pAgent);
            }
        }
    }

    const char** newAgentNames = new const char* [m_pAgents.size()];
    for (int i = 0; i < m_pAgents.size(); ++i)
        newAgentNames[i] = (const char*)(&m_pAgents[i]->m_Info->m_Name);

    const char** oldNames = m_playerList;
    m_playerList = newAgentNames;
    m_playerCount = m_pAgents.size();

    delete[] oldNames;
}

float quicklerp(float a, float b, float t)
{
    return a + (b - a) * t;
}

float CurrentFOV = 0.0f;
float boneOffsetX = 0.0f; // Right
float boneOffsetY = 0.0f; // Up
float boneOffsetZ = 0.0f; // Forward
void CameraManager::CameraHook(__int64 pCamera)
{
    // Get Object Reference To the Game Camera
    TD::GameCamera* pGameCamera = (TD::GameCamera*)pCamera;

    if(useFirstPerson)
    {
        if (!m_selectedPlayerIndex)
        {
            XMMATRIX targetMatrix = m_pAgents[m_selectedPlayerIndex]->GetHeadBoneMatrix();
            XMMATRIX rotationMatrix = XMMatrixRotationRollPitchYaw(XMConvertToRadians(m_camera.pitch), XMConvertToRadians(m_camera.yaw), 0);
            XMMATRIX rollMatrix = XMMatrixRotationRollPitchYaw(0, 0, XMConvertToRadians(m_camera.roll));
            rotationMatrix = XMMatrixMultiply(rollMatrix, rotationMatrix);

            XMVECTOR cameraPos = targetMatrix.r[3];
            cameraPos += m_camera.position.m128_f32[0] * targetMatrix.r[0];
            cameraPos += m_camera.position.m128_f32[1] * targetMatrix.r[1];
            cameraPos += m_camera.position.m128_f32[2] * targetMatrix.r[2];

            cameraPos += (boneOffsetX / 100) * targetMatrix.r[0];
            cameraPos += (boneOffsetY / 100) * targetMatrix.r[1];
            cameraPos += (boneOffsetZ / 100) * targetMatrix.r[2];

            targetMatrix.r[0] = XMVectorScale(targetMatrix.r[0], 0.0001f);
            targetMatrix.r[1] = XMVectorScale(targetMatrix.r[1], 0.0001f);
            targetMatrix.r[2] = XMVectorScale(targetMatrix.r[2], 0.0001f);

            pGameCamera->m_Transform.r[3] = cameraPos;
        }
        

    }

    if (CurrentFOV == 0.0f)
    {
        CurrentFOV = static_cast<float>(FovAmount);
    }

    bool isAiming = false;
    if (UseFOVZoom)
    {
        if (!ZoomKey.keys.empty()) 
        {
            if (KeybindHelper::IsKeyBindPressed(ZoomKey))
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

void CameraManager::Update()
{
    UpdatePlayerList();
}


KeyBind ZoomKey;
bool g_gameUIDisabled;
void CameraManager::DrawUI()
{
    ImGui::Checkbox(xor ("FOV"), &UseFOV);
    if (UseFOV) {
        ImGui::SameLine(); ImGui::Checkbox(xor ("Zoom When Aiming"), &UseFOVZoom);
        if (UseFOVZoom) {
            KeybindHelper::DrawKeyBindButton(xor ("Aim Button"), ZoomKey);
            ImGui::SliderInt(xor ("Zoom Speed"), &ZoomSpeed, 1, 10);
            ImGui::SliderInt(xor ("Zoom Field Of View"), &ZoomFovAmount, 1, 180);
        }

        ImGui::SliderInt(xor ("Field Of View"), &FovAmount, 55, 180);


        ImGui::Checkbox(xor ("Disable HUD / UI"), &g_gameUIDisabled);
        //ImGui::Combo("##PlayerList", &m_selectedPlayerIndex, m_playerList, m_playerCount); // for debugging. do not uncomment unless u are filthy cheater!
    }

    ImGui::Checkbox(xor ("Use First Person View"), &useFirstPerson);
    if (useFirstPerson)
    {
        ImGui::SliderFloat("Offset X (Right)", &boneOffsetX, -20.0f, 20.0f, NULL);
        ImGui::SliderFloat("Offset Y (Up)", &boneOffsetY, -20.0f, 20.0f, NULL);
        ImGui::SliderFloat("Offset Z (Forward)", &boneOffsetZ, -20.0f, 20.0f, NULL);
    }
}

