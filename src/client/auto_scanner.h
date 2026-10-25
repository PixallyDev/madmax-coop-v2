#pragma once
#include <windows.h>
#include <vector>
#include <iostream>
#include "../common/protocol.h"

namespace AutoScanner {

struct CandidateCoord {
    uintptr_t address;
    float lastX, lastY, lastZ;
    float variance;
    int samples;
};

static std::vector<CandidateCoord> g_candidates;
static uintptr_t g_selectedPlayerAddress = 0;
static bool g_bScanningActive = false;

inline float curCoords(uintptr_t addr, int idx) {
    float val = 0.0f;
    SIZE_T read = 0;
    ReadProcessMemory(GetCurrentProcess(), (LPCVOID)(addr + idx * sizeof(float)), &val, sizeof(float), &read);
    return val;
}

// Scan heap & writable memory for plausible Wasteland player coordinates
inline void StartHeuristicScan() {
    g_candidates.clear();
    g_selectedPlayerAddress = 0;
    g_bScanningActive = true;

    SYSTEM_INFO si;
    GetSystemInfo(&si);

    MEMORY_BASIC_INFORMATION mbi;
    uintptr_t currentAddr = (uintptr_t)si.lpMinimumApplicationAddress;

    while (currentAddr < (uintptr_t)si.lpMaximumApplicationAddress) {
        if (VirtualQuery((LPCVOID)currentAddr, &mbi, sizeof(mbi)) == sizeof(mbi)) {
            // Only scan commit, readable & writable non-page-guarded memory (Heap, Data, Stack)
            if (mbi.State == MEM_COMMIT && (mbi.Protect == PAGE_READWRITE) && !(mbi.Protect & PAGE_GUARD)) {
                const size_t chunkSize = 64 * 1024;
                std::vector<uint8_t> buffer(chunkSize);

                for (size_t offset = 0; offset < mbi.RegionSize; offset += chunkSize) {
                    size_t toRead = min(chunkSize, mbi.RegionSize - offset);
                    SIZE_T bytesRead = 0;

                    if (ReadProcessMemory(GetCurrentProcess(), (LPCVOID)(currentAddr + offset), buffer.data(), toRead, &bytesRead) && bytesRead > sizeof(float) * 3) {
                        for (size_t i = 0; i + sizeof(float) * 3 <= bytesRead; i += 4) {
                            float x = *reinterpret_cast<float*>(&buffer[i]);
                            float y = *reinterpret_cast<float*>(&buffer[i + 4]);
                            float z = *reinterpret_cast<float*>(&buffer[i + 8]);

                            // Realistic wasteland bounds & non-zero coordinates
                            if (x > -40000.0f && x < 40000.0f && abs(x) > 1.0f &&
                                y > -200.0f && y < 2000.0f && abs(y) > 0.1f &&
                                z > -40000.0f && z < 40000.0f && abs(z) > 1.0f) {
                                
                                uintptr_t candAddr = currentAddr + offset + i;
                                g_candidates.push_back({ candAddr, x, y, z, 0.0f, 1 });
                                if (g_candidates.size() >= 2000) break;
                            }
                        }
                    }
                    if (g_candidates.size() >= 2000) break;
                }
            }
            currentAddr = (uintptr_t)mbi.BaseAddress + mbi.RegionSize;
        } else {
            currentAddr += 0x1000;
        }
        if (g_candidates.size() >= 2000) break;
    }
}

// Track candidates over time while user walks
inline void UpdateCandidates(float* outCoords) {
    if (!g_bScanningActive || g_candidates.empty()) return;

    if (g_selectedPlayerAddress != 0) {
        SIZE_T read = 0;
        float cur[3];
        if (ReadProcessMemory(GetCurrentProcess(), (LPCVOID)g_selectedPlayerAddress, cur, sizeof(cur), &read) && read == sizeof(cur)) {
            outCoords[0] = cur[0];
            outCoords[1] = cur[1];
            outCoords[2] = cur[2];
        } else {
            g_selectedPlayerAddress = 0; // Lost pointer
        }
        return;
    }

    uintptr_t bestCandidate = 0;
    float maxMovement = 0.0f;

    for (auto it = g_candidates.begin(); it != g_candidates.end(); ) {
        SIZE_T read = 0;
        float cur[3];
        if (ReadProcessMemory(GetCurrentProcess(), (LPCVOID)it->address, cur, sizeof(cur), &read) && read == sizeof(cur)) {
            float dx = cur[0] - it->lastX;
            float dy = cur[1] - it->lastY;
            float dz = cur[2] - it->lastZ;
            float dist = sqrtf(dx * dx + dz * dz + dy * dy);

            // Realistic walking speed delta per frame: between 0.02m and 15.0m
            if (dist > 0.02f && dist < 15.0f) {
                it->variance += dist;
                it->samples++;
                it->lastX = cur[0];
                it->lastY = cur[1];
                it->lastZ = cur[2];

                if (it->variance > maxMovement && it->samples >= 3) {
                    maxMovement = it->variance;
                    bestCandidate = it->address;
                }
                ++it;
            } else if (dist > 50.0f) {
                it = g_candidates.erase(it); // Garbage noise
            } else {
                ++it;
            }
        } else {
            it = g_candidates.erase(it);
        }
    }

    if (bestCandidate != 0 && maxMovement > 1.0f) {
        g_selectedPlayerAddress = bestCandidate;
        outCoords[0] = curCoords(bestCandidate, 0);
        outCoords[1] = curCoords(bestCandidate, 1);
        outCoords[2] = curCoords(bestCandidate, 2);
    }
}

}
