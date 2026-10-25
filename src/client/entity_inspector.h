#pragma once
#include <windows.h>
#include <iostream>
#include <vector>
#include <sstream>
#include <iomanip>
#include "vmt_catalog.h"

namespace EntityInspector {

struct InspectedObject {
    uintptr_t baseAddress;
    uintptr_t vmtAddress;
    const char* className;
    ptrdiff_t coordOffset;
    uintptr_t memberPointers[16];
    bool isValid;
};

static InspectedObject g_playerEntity = { 0 };
static bool g_bInspected = false;
static char g_diagnosticLog[1024] = "Waiting for coordinate lock...";

inline bool SafeReadPtr(uintptr_t addr, uintptr_t* outVal) {
    SIZE_T read = 0;
    return ReadProcessMemory(GetCurrentProcess(), (LPCVOID)addr, outVal, sizeof(uintptr_t), &read) && read == sizeof(uintptr_t);
}

// Deep inspects the memory around coordAddress (both direct struct and stack pointer dereference)
inline bool InspectPlayerObject(uintptr_t coordAddress, uintptr_t gameBase, size_t gameSize) {
    if (!coordAddress || !gameBase) return false;

    MEMORY_BASIC_INFORMATION mbi;
    VirtualQuery((LPCVOID)coordAddress, &mbi, sizeof(mbi));

    // Phase 1: Direct struct scan (walking backwards up to 0x1000 bytes)
    for (ptrdiff_t offset = 0; offset <= 0x1000; offset += 8) {
        uintptr_t candBase = coordAddress - offset;
        uintptr_t vmtPtr = 0;
        if (SafeReadPtr(candBase, &vmtPtr)) {
            if (vmtPtr >= gameBase && vmtPtr < (gameBase + gameSize)) {
                const char* clsName = VMTCatalog::LookupClassByVMT(vmtPtr, gameBase);
                if (clsName) {
                    g_playerEntity.baseAddress = candBase;
                    g_playerEntity.vmtAddress = vmtPtr;
                    g_playerEntity.className = clsName;
                    g_playerEntity.coordOffset = offset;
                    g_playerEntity.isValid = true;

                    for (int i = 0; i < 16; i++) {
                        SafeReadPtr(candBase + i * 8, &g_playerEntity.memberPointers[i]);
                    }

                    sprintf_s(g_diagnosticLog, sizeof(g_diagnosticLog),
                        "[Direct Struct] %s @ 0x%p (VMT 0x%p, Offset +0x%tX)",
                        clsName, (void*)candBase, (void*)vmtPtr, offset);
                    
                    g_bInspected = true;
                    return true;
                }
            }
        }
    }

    // Phase 2: Pointer Dereference Scan (e.g. if coordAddress is on Stack/TLS, find 'this' pointer)
    // Scan [-0x800 to +0x800] from coordAddress for any pointer pointing to an object with a recognized VMT
    for (ptrdiff_t offset = -0x800; offset <= 0x800; offset += 8) {
        uintptr_t stackVal = 0;
        if (SafeReadPtr(coordAddress + offset, &stackVal)) {
            // Check if stackVal is a plausible heap pointer (aligned to 8 bytes, non-null)
            if (stackVal > 0x10000 && (stackVal & 7) == 0) {
                uintptr_t targetVmt = 0;
                if (SafeReadPtr(stackVal, &targetVmt)) {
                    if (targetVmt >= gameBase && targetVmt < (gameBase + gameSize)) {
                        const char* clsName = VMTCatalog::LookupClassByVMT(targetVmt, gameBase);
                        if (clsName) {
                            g_playerEntity.baseAddress = stackVal;
                            g_playerEntity.vmtAddress = targetVmt;
                            g_playerEntity.className = clsName;
                            g_playerEntity.coordOffset = offset;
                            g_playerEntity.isValid = true;

                            for (int i = 0; i < 16; i++) {
                                SafeReadPtr(stackVal + i * 8, &g_playerEntity.memberPointers[i]);
                            }

                            sprintf_s(g_diagnosticLog, sizeof(g_diagnosticLog),
                                "[Stack Ref @ %c0x%tX] -> %s @ 0x%p (VMT 0x%p)",
                                (offset >= 0 ? '+' : '-'), (offset >= 0 ? offset : -offset),
                                clsName, (void*)stackVal, (void*)targetVmt);

                            g_bInspected = true;
                            return true;
                        }
                    }
                }
            }
        }
    }

    sprintf_s(g_diagnosticLog, sizeof(g_diagnosticLog),
        "Coord @ 0x%p (Region 0x%p, Size 0x%zX): No VMT in direct struct or stack frame",
        (void*)coordAddress, mbi.BaseAddress, mbi.RegionSize);
    return false;
}

}
