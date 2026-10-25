#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <enet/enet.h>

#include <d3d11.h>
#include <dxgi.h>
#include <iostream>
#include <vector>
#include <sstream>
#include <MinHook.h>
#include <imgui.h>
#include <backends/imgui_impl_win32.h>
#include <backends/imgui_impl_dx11.h>

#include "../common/protocol.h"
#include "../common/memory_scanner.h"
#include "auto_scanner.h"
#include "remote_render.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

typedef HRESULT(__fastcall* IDXGISwapChain_Present_t)(IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags);
static IDXGISwapChain_Present_t oPresent = nullptr;

typedef HRESULT(__fastcall* IDXGISwapChain_ResizeBuffers_t)(IDXGISwapChain* pSwapChain, UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT NewFormat, UINT SwapChainFlags);
static IDXGISwapChain_ResizeBuffers_t oResizeBuffers = nullptr;

static HWND g_hWnd = nullptr;
static WNDPROC oWndProc = nullptr;
static ID3D11Device* g_pd3dDevice = nullptr;
static ID3D11DeviceContext* g_pd3dDeviceContext = nullptr;
static ID3D11RenderTargetView* g_mainRenderTargetView = nullptr;
static bool g_bImGuiInitialized = false;
static bool g_bShowMenu = true;

// Networking state (ENet & MMCP)
static ENetHost* g_pClient = nullptr;
static ENetPeer* g_pServerPeer = nullptr;
static bool g_bConnected = false;
static char g_serverIp[64] = "127.0.0.1";
static int g_serverPort = 7777;
static uint32_t g_assignedClientId = 0;
static uint32_t g_packetSeq = 1;
static char g_motd[64] = "Waiting for connection...";

// Telemetry & Memory
static uintptr_t g_gameBase = 0;
static size_t g_gameSize = 0;
static uintptr_t g_targetCandidateAddress = 0;
static float g_localPlayerPos[3] = { 0.0f, 0.0f, 0.0f };
static bool g_bMemoryHooked = false;

static bool SafeReadCoords(uintptr_t address, float* outCoords) {
    if (!address) return false;
    SIZE_T bytesRead = 0;
    return ReadProcessMemory(GetCurrentProcess(), (LPCVOID)address, outCoords, sizeof(float) * 3, &bytesRead) && bytesRead == sizeof(float) * 3;
}

static void CleanupRenderTarget() {
    if (g_mainRenderTargetView) {
        g_mainRenderTargetView->Release();
        g_mainRenderTargetView = nullptr;
    }
}

static void CreateRenderTarget(IDXGISwapChain* pSwapChain) {
    ID3D11Texture2D* pBackBuffer = nullptr;
    if (SUCCEEDED(pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&pBackBuffer)) && pBackBuffer) {
        g_pd3dDevice->CreateRenderTargetView(pBackBuffer, NULL, &g_mainRenderTargetView);
        pBackBuffer->Release();
    }
}

LRESULT CALLBACK Hooked_WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    if (g_bShowMenu && ImGui_ImplWin32_WndProcHandler(hWnd, uMsg, wParam, lParam))
        return true;
    if (uMsg == WM_KEYDOWN && wParam == VK_F5) {
        g_bShowMenu = !g_bShowMenu;
        return 0;
    }
    return CallWindowProc(oWndProc, hWnd, uMsg, wParam, lParam);
}

HRESULT __fastcall Hooked_ResizeBuffers(IDXGISwapChain* pSwapChain, UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT NewFormat, UINT SwapChainFlags) {
    CleanupRenderTarget();
    HRESULT hr = oResizeBuffers(pSwapChain, BufferCount, Width, Height, NewFormat, SwapChainFlags);
    if (SUCCEEDED(hr) && g_pd3dDevice) {
        CreateRenderTarget(pSwapChain);
    }
    return hr;
}

