#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <dxgi.h>

typedef HRESULT(WINAPI* PFN_CreateDXGIFactory1)(REFIID riid, void** ppFactory);
static PFN_CreateDXGIFactory1 g_pfnCreateDXGIFactory1 = nullptr;
static HMODULE g_hRealDxgi = nullptr;

static void LoadRealDxgi() {
    if (!g_hRealDxgi) {
        char sysPath[MAX_PATH];
        GetSystemDirectoryA(sysPath, MAX_PATH);
        strcat_s(sysPath, "\\dxgi.dll");
        g_hRealDxgi = LoadLibraryA(sysPath);
        if (g_hRealDxgi) {
            g_pfnCreateDXGIFactory1 = (PFN_CreateDXGIFactory1)GetProcAddress(g_hRealDxgi, "CreateDXGIFactory1");
        }
    }
}

extern "C" HRESULT WINAPI Proxy_CreateDXGIFactory1(REFIID riid, void** ppFactory) {
    LoadRealDxgi();
    if (g_pfnCreateDXGIFactory1) {
        return g_pfnCreateDXGIFactory1(riid, ppFactory);
    }
    return E_FAIL;
}
