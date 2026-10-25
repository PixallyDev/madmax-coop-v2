#pragma once
#include <windows.h>
#include <vector>
#include <string>
#include <sstream>

namespace Memory {

inline uintptr_t PatternScan(uintptr_t baseAddress, size_t imageSize, const char* signature, const char* mask) {
    size_t patternLength = strlen(mask);
    for (size_t i = 0; i < imageSize - patternLength; ++i) {
        bool found = true;
        for (size_t j = 0; j < patternLength; ++j) {
            if (mask[j] != '?' && *(reinterpret_cast<const char*>(baseAddress + i + j)) != signature[j]) {
                found = false;
                break;
            }
        }
        if (found) {
            return baseAddress + i;
        }
    }
    return 0;
}

inline uintptr_t FindModule(const char* moduleName, size_t* outSize = nullptr) {
    HMODULE hMod = GetModuleHandleA(moduleName);
    if (!hMod) return 0;

    PIMAGE_DOS_HEADER dosHeader = (PIMAGE_DOS_HEADER)hMod;
    PIMAGE_NT_HEADERS ntHeaders = (PIMAGE_NT_HEADERS)((uintptr_t)hMod + dosHeader->e_lfanew);

    if (outSize) {
        *outSize = ntHeaders->OptionalHeader.SizeOfImage;
    }
    return (uintptr_t)hMod;
}

}