static void ServiceNetwork() {
    if (!g_pClient) return;

    ENetEvent event;
    while (enet_host_service(g_pClient, &event, 0) > 0) {
        if (event.type == ENET_EVENT_TYPE_CONNECT) {
            std::vector<uint8_t> reqBuf(sizeof(PacketHeader) + sizeof(MsgClientJoinReq));
            PacketHeader* hdr = reinterpret_cast<PacketHeader*>(reqBuf.data());
            hdr->magic = MMCP_MAGIC;
            hdr->opcode = OP_CLIENT_JOIN_REQ;
            hdr->payloadSize = sizeof(MsgClientJoinReq);
            hdr->sequence = g_packetSeq++;
            hdr->clientId = 0;

            MsgClientJoinReq* req = reinterpret_cast<MsgClientJoinReq*>(reqBuf.data() + sizeof(PacketHeader));
            strncpy_s(req->playerName, "MaxRockatansky", sizeof(req->playerName));
            req->protocolVersion = 1;

            ENetPacket* pkt = enet_packet_create(reqBuf.data(), reqBuf.size(), ENET_PACKET_FLAG_RELIABLE);
            enet_peer_send(g_pServerPeer, 0, pkt);
        } else if (event.type == ENET_EVENT_TYPE_RECEIVE) {
            if (event.packet->dataLength >= sizeof(PacketHeader)) {
                PacketHeader* hdr = reinterpret_cast<PacketHeader*>(event.packet->data);
                if (hdr->magic == MMCP_MAGIC) {
                    uint8_t* payload = event.packet->data + sizeof(PacketHeader);
                    if (hdr->opcode == OP_CLIENT_JOIN_ACK) {
                        MsgClientJoinAck* ack = reinterpret_cast<MsgClientJoinAck*>(payload);
                        g_assignedClientId = ack->assignedClientId;
                        strncpy_s(g_motd, ack->serverMotd, sizeof(g_motd));
                        g_bConnected = true;
                    } else if (hdr->opcode == OP_PLAYER_SYNC && hdr->payloadSize == sizeof(MsgPlayerSync)) {
                        MsgPlayerSync* sync = reinterpret_cast<MsgPlayerSync*>(payload);
                        RemoteRenderer::UpdateOrAddPlayer(hdr->clientId, sync->posX, sync->posY, sync->posZ, sync->health);
                    }
                }
            }
            enet_packet_destroy(event.packet);
        } else if (event.type == ENET_EVENT_TYPE_DISCONNECT) {
            g_bConnected = false;
            g_assignedClientId = 0;
            strncpy_s(g_motd, "Disconnected", sizeof(g_motd));
        }
    }
}

