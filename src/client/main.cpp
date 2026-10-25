#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <enet/enet.h>

#include <d3d11.h>
#include <dxgi.h>
#include <iostream>
#include <MinHook.h>
#include <imgui.h>
#include <backends/imgui_impl_win32.h>
#include <backends/imgui_impl_dx11.h>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

typedef HRESULT(__fastcall* IDXGISwapChain_Present_t)(IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags);
static IDXGISwapChain_Present_t oPresent = nullptr;

static HWND g_hWnd = nullptr;
static WNDPROC oWndProc = nullptr;
static ID3D11Device* g_pd3dDevice = nullptr;
static ID3D11DeviceContext* g_pd3dDeviceContext = nullptr;
static ID3D11RenderTargetView* g_mainRenderTargetView = nullptr;
static bool g_bImGuiInitialized = false;
static bool g_bShowMenu = true;

// Networking state (ENet)
static ENetHost* g_pClient = nullptr;
static ENetPeer* g_pServerPeer = nullptr;
static bool g_bConnected = false;
static char g_serverIp[64] = "127.0.0.1";
static int g_serverPort = 7777;

LRESULT CALLBACK Hooked_WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    if (g_bShowMenu && ImGui_ImplWin32_WndProcHandler(hWnd, uMsg, wParam, lParam))
        return true;
    if (uMsg == WM_KEYDOWN && wParam == VK_F5) {
        g_bShowMenu = !g_bShowMenu;
        return 0;
    }
    return CallWindowProc(oWndProc, hWnd, uMsg, wParam, lParam);
}

HRESULT __fastcall Hooked_Present(IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags) {
    if (!g_bImGuiInitialized) {
        if (SUCCEEDED(pSwapChain->GetDevice(__uuidof(ID3D11Device), (void**)&g_pd3dDevice))) {
            g_pd3dDevice->GetImmediateContext(&g_pd3dDeviceContext);
            DXGI_SWAP_CHAIN_DESC sd;
            pSwapChain->GetDesc(&sd);
            g_hWnd = sd.OutputWindow;

            ID3D11Texture2D* pBackBuffer = nullptr;
            pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&pBackBuffer);
            if (pBackBuffer) {
                g_pd3dDevice->CreateRenderTargetView(pBackBuffer, NULL, &g_mainRenderTargetView);
                pBackBuffer->Release();
            }

            oWndProc = (WNDPROC)SetWindowLongPtr(g_hWnd, GWLP_WNDPROC, (LONG_PTR)Hooked_WndProc);

            IMGUI_CHECKVERSION();
            ImGui::CreateContext();
            ImGuiIO& io = ImGui::GetIO();
            io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

            ImGui::StyleColorsDark();
            ImGui_ImplWin32_Init(g_hWnd);
            ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);

            enet_initialize();
            g_pClient = enet_host_create(NULL, 1, 2, 0, 0);

            g_bImGuiInitialized = true;
        }
    }

    if (g_bImGuiInitialized && g_mainRenderTargetView) {
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        if (g_bShowMenu) {
            ImGui::Begin("Mad Max Co-Op Multiplayer (JC2-MP Architecture)", &g_bShowMenu, ImGuiWindowFlags_AlwaysAutoResize);
            ImGui::Text("Mad Max Multiplayer Injected - Press [F5] to Toggle Menu");
            ImGui::Separator();

            ImGui::InputText("Server IP", g_serverIp, sizeof(g_serverIp));
            ImGui::InputInt("Port", &g_serverPort);

            if (!g_bConnected) {
                if (ImGui::Button("Connect to Server")) {
                    ENetAddress address;
                    enet_address_set_host(&address, g_serverIp);
                    address.port = (enet_uint16)g_serverPort;
                    g_pServerPeer = enet_host_connect(g_pClient, &address, 2, 0);
                    if (g_pServerPeer) {
                        g_bConnected = true;
                    }
                }
            } else {
                ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), "Connected to Relay Server!");
                if (ImGui::Button("Disconnect")) {
                    if (g_pServerPeer) enet_peer_disconnect(g_pServerPeer, 0);
                    g_bConnected = false;
                }
            }

            ImGui::Separator();
            ImGui::Text("State Sync Telemetry:");
            ImGui::Text("Max Position (XYZ): [Locating AOB hook...]");
            ImGui::Text("Magnum Opus Velocity: [Locating AOB hook...]");
            ImGui::End();
        }

        ImGui::Render();
        g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, NULL);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    }

    return oPresent(pSwapChain, SyncInterval, Flags);
}

DWORD WINAPI ModThread(LPVOID lpParam) {
    Sleep(2500);

    WNDCLASSEX wc = { sizeof(WNDCLASSEX), CS_CLASSDC, DefWindowProc, 0L, 0L, GetModuleHandle(NULL), NULL, NULL, NULL, NULL, "DXGITest", NULL };
    RegisterClassEx(&wc);
    HWND hWnd = CreateWindow("DXGITest", "DXGITest", WS_OVERLAPPEDWINDOW, 0, 0, 100, 100, NULL, NULL, wc.hInstance, NULL);

    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL featureLevels[] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };

    DXGI_SWAP_CHAIN_DESC scd;
    ZeroMemory(&scd, sizeof(scd));
    scd.BufferCount = 1;
    scd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scd.OutputWindow = hWnd;
    scd.SampleDesc.Count = 1;
    scd.Windowed = TRUE;

    ID3D11Device* pDevice = nullptr;
    ID3D11DeviceContext* pContext = nullptr;
    IDXGISwapChain* pSwapChain = nullptr;

    HRESULT hr = D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, 0, featureLevels, 2,
        D3D11_SDK_VERSION, &scd, &pSwapChain, &pDevice, &featureLevel, &pContext);

    if (SUCCEEDED(hr) && pSwapChain) {
        void** pVMT = *(void***)pSwapChain;
        void* pPresent = pVMT[8];

        pSwapChain->Release();
        pDevice->Release();
        pContext->Release();
        DestroyWindow(hWnd);
        UnregisterClass("DXGITest", wc.hInstance);

        if (MH_Initialize() == MH_OK) {
            MH_CreateHook(pPresent, &Hooked_Present, (LPVOID*)&oPresent);
            MH_EnableHook(pPresent);
        }
    }

    return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    if (ul_reason_for_call == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hModule);
        CreateThread(NULL, 0, ModThread, NULL, 0, NULL);
    }
    return TRUE;
}
