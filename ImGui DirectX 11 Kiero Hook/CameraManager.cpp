#include "CameraManager.h"
#include "Snowdrop.h"
#include "Main.h"
#include "Util.h"
#include <stdio.h>
#include <iostream>
#include <iomanip>

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

void CameraManager::CameraHook(__int64 pCamera)
{
    TD::GameCamera* pGameCamera = (TD::GameCamera*)pCamera;
    pGameCamera->m_FieldOfView = XMConvertToRadians(static_cast<float>(fovAmount));
}

