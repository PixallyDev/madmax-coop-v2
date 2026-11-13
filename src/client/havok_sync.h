#pragma once
#include <windows.h>
#include <cmath>
#include <vector>
#include <iostream>
#include "../common/protocol.h"

namespace HavokSync {

struct HavokTransform {
    float rot[3][3];    // 3x3 rotation matrix
    float pos[3];       // position vector (X, Y, Z)
    float vel[3];       // velocity vector (m/s)
    float yaw;          // Facing angle in radians
    float pitch;        // Vertical tilt in radians
    float speed;        // Speed in m/s
    bool isLocked;
};

static uintptr_t g_pHavokMotionInstance = 0;
static HavokTransform g_localTransform = { 0 };
static uint8_t s_scanBuffer[64 * 1024]; // Static buffer: zero dynamic heap allocation

// Scan committed memory across 64-bit address space for active Wasteland Havok / Player entities
inline bool FindHavokMotion(uintptr_t gameBase) {
    uintptr_t vmtHavokMotion = gameBase + 0x013FB128;
    uintptr_t vmtWorldObject = gameBase + 0x013FB240;
    uintptr_t vmtCharRigidBody = gameBase + 0x013246C0;
    uintptr_t vmtPlayer = gameBase + 0x012C2CB0;
    uintptr_t vmtLandVeh = gameBase + 0x012D9A68;
    uintptr_t vmtVehChassis = gameBase + 0x0132C3B0;

    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);
    uintptr_t maxScanAddr = (uintptr_t)sysInfo.lpMaximumApplicationAddress;
    if (maxScanAddr > 0x0000030000000000ULL) {
        maxScanAddr = 0x0000030000000000ULL; // Scan practical 64-bit heap range (up to 3 TB)
    }

    MEMORY_BASIC_INFORMATION mbi;
    uintptr_t currentAddr = 0x10000;

    while (currentAddr < maxScanAddr) {
        if (VirtualQuery((LPCVOID)currentAddr, &mbi, sizeof(mbi)) == sizeof(mbi)) {
            if (mbi.State == MEM_COMMIT && (mbi.Protect == PAGE_READWRITE) && !(mbi.Protect & PAGE_GUARD) && mbi.RegionSize <= 64 * 1024 * 1024) {
                const size_t chunkSize = sizeof(s_scanBuffer);

                for (size_t offset = 0; offset < mbi.RegionSize; offset += chunkSize) {
                    size_t toRead = min(chunkSize, mbi.RegionSize - offset);
                    SIZE_T bytesRead = 0;

                    if (ReadProcessMemory(GetCurrentProcess(), (LPCVOID)(currentAddr + offset), s_scanBuffer, toRead, &bytesRead) && bytesRead >= 0x80) {
                        for (size_t i = 0; i + 0x70 <= bytesRead; i += 8) {
                            uintptr_t vmt = *reinterpret_cast<uintptr_t*>(&s_scanBuffer[i]);
                            if (vmt == vmtHavokMotion || vmt == vmtWorldObject || vmt == vmtCharRigidBody || 
                                vmt == vmtPlayer || vmt == vmtLandVeh || vmt == vmtVehChassis) {
                                float* pPos = reinterpret_cast<float*>(&s_scanBuffer[i + 0x50]);
                                
                                // Plausible wasteland ground & non-zero coordinates
                                if (pPos[1] >= -300.0f && pPos[1] <= 3500.0f &&
                                    (abs(pPos[0]) > 0.1f || abs(pPos[2]) > 0.1f)) {
                                    
                                    g_pHavokMotionInstance = currentAddr + offset + i;
                                    g_localTransform.isLocked = true;
                                    g_localTransform.pos[0] = pPos[0];
                                    g_localTransform.pos[1] = pPos[1];
                                    g_localTransform.pos[2] = pPos[2];
                                    return true;
                                }
                            }
                        }
                    }
                }
            }
            currentAddr = (uintptr_t)mbi.BaseAddress + mbi.RegionSize;
        } else {
            currentAddr += 0x10000;
        }
    }
    return false;
}

