#pragma once
#include <windows.h>
#include <iostream>
#include <MinHook.h>
#include "../common/protocol.h"

namespace PuppetSpawner {

// Factory VFunc prototypes discovered in reverse engineering
// CGameObjectFactory::CreateObject (VFunc 0 at VA 0x142232B50 / RVA 0x02232B50)
typedef uintptr_t(__fastcall* PFN_CreateGameObject)(uintptr_t pFactory, uint32_t typeHash, uintptr_t pSpawnParams);
static PFN_CreateGameObject oCreateGameObject = nullptr;

// CCloneFactory::CloneEntity (VFunc 0 at VA 0x1424400E0 / RVA 0x024400E0)
typedef uintptr_t(__fastcall* PFN_CloneEntity)(uintptr_t pCloneFactory, uintptr_t pSourceEntity, float* pNewTransform);
static PFN_CloneEntity oCloneEntity = nullptr;

// CCallSignatureVehicle::Execute (VFunc 0 at VA 0x1424B8B50 / RVA 0x024B8B50)
typedef uintptr_t(__fastcall* PFN_CallSignatureVehicle)(uintptr_t pCaller, uintptr_t pParam1);
static PFN_CallSignatureVehicle oCallSignatureVehicle = nullptr;

// CVehicleManager::VFunc 0 (VA 0x1424561E0 / RVA 0x024561E0)
typedef uintptr_t(__fastcall* PFN_VehicleManagerVFunc0)(uintptr_t pManager, uintptr_t pParam1);
static PFN_VehicleManagerVFunc0 oVehicleManagerVFunc0 = nullptr;

static uintptr_t g_pCloneFactoryInstance = 0;
static uintptr_t g_pSignatureVehicleCaller = 0;
static uintptr_t g_pVehicleManagerInstance = 0;
static uintptr_t g_spawnedPuppetEntity = 0;
static bool g_bPuppetSpawned = false;

inline uintptr_t Hooked_CloneEntity(uintptr_t pCloneFactory, uintptr_t pSourceEntity, float* pNewTransform) {
    g_pCloneFactoryInstance = pCloneFactory;
    return oCloneEntity(pCloneFactory, pSourceEntity, pNewTransform);
}

inline uintptr_t Hooked_CallSignatureVehicle(uintptr_t pCaller, uintptr_t pParam1) {
    g_pSignatureVehicleCaller = pCaller;
    return oCallSignatureVehicle(pCaller, pParam1);
}

inline uintptr_t Hooked_VehicleManagerVFunc0(uintptr_t pManager, uintptr_t pParam1) {
    g_pVehicleManagerInstance = pManager;
    return oVehicleManagerVFunc0(pManager, pParam1);
}

inline void InitializeSpawnerHooks(uintptr_t imageBase) {
    uintptr_t pFnClone = imageBase + 0x024400E0; // CCloneFactory VFunc 0
    if (MH_CreateHook((LPVOID)pFnClone, &Hooked_CloneEntity, (LPVOID*)&oCloneEntity) == MH_OK) {
        MH_EnableHook((LPVOID)pFnClone);
    }

    uintptr_t pFnCallSig = imageBase + 0x024B8B50; // CCallSignatureVehicle VFunc 0
    if (MH_CreateHook((LPVOID)pFnCallSig, &Hooked_CallSignatureVehicle, (LPVOID*)&oCallSignatureVehicle) == MH_OK) {
        MH_EnableHook((LPVOID)pFnCallSig);
    }

    uintptr_t pFnVehMgr = imageBase + 0x024561E0; // CVehicleManager VFunc 0
    if (MH_CreateHook((LPVOID)pFnVehMgr, &Hooked_VehicleManagerVFunc0, (LPVOID*)&oVehicleManagerVFunc0) == MH_OK) {
        MH_EnableHook((LPVOID)pFnVehMgr);
    }
}

// Spawns puppet entity by cloning local player/vehicle actor or triggering signature vehicle spawner
inline bool SpawnPuppetAt(uintptr_t pLocalPlayerEntity, float posX, float posY, float posZ) {
    if (g_pSignatureVehicleCaller && oCallSignatureVehicle) {
        oCallSignatureVehicle(g_pSignatureVehicleCaller, 0);
        g_bPuppetSpawned = true;
        return true;
    }

    if (!g_pCloneFactoryInstance || !pLocalPlayerEntity || !oCloneEntity) return false;

    float spawnTransform[16] = {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        posX, posY, posZ, 1.0f
    };

    g_spawnedPuppetEntity = oCloneEntity(g_pCloneFactoryInstance, pLocalPlayerEntity, spawnTransform);
    if (g_spawnedPuppetEntity) {
        g_bPuppetSpawned = true;
        return true;
    }
    return false;
}

// Updates spawned puppet position every frame from received network state
inline void SetPuppetPosition(float posX, float posY, float posZ) {
    if (!g_bPuppetSpawned || !g_spawnedPuppetEntity) return;

    // In Avalanche Apex Engine, Entity position vector is located at offset 0x50 / 0x60
    float* pMatrix = reinterpret_cast<float*>(g_spawnedPuppetEntity + 0x50);
    pMatrix[0] = posX;
    pMatrix[1] = posY;
    pMatrix[2] = posZ;
}

}
