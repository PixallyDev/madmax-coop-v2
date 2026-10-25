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
#include "multi_instance.h"
#include "auto_scanner.h"
#include "remote_render.h"
#include "combat_sync.h"
#include "puppet_spawner.h"
#include "entity_inspector.h"
#include "havok_sync.h"

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
            char myName[32];
            sprintf_s(myName, "Player_%u", GetCurrentProcessId());
            strncpy_s(req->playerName, myName, sizeof(req->playerName));
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
                        RemoteRenderer::UpdateOrAddPlayer(hdr->clientId, sync->posX, sync->posY, sync->posZ, sync->health,
                                                          sync->quatX, sync->quatY, sync->quatZ, sync->quatW,
                                                          sync->velX, sync->velY, sync->velZ, sync->isInVehicle);
                        PuppetSpawner::SetPuppetPosition(sync->posX, sync->posY, sync->posZ);
                    } else if (hdr->opcode == OP_COMBAT_EVENT && hdr->payloadSize == sizeof(MsgCombatEvent)) {
                        MsgCombatEvent* evt = reinterpret_cast<MsgCombatEvent*>(payload);
                        CombatSync::HandleRemoteCombat(evt, hdr->clientId);
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
            PuppetSpawner::InitializeSpawnerHooks(g_gameBase);

            g_bImGuiInitialized = true;
        }
    }

    if (!g_mainRenderTargetView && g_pd3dDevice) {
        CreateRenderTarget(pSwapChain);
    }

    ServiceNetwork();

    // Priority 1: Havok Physics Character Motion Telemetry
    if (HavokSync::UpdateLocalTransform(g_gameBase)) {
        g_localPlayerPos[0] = HavokSync::g_localTransform.pos[0];
        g_localPlayerPos[1] = HavokSync::g_localTransform.pos[1];
        g_localPlayerPos[2] = HavokSync::g_localTransform.pos[2];
        g_bMemoryHooked = true;
    } else if (AutoScanner::g_bScanningActive) {
        // Priority 2: Fallback heuristic scanner
        AutoScanner::UpdateCandidates(g_localPlayerPos);
        if (AutoScanner::g_selectedPlayerAddress != 0) {
            g_targetCandidateAddress = AutoScanner::g_selectedPlayerAddress;
            g_bMemoryHooked = true;
            if (!EntityInspector::g_bInspected) {
                EntityInspector::InspectPlayerObject(g_targetCandidateAddress, g_gameBase, g_gameSize);
            }
        }
    } else if (g_bMemoryHooked && g_targetCandidateAddress != 0) {
        if (!SafeReadCoords(g_targetCandidateAddress, g_localPlayerPos)) {
            g_bMemoryHooked = false;
        }
    }

    if (g_bConnected && g_bMemoryHooked) {
        static DWORD lastSend = 0;
        DWORD now = GetTickCount();
        if (now - lastSend >= 33) {
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
            if (HavokSync::g_localTransform.isLocked) {
                HavokSync::MatrixToQuaternion(HavokSync::g_localTransform.rot, sync->quatX, sync->quatY, sync->quatZ, sync->quatW);
                sync->velX = HavokSync::g_localTransform.vel[0];
                sync->velY = HavokSync::g_localTransform.vel[1];
                sync->velZ = HavokSync::g_localTransform.vel[2];
            } else {
                sync->quatX = 0.0f; sync->quatY = 0.0f; sync->quatZ = 0.0f; sync->quatW = 1.0f;
                sync->velX = 0.0f; sync->velY = 0.0f; sync->velZ = 0.0f;
            }
            sync->health = 100.0f;
            sync->isInVehicle = (HavokSync::g_localTransform.speed > 1.2f) ? 1 : 0;

            ENetPacket* pkt = enet_packet_create(syncBuf.data(), syncBuf.size(), 0);
            enet_peer_send(g_pServerPeer, 1, pkt);
        }
    }

    RemoteRenderer::InterpolatePositions(0.016f);

    if (g_bImGuiInitialized && g_mainRenderTargetView) {
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        if (g_bConnected) {
            RemoteRenderer::RenderWorldHUD(g_localPlayerPos[0], g_localPlayerPos[1], g_localPlayerPos[2], HavokSync::g_localTransform.yaw);
            CombatSync::RenderCombatFeed();
        }

        if (g_bShowMenu) {
            ImGui::Begin("Mad Max Co-Op Multiplayer (Milestone E/I - Puppet Rig)", &g_bShowMenu, ImGuiWindowFlags_AlwaysAutoResize);
            ImGui::Text("Mad Max Multiplayer Injected (PID %u) - Press [F5] to Toggle Menu", GetCurrentProcessId());
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
            ImGui::Text("In-World 3D Model Rig (Magnum Opus / Max):");
            const char* modelModes[] = { "Auto (Vehicle in Motion / Max on Foot)", "Force 3D Magnum Opus Car", "Force 3D Mad Max Character" };
            ImGui::Combo("3D Model Render Mode", &RemoteRenderer::g_renderMode, modelModes, IM_ARRAYSIZE(modelModes));

            ImGui::Separator();
            ImGui::Text("3D Puppet & Native Engine Spawner:");
            if (!PuppetSpawner::g_bPuppetSpawned) {
                if (ImGui::Button("Spawn Peer Puppet in World")) {
                    if (g_targetCandidateAddress != 0) {
                        PuppetSpawner::SpawnPuppetAt(g_targetCandidateAddress, g_localPlayerPos[0] + 5.0f, g_localPlayerPos[1], g_localPlayerPos[2] + 5.0f);
                    }
                }
            } else {
                ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), "Peer Puppet Active @ 0x%p", (void*)PuppetSpawner::g_spawnedPuppetEntity);
            }

            ImGui::Separator();
            ImGui::Text("Co-Op Combat & Harpoon Actions:");
            if (ImGui::Button("Fire Shotgun")) {
                if (g_bConnected) {
                    std::vector<uint8_t> combatBuf(sizeof(PacketHeader) + sizeof(MsgCombatEvent));
                    PacketHeader* hdr = reinterpret_cast<PacketHeader*>(combatBuf.data());
                    hdr->magic = MMCP_MAGIC;
                    hdr->opcode = OP_COMBAT_EVENT;
                    hdr->payloadSize = sizeof(MsgCombatEvent);
                    hdr->sequence = g_packetSeq++;
                    hdr->clientId = g_assignedClientId;

                    MsgCombatEvent* evt = reinterpret_cast<MsgCombatEvent*>(combatBuf.data() + sizeof(PacketHeader));
                    evt->actionType = COMBAT_ACTION_SHOTGUN_FIRE;
                    evt->impactPosX = g_localPlayerPos[0] + 5.0f;
                    evt->impactPosY = g_localPlayerPos[1];
                    evt->impactPosZ = g_localPlayerPos[2] + 5.0f;
                    evt->damageAmount = 85.0f;

                    ENetPacket* pkt = enet_packet_create(combatBuf.data(), combatBuf.size(), ENET_PACKET_FLAG_RELIABLE);
                    enet_peer_send(g_pServerPeer, 0, pkt);
                    CombatSync::LogAction("You fired your Shotgun!", ImVec4(1.0f, 0.3f, 0.3f, 1.0f));
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("Tether Harpoon")) {
                if (g_bConnected) {
                    std::vector<uint8_t> combatBuf(sizeof(PacketHeader) + sizeof(MsgCombatEvent));
                    PacketHeader* hdr = reinterpret_cast<PacketHeader*>(combatBuf.data());
                    hdr->magic = MMCP_MAGIC;
                    hdr->opcode = OP_COMBAT_EVENT;
                    hdr->payloadSize = sizeof(MsgCombatEvent);
                    hdr->sequence = g_packetSeq++;
                    hdr->clientId = g_assignedClientId;

                    MsgCombatEvent* evt = reinterpret_cast<MsgCombatEvent*>(combatBuf.data() + sizeof(PacketHeader));
                    evt->actionType = COMBAT_ACTION_HARPOON_TETHER;
                    evt->damageAmount = 25.0f;

                    ENetPacket* pkt = enet_packet_create(combatBuf.data(), combatBuf.size(), ENET_PACKET_FLAG_RELIABLE);
                    enet_peer_send(g_pServerPeer, 0, pkt);
                    CombatSync::LogAction("You tethered Magnum Opus Harpoon!", ImVec4(0.2f, 1.0f, 0.4f, 1.0f));
                }
            }

            ImGui::Separator();
            ImGui::Text("Coordinate Scanner & Telemetry:");
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
                if (HavokSync::g_localTransform.isLocked) {
                    ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.4f, 1.0f), "Havok Rig: LOCKED @ 0x%p", (void*)HavokSync::g_pHavokMotionInstance);
                    ImGui::Text("Speed: %.1f km/h | Yaw: %.1f deg", HavokSync::g_localTransform.speed * 3.6f, HavokSync::g_localTransform.yaw * 57.2958f);
                }
            }

            ImGui::Separator();
            ImGui::Text("Engine Entity Inspector (Active Call Stack):");
            ImGui::TextWrapped("%s", EntityInspector::g_diagnosticLog);
            if (ImGui::Button("Re-Inspect Memory")) {
                EntityInspector::g_bInspected = false;
                if (g_targetCandidateAddress != 0) {
                    EntityInspector::InspectPlayerObject(g_targetCandidateAddress, g_gameBase, g_gameSize);
                }
            }

            for (size_t idx = 0; idx < EntityInspector::g_discoveredList.size(); idx++) {
                const auto& ent = EntityInspector::g_discoveredList[idx];
                bool isCore = strstr(ent.className, "Player") || strstr(ent.className, "Character") || strstr(ent.className, "Vehicle");
                ImVec4 col = isCore ? ImVec4(0.2f, 1.0f, 0.4f, 1.0f) : ImVec4(1.0f, 0.8f, 0.2f, 1.0f);
                
                char headerLabel[128];
                sprintf_s(headerLabel, "[%zu] %s @ 0x%p (Offset %c0x%tX)###ent_%zu",
                    idx + 1, ent.className, (void*)ent.baseAddress,
                    (ent.stackOffset >= 0 ? '+' : '-'), (ent.stackOffset >= 0 ? ent.stackOffset : -ent.stackOffset), idx);
                
                ImGui::PushStyleColor(ImGuiCol_Text, col);
                bool open = ImGui::TreeNode(headerLabel);
                ImGui::PopStyleColor();

                if (open) {
                    ImGui::Text("VMT: 0x%p", (void*)ent.vmtAddress);
                    ImGui::Text("Base: 0x%p", (void*)ent.baseAddress);
                    for (int m = 0; m < 8; m++) {
                        ImGui::Text("  [+0x%02X] 0x%016llX", m * 8, ent.memberPointers[m]);
                    }
                    ImGui::TreePop();
                }
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
    EnableMultiInstanceHooks();
    Sleep(1500);

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

        MH_CreateHook(pPresent, &Hooked_Present, (LPVOID*)&oPresent);
        MH_EnableHook(pPresent);

        MH_CreateHook(pResizeBuffers, &Hooked_ResizeBuffers, (LPVOID*)&oResizeBuffers);
        MH_EnableHook(pResizeBuffers);
    }

    return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    if (ul_reason_for_call == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hModule);
        if (MH_Initialize() == MH_OK) {
            EnableMultiInstanceHooks();
        }
        CreateThread(NULL, 0, ModThread, NULL, 0, NULL);
    }
    return TRUE;
}
