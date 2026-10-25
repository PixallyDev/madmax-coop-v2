#pragma once
#include <windows.h>
#include <cmath>
#include <vector>
#include <iostream>
#include "../common/protocol.h"

namespace HavokSync {

struct HavokTransform {
    float rot[3][3];    // +0x20 3x3 rotation matrix
    float pos[3];       // +0x50 position vector (X, Y, Z)
    float vel[3];       // +0x60 velocity vector
    float yaw;          // Facing angle in radians
    float speed;        // Speed in m/s
    bool isLocked;
};

static uintptr_t g_pHavokMotionInstance = 0;
static HavokTransform g_localTransform = { 0 };
static bool g_bScanningActive = false;

// Scan committed memory for hkpCharacterMotion (VMT 0x1413FB128)
inline bool FindHavokMotion(uintptr_t gameBase) {
    uintptr_t targetVmt = gameBase + 0x013FB128; // hkpCharacterMotion VMT

    SYSTEM_INFO si;
    GetSystemInfo(&si);

    MEMORY_BASIC_INFORMATION mbi;
    uintptr_t currentAddr = (uintptr_t)si.lpMinimumApplicationAddress;

    while (currentAddr < (uintptr_t)si.lpMaximumApplicationAddress) {
        if (VirtualQuery((LPCVOID)currentAddr, &mbi, sizeof(mbi)) == sizeof(mbi)) {
            if (mbi.State == MEM_COMMIT && (mbi.Protect == PAGE_READWRITE) && !(mbi.Protect & PAGE_GUARD)) {
                const size_t chunkSize = 128 * 1024;
                std::vector<uint8_t> buffer(chunkSize);

                for (size_t offset = 0; offset < mbi.RegionSize; offset += chunkSize) {
                    size_t toRead = min(chunkSize, mbi.RegionSize - offset);
                    SIZE_T bytesRead = 0;

                    if (ReadProcessMemory(GetCurrentProcess(), (LPCVOID)(currentAddr + offset), buffer.data(), toRead, &bytesRead) && bytesRead >= 0x80) {
                        for (size_t i = 0; i + 0x70 <= bytesRead; i += 8) {
                            uintptr_t vmt = *reinterpret_cast<uintptr_t*>(&buffer[i]);
                            if (vmt == targetVmt) {
                                // Check if candidate has valid matrix (rot[0][0] approx 1 or orientation)
                                float* pRot = reinterpret_cast<float*>(&buffer[i + 0x20]);
                                float* pPos = reinterpret_cast<float*>(&buffer[i + 0x50]);

                                // Altitude in wasteland is typically between -100m and 2000m
                                if (pPos[1] > -100.0f && pPos[1] < 3000.0f && abs(pPos[1]) > 0.01f) {
                                    g_pHavokMotionInstance = currentAddr + offset + i;
                                    g_localTransform.isLocked = true;
                                    return true;
                                }
                            }
                        }
                    }
                }
            }
            currentAddr = (uintptr_t)mbi.BaseAddress + mbi.RegionSize;
        } else {
            currentAddr += 0x1000;
        }
    }
    return false;
}

// Read current Havok motion state
inline bool UpdateLocalTransform(uintptr_t gameBase) {
    if (!g_pHavokMotionInstance) {
        FindHavokMotion(gameBase);
        if (!g_pHavokMotionInstance) return false;
    }

    uint8_t motionData[0x80];
    SIZE_T read = 0;
    if (ReadProcessMemory(GetCurrentProcess(), (LPCVOID)g_pHavokMotionInstance, motionData, sizeof(motionData), &read) && read == sizeof(motionData)) {
        // Read 3x3 rotation matrix (+0x20)
        float* pRot = reinterpret_cast<float*>(&motionData[0x20]);
        for (int r = 0; r < 3; r++) {
            for (int c = 0; c < 3; c++) {
                g_localTransform.rot[r][c] = pRot[r * 4 + c];
            }
        }

        // Read Position (+0x50)
        float* pPos = reinterpret_cast<float*>(&motionData[0x50]);
        g_localTransform.pos[0] = pPos[0];
        g_localTransform.pos[1] = pPos[1];
        g_localTransform.pos[2] = pPos[2];

        // Read Velocity (+0x60)
        float* pVel = reinterpret_cast<float*>(&motionData[0x60]);
        g_localTransform.vel[0] = pVel[0];
        g_localTransform.vel[1] = pVel[1];
        g_localTransform.vel[2] = pVel[2];

        // Compute yaw and speed
        g_localTransform.yaw = atan2f(g_localTransform.rot[0][2], g_localTransform.rot[2][2]);
        g_localTransform.speed = sqrtf(pVel[0] * pVel[0] + pVel[1] * pVel[1] + pVel[2] * pVel[2]);
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