HRESULT __fastcall Hooked_Present(IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags) {
    if (!g_bImGuiInitialized) {
        if (SUCCEEDED(pSwapChain->GetDevice(__uuidof(ID3D11Device), (void**)&g_pd3dDevice))) {
            g_pd3dDevice->GetImmediateContext(&g_pd3dDeviceContext);
            DXGI_SWAP_CHAIN_DESC sd;
            pSwapChain->GetDesc(&sd);
            g_hWnd = sd.OutputWindow;

            CreateRenderTarget(pSwapChain);

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

            g_gameBase = Memory::FindModule(NULL, &g_gameSize);

            g_bImGuiInitialized = true;
        }
    }

    if (!g_mainRenderTargetView && g_pd3dDevice) {
        CreateRenderTarget(pSwapChain);
    }

    ServiceNetwork();

    // Run auto-scanner updates
    if (AutoScanner::g_bScanningActive) {
        AutoScanner::UpdateCandidates(g_localPlayerPos);
        if (AutoScanner::g_selectedPlayerAddress != 0) {
            g_targetCandidateAddress = AutoScanner::g_selectedPlayerAddress;
            g_bMemoryHooked = true;
        }
    } else if (g_bMemoryHooked && g_targetCandidateAddress != 0) {
        if (!SafeReadCoords(g_targetCandidateAddress, g_localPlayerPos)) {
            g_bMemoryHooked = false;
        }
    }

    // Broadcast transform packet if attached and connected
    if (g_bConnected && g_bMemoryHooked) {
        static DWORD lastSend = 0;
        DWORD now = GetTickCount();
        if (now - lastSend >= 33) { // 30 Hz sync rate
            lastSend = now;
            std::vector<uint8_t> syncBuf(sizeof(PacketHeader) + sizeof(MsgPlayerSync));
            PacketHeader* hdr = reinterpret_cast<PacketHeader*>(syncBuf.data());
            hdr->magic = MMCP_MAGIC;
            hdr->opcode = OP_PLAYER_SYNC;
            hdr->payloadSize = sizeof(MsgPlayerSync);
            hdr->sequence = g_packetSeq++;
            hdr->clientId = g_assignedClientId;

            MsgPlayerSync* sync = reinterpret_cast<MsgPlayerSync*>(syncBuf.data() + sizeof(PacketHeader));
            sync->posX = g_localPlayerPos[0];
            sync->posY = g_localPlayerPos[1];
            sync->posZ = g_localPlayerPos[2];
            sync->health = 100.0f;

            ENetPacket* pkt = enet_packet_create(syncBuf.data(), syncBuf.size(), 0);
            enet_peer_send(g_pServerPeer, 1, pkt);
        }
    }

    // Interpolate positions of remote players
    RemoteRenderer::InterpolatePositions(0.016f);

    if (g_bImGuiInitialized && g_mainRenderTargetView) {
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        // Render Co-Op World HUD (Compass and Peer Waypoints)
        if (g_bConnected) {
            RemoteRenderer::RenderWorldHUD(g_localPlayerPos[0], g_localPlayerPos[1], g_localPlayerPos[2]);
        }

        if (g_bShowMenu) {
            ImGui::Begin("Mad Max Co-Op Multiplayer (Milestones C, D, E & F)", &g_bShowMenu, ImGuiWindowFlags_AlwaysAutoResize);
            ImGui::Text("Mad Max Multiplayer Injected - Press [F5] to Toggle Menu");
            ImGui::Separator();

            ImGui::Text("Network & Server Connection:");
            ImGui::InputText("Server IP", g_serverIp, sizeof(g_serverIp));
            ImGui::InputInt("Port", &g_serverPort);

            if (!g_bConnected) {
                if (ImGui::Button("Connect to Server")) {
                    ENetAddress address;
                    enet_address_set_host(&address, g_serverIp);
                    address.port = (enet_uint16)g_serverPort;
                    g_pServerPeer = enet_host_connect(g_pClient, &address, 2, 0);
                }
            } else {
                ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), "Connected! ClientID: %u", g_assignedClientId);
                ImGui::Text("MOTD: %s", g_motd);
                ImGui::Text("Peers in Session: %zu", RemoteRenderer::g_remotePlayers.size());
                if (ImGui::Button("Disconnect")) {
                    if (g_pServerPeer) enet_peer_disconnect(g_pServerPeer, 0);
                    g_bConnected = false;
                }
            }

            ImGui::Separator();
            ImGui::Text("Milestone D: Coordinate Scanner & Telemetry:");
            ImGui::Text("MadMax.exe Base: 0x%p (Size: 0x%zX)", (void*)g_gameBase, g_gameSize);

            if (!AutoScanner::g_bScanningActive) {
                if (ImGui::Button("Auto-Detect Max Coordinates (1-Click)")) {
                    AutoScanner::StartHeuristicScan();
                }
            } else {
                if (AutoScanner::g_selectedPlayerAddress == 0) {
                    ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "Scanning %zu candidate memory regions... Walk a few steps in-game!", AutoScanner::g_candidates.size());
                } else {
                    ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), "LOCKED onto Max @ 0x%p", (void*)AutoScanner::g_selectedPlayerAddress);
                }
            }

            if (g_bMemoryHooked) {
                ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), "Live Wasteland Telemetry:");
                ImGui::Text("X (East/West):   %.3f", g_localPlayerPos[0]);
                ImGui::Text("Y (Altitude/Up): %.3f", g_localPlayerPos[1]);
                ImGui::Text("Z (North/South): %.3f", g_localPlayerPos[2]);
            }

            ImGui::End();
        }

        ImGui::Render();
        g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, NULL);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    }

    return oPresent(pSwapChain, SyncInterval, Flags);
}

DWORD WINAPI ModThread(LPVOID lpParam) {
    Sleep(2000);

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
        void* pResizeBuffers = pVMT[13];

        pSwapChain->Release();
        pDevice->Release();
        pContext->Release();
        DestroyWindow(hWnd);
        UnregisterClass("DXGITest", wc.hInstance);

        if (MH_Initialize() == MH_OK) {
            MH_CreateHook(pPresent, &Hooked_Present, (LPVOID*)&oPresent);
            MH_EnableHook(pPresent);

            MH_CreateHook(pResizeBuffers, &Hooked_ResizeBuffers, (LPVOID*)&oResizeBuffers);
            MH_EnableHook(pResizeBuffers);
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