// Read current Havok motion state (with continuous altitude and non-zero validation)
inline bool UpdateLocalTransform(uintptr_t gameBase) {
    if (!g_pHavokMotionInstance) {
        static DWORD s_lastScanTime = 0;
        DWORD now = GetTickCount();
        if (now - s_lastScanTime < 1500) return false;
        s_lastScanTime = now;

        FindHavokMotion(gameBase);
        if (!g_pHavokMotionInstance) return false;
    }

    uint8_t motionData[0x80];
    SIZE_T read = 0;
    if (ReadProcessMemory(GetCurrentProcess(), (LPCVOID)g_pHavokMotionInstance, motionData, sizeof(motionData), &read) && read == sizeof(motionData)) {
        // Read Position (+0x50)
        float* pPos = reinterpret_cast<float*>(&motionData[0x50]);

        // Validate that this object is still in the active wasteland play area
        if (pPos[1] < -300.0f || pPos[1] > 3500.0f || (pPos[0] == 0.0f && pPos[2] == 0.0f)) {
            // Position became invalid or 0,0,0! Unlock so next cycle scans for the real object
            g_pHavokMotionInstance = 0;
            g_localTransform.isLocked = false;
            return false;
        }

        // Frame-delta velocity & speed calculation (100% reliable, zero artifacting)
        static float s_lastPos[3] = { 0 };
        static DWORD s_lastTime = 0;
        DWORD now = GetTickCount();
        float dt = (now - s_lastTime) * 0.001f;

        if (dt > 0.005f && dt < 0.5f && s_lastPos[1] >= 400.0f) {
            float dx = pPos[0] - s_lastPos[0];
            float dy = pPos[1] - s_lastPos[1];
            float dz = pPos[2] - s_lastPos[2];
            float distMoved = sqrtf(dx * dx + dy * dy + dz * dz);
            
            g_localTransform.vel[0] = dx / dt;
            g_localTransform.vel[1] = dy / dt;
            g_localTransform.vel[2] = dz / dt;
            g_localTransform.speed = distMoved / dt;

            // Update heading yaw in direction of travel
            if (distMoved > 0.04f) {
                g_localTransform.yaw = atan2f(dx, dz);
            }
        } else {
            g_localTransform.speed = 0.0f;
            g_localTransform.vel[0] = 0.0f;
            g_localTransform.vel[1] = 0.0f;
            g_localTransform.vel[2] = 0.0f;
        }

        s_lastPos[0] = pPos[0];
        s_lastPos[1] = pPos[1];
        s_lastPos[2] = pPos[2];
        s_lastTime = now;

        g_localTransform.pos[0] = pPos[0];
        g_localTransform.pos[1] = pPos[1];
        g_localTransform.pos[2] = pPos[2];

        // Read 3x3 rotation matrix (+0x20)
        float* pRot = reinterpret_cast<float*>(&motionData[0x20]);
        for (int r = 0; r < 3; r++) {
            for (int c = 0; c < 3; c++) {
                g_localTransform.rot[r][c] = pRot[r * 4 + c];
            }
        }

        g_localTransform.isLocked = true;
        return true;
    } else {
        g_pHavokMotionInstance = 0;
        g_localTransform.isLocked = false;
        return false;
    }
}

// Converts 3x3 rotation matrix to quaternion
inline void MatrixToQuaternion(const float rot[3][3], float& qx, float& qy, float& qz, float& qw) {
    float trace = rot[0][0] + rot[1][1] + rot[2][2];
    if (trace > 0.0f) {
        float s = 0.5f / sqrtf(trace + 1.0f);
        qw = 0.25f / s;
        qx = (rot[2][1] - rot[1][2]) * s;
        qy = (rot[0][2] - rot[2][0]) * s;
        qz = (rot[1][0] - rot[0][1]) * s;
    } else {
        if (rot[0][0] > rot[1][1] && rot[0][0] > rot[2][2]) {
            float s = 2.0f * sqrtf(1.0f + rot[0][0] - rot[1][1] - rot[2][2]);
            qw = (rot[2][1] - rot[1][2]) / s;
            qx = 0.25f * s;
            qy = (rot[0][1] + rot[1][0]) / s;
            qz = (rot[0][2] + rot[2][0]) / s;
        } else if (rot[1][1] > rot[2][2]) {
            float s = 2.0f * sqrtf(1.0f + rot[1][1] - rot[0][0] - rot[2][2]);
            qw = (rot[0][2] - rot[2][0]) / s;
            qx = (rot[0][1] + rot[1][0]) / s;
            qy = 0.25f * s;
            qz = (rot[1][2] + rot[2][1]) / s;
        } else {
            float s = 2.0f * sqrtf(1.0f + rot[2][2] - rot[0][0] - rot[1][1]);
            qw = (rot[1][0] - rot[0][1]) / s;
            qx = (rot[0][2] + rot[2][0]) / s;
            qy = (rot[1][2] + rot[2][1]) / s;
            qz = 0.25f * s;
        }
    }
}

}
