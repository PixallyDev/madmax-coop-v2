#pragma once
#include <windows.h>
#include <iostream>
#include <vector>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include "vmt_catalog.h"

namespace EntityInspector {

struct DiscoveredEntity {
    ptrdiff_t stackOffset;
    uintptr_t baseAddress;
    uintptr_t vmtAddress;
    const char* className;
    uintptr_t memberPointers[8];
};

static std::vector<DiscoveredEntity> g_discoveredList;
static bool g_bInspected = false;
static char g_diagnosticLog[1024] = "Waiting for coordinate lock...";

inline bool SafeReadPtr(uintptr_t addr, uintptr_t* outVal) {
    SIZE_T read = 0;
    return ReadProcessMemory(GetCurrentProcess(), (LPCVOID)addr, outVal, sizeof(uintptr_t), &read) && read == sizeof(uintptr_t);
}

// Deep inspects the memory around coordAddress (both direct struct and all stack frame pointers)
inline bool InspectPlayerObject(uintptr_t coordAddress, uintptr_t gameBase, size_t gameSize) {
    if (!coordAddress || !gameBase) return false;

    g_discoveredList.clear();

    // Phase 1: Direct struct scan (walking backwards up to 0x1000 bytes)
    for (ptrdiff_t offset = 0; offset <= 0x1000; offset += 8) {
        uintptr_t candBase = coordAddress - offset;
        uintptr_t vmtPtr = 0;
        if (SafeReadPtr(candBase, &vmtPtr)) {
            if (vmtPtr >= gameBase && vmtPtr < (gameBase + gameSize)) {
                const char* clsName = VMTCatalog::LookupClassByVMT(vmtPtr, gameBase);
                if (clsName) {
                    DiscoveredEntity ent;
                    ent.stackOffset = -offset;
                    ent.baseAddress = candBase;
                    ent.vmtAddress = vmtPtr;
                    ent.className = clsName;
                    for (int i = 0; i < 8; i++) {
                        SafeReadPtr(candBase + i * 8, &ent.memberPointers[i]);
                    }
                    g_discoveredList.push_back(ent);
                }
            }
        }
    }

    // Phase 2: Comprehensive Stack Frame Scan [-0x1000 to +0x1000]
    for (ptrdiff_t offset = -0x1000; offset <= 0x1000; offset += 8) {
        uintptr_t stackVal = 0;
        if (SafeReadPtr(coordAddress + offset, &stackVal)) {
            if (stackVal > 0x10000 && (stackVal & 7) == 0) {
                uintptr_t targetVmt = 0;
                if (SafeReadPtr(stackVal, &targetVmt)) {
                    if (targetVmt >= gameBase && targetVmt < (gameBase + gameSize)) {
                        const char* clsName = VMTCatalog::LookupClassByVMT(targetVmt, gameBase);
                        if (clsName) {
                            // Check if this object is already in list
                            bool alreadyIn = false;
                            for (const auto& existing : g_discoveredList) {
                                if (existing.baseAddress == stackVal) {
                                    alreadyIn = true;
                                    break;
                                }
                            }
                            if (!alreadyIn) {
                                DiscoveredEntity ent;
                                ent.stackOffset = offset;
                                ent.baseAddress = stackVal;
                                ent.vmtAddress = targetVmt;
                                ent.className = clsName;
                                for (int i = 0; i < 8; i++) {
                                    SafeReadPtr(stackVal + i * 8, &ent.memberPointers[i]);
                                }
                                g_discoveredList.push_back(ent);
                            }
                        }
                    }
                }
            }
        }
    }

    if (!g_discoveredList.empty()) {
        sprintf_s(g_diagnosticLog, sizeof(g_diagnosticLog),
            "Identified %zu Engine Objects in Active Call Stack!", g_discoveredList.size());
        g_bInspected = true;
        return true;
    }

    sprintf_s(g_diagnosticLog, sizeof(g_diagnosticLog),
        "Coord @ 0x%p: No VMT in direct struct or stack frame", (void*)coordAddress);
    return false;
}

}
