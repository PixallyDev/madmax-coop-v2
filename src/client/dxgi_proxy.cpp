#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <dxgi.h>

static HMODULE g_hRealDxgi = nullptr;

static FARPROC GetRealProc(const char* funcName) {
    if (!g_hRealDxgi) {
        char sysPath[MAX_PATH];
        GetSystemDirectoryA(sysPath, MAX_PATH);
        strcat_s(sysPath, "\\dxgi.dll");
        g_hRealDxgi = LoadLibraryA(sysPath);
    }
    return g_hRealDxgi ? GetProcAddress(g_hRealDxgi, funcName) : nullptr;
}

#define FORWARD_EXPORT(name) \
    extern "C" FARPROC WINAPI Proxy_##name() { \
        static FARPROC fn = nullptr; \
        if (!fn) fn = GetRealProc(#name); \
        return fn; \
    }

extern "C" HRESULT WINAPI Proxy_CreateDXGIFactory(REFIID riid, void** ppFactory) {
    typedef HRESULT(WINAPI* PFN)(REFIID, void**);
    static PFN fn = (PFN)GetRealProc("CreateDXGIFactory");
    return fn ? fn(riid, ppFactory) : E_FAIL;
}

extern "C" HRESULT WINAPI Proxy_CreateDXGIFactory1(REFIID riid, void** ppFactory) {
    typedef HRESULT(WINAPI* PFN)(REFIID, void**);
    static PFN fn = (PFN)GetRealProc("CreateDXGIFactory1");
    return fn ? fn(riid, ppFactory) : E_FAIL;
}

extern "C" HRESULT WINAPI Proxy_CreateDXGIFactory2(UINT Flags, REFIID riid, void** ppFactory) {
    typedef HRESULT(WINAPI* PFN)(UINT, REFIID, void**);
    static PFN fn = (PFN)GetRealProc("CreateDXGIFactory2");
    return fn ? fn(Flags, riid, ppFactory) : E_FAIL;
}

FORWARD_EXPORT(ApplyCompatResolutionQuirking)
FORWARD_EXPORT(CompatString)
FORWARD_EXPORT(CompatValue)
FORWARD_EXPORT(DXGID3D10CreateDevice)
FORWARD_EXPORT(DXGID3D10CreateLayeredDevice)
FORWARD_EXPORT(DXGID3D10GetLayeredDeviceSize)
FORWARD_EXPORT(DXGID3D10RegisterLayers)
FORWARD_EXPORT(DXGIDeclareAdapterRemovalSupport)
FORWARD_EXPORT(DXGIDisableVBlankVirtualization)
FORWARD_EXPORT(DXGIDumpJournal)
FORWARD_EXPORT(DXGIGetDebugInterface1)
FORWARD_EXPORT(DXGIReportAdapterConfiguration)
FORWARD_EXPORT(PIXBeginCapture)
FORWARD_EXPORT(PIXEndCapture)
FORWARD_EXPORT(PIXGetCaptureState)
FORWARD_EXPORT(SetAppCompatStringPointer)
FORWARD_EXPORT(UpdateHMDEmulationStatus)
