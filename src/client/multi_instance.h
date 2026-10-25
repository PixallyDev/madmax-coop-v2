#pragma once
#include <windows.h>
#include <MinHook.h>
#include <iostream>

typedef HANDLE(WINAPI* PFN_CreateMutexA)(LPSECURITY_ATTRIBUTES lpMutexAttributes, BOOL bInitialOwner, LPCSTR lpName);
typedef HANDLE(WINAPI* PFN_CreateMutexW)(LPSECURITY_ATTRIBUTES lpMutexAttributes, BOOL bInitialOwner, LPCWSTR lpName);
typedef HANDLE(WINAPI* PFN_CreateMutexExA)(LPSECURITY_ATTRIBUTES lpMutexAttributes, LPCSTR lpName, DWORD dwFlags, DWORD dwDesiredAccess);
typedef HANDLE(WINAPI* PFN_CreateMutexExW)(LPSECURITY_ATTRIBUTES lpMutexAttributes, LPCWSTR lpName, DWORD dwFlags, DWORD dwDesiredAccess);

static PFN_CreateMutexA oCreateMutexA = nullptr;
static PFN_CreateMutexW oCreateMutexW = nullptr;
static PFN_CreateMutexExA oCreateMutexExA = nullptr;
static PFN_CreateMutexExW oCreateMutexExW = nullptr;

inline HANDLE WINAPI Hooked_CreateMutexA(LPSECURITY_ATTRIBUTES lpMutexAttributes, BOOL bInitialOwner, LPCSTR lpName) {
    if (lpName) {
        char uniqueName[256];
        sprintf_s(uniqueName, "%s_%u", lpName, GetCurrentProcessId());
        return oCreateMutexA(lpMutexAttributes, bInitialOwner, uniqueName);
    }
    return oCreateMutexA(lpMutexAttributes, bInitialOwner, lpName);
}

inline HANDLE WINAPI Hooked_CreateMutexW(LPSECURITY_ATTRIBUTES lpMutexAttributes, BOOL bInitialOwner, LPCWSTR lpName) {
    if (lpName) {
        wchar_t uniqueName[256];
        swprintf_s(uniqueName, L"%ls_%u", lpName, GetCurrentProcessId());
        return oCreateMutexW(lpMutexAttributes, bInitialOwner, uniqueName);
    }
    return oCreateMutexW(lpMutexAttributes, bInitialOwner, lpName);
}

inline HANDLE WINAPI Hooked_CreateMutexExA(LPSECURITY_ATTRIBUTES lpMutexAttributes, LPCSTR lpName, DWORD dwFlags, DWORD dwDesiredAccess) {
    if (lpName) {
        char uniqueName[256];
        sprintf_s(uniqueName, "%s_%u", lpName, GetCurrentProcessId());
        return oCreateMutexExA(lpMutexAttributes, uniqueName, dwFlags, dwDesiredAccess);
    }
    return oCreateMutexExA(lpMutexAttributes, lpName, dwFlags, dwDesiredAccess);
}

inline HANDLE WINAPI Hooked_CreateMutexExW(LPSECURITY_ATTRIBUTES lpMutexAttributes, LPCWSTR lpName, DWORD dwFlags, DWORD dwDesiredAccess) {
    if (lpName) {
        wchar_t uniqueName[256];
        swprintf_s(uniqueName, L"%ls_%u", lpName, GetCurrentProcessId());
        return oCreateMutexExW(lpMutexAttributes, uniqueName, dwFlags, dwDesiredAccess);
    }
    return oCreateMutexExW(lpMutexAttributes, lpName, dwFlags, dwDesiredAccess);
}

inline void EnableMultiInstanceHooks() {
    HMODULE hKernel32 = GetModuleHandleA("kernel32.dll");
    if (!hKernel32) return;

    void* pCreateMutexA = (void*)GetProcAddress(hKernel32, "CreateMutexA");
    void* pCreateMutexW = (void*)GetProcAddress(hKernel32, "CreateMutexW");
    void* pCreateMutexExA = (void*)GetProcAddress(hKernel32, "CreateMutexExA");
    void* pCreateMutexExW = (void*)GetProcAddress(hKernel32, "CreateMutexExW");

    if (pCreateMutexA) {
        MH_CreateHook(pCreateMutexA, &Hooked_CreateMutexA, (LPVOID*)&oCreateMutexA);
        MH_EnableHook(pCreateMutexA);
    }
    if (pCreateMutexW) {
        MH_CreateHook(pCreateMutexW, &Hooked_CreateMutexW, (LPVOID*)&oCreateMutexW);
        MH_EnableHook(pCreateMutexW);
    }
    if (pCreateMutexExA) {
        MH_CreateHook(pCreateMutexExA, &Hooked_CreateMutexExA, (LPVOID*)&oCreateMutexExA);
        MH_EnableHook(pCreateMutexExA);
    }
    if (pCreateMutexExW) {
        MH_CreateHook(pCreateMutexExW, &Hooked_CreateMutexExW, (LPVOID*)&oCreateMutexExW);
        MH_EnableHook(pCreateMutexExW);
    }
}
