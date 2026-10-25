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

// Inspects the object containing the scanned player coordinates
inline bool InspectPlayerObject(uintptr_t coordAddress, uintptr_t gameBase, size_t gameSize) {
    if (!coordAddress || !gameBase) return false;

    // Scan backwards from coordAddress up to 0x400 bytes to find vtable pointer
    for (ptrdiff_t offset = 0; offset <= 0x400; offset += 8) {
        uintptr_t candBase = coordAddress - offset;
        uintptr_t vmtPtr = 0;
        SIZE_T read = 0;

        if (ReadProcessMemory(GetCurrentProcess(), (LPCVOID)candBase, &vmtPtr, sizeof(uintptr_t), &read) && read == sizeof(uintptr_t)) {
            // Check if vmtPtr is within the game's executable image
            if (vmtPtr >= gameBase && vmtPtr < (gameBase + gameSize)) {
                const char* clsName = VMTCatalog::LookupClassByVMT(vmtPtr, gameBase);
                if (clsName) {
                    g_playerEntity.baseAddress = candBase;
                    g_playerEntity.vmtAddress = vmtPtr;
                    g_playerEntity.className = clsName;
                    g_playerEntity.coordOffset = offset;
                    g_playerEntity.isValid = true;

                    // Read first 16 member pointers of the object
                    for (int i = 0; i < 16; i++) {
                        uintptr_t memVal = 0;
                        ReadProcessMemory(GetCurrentProcess(), (LPCVOID)(candBase + i * 8), &memVal, sizeof(uintptr_t), &read);
                        g_playerEntity.memberPointers[i] = memVal;
                    }

                    sprintf_s(g_diagnosticLog, sizeof(g_diagnosticLog),
                        "Found %s @ 0x%p (VMT 0x%p, Coord Offset +0x%tX)",
                        clsName, (void*)candBase, (void*)vmtPtr, offset);
                    
                    g_bInspected = true;
                    return true;
                }
            }
        }
    }

    sprintf_s(g_diagnosticLog, sizeof(g_diagnosticLog),
        "Coordinate @ 0x%p has no recognized VMT within -0x400 window", (void*)coordAddress);
    return false;
}

}
