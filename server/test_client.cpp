#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <enet/enet.h>

#include <iostream>
#include <vector>
#include <cstring>
#include <cmath>
#include "../src/common/protocol.h"

int run_client_simulation(const char* name, int syncSeconds) {
    std::cout << "========================================" << std::endl;
    std::cout << "  Mad Max Synthetic Peer: " << name << std::endl;
    std::cout << "========================================" << std::endl;

    ENetHost* client = enet_host_create(NULL, 1, 2, 0, 0);
    if (!client) {
        std::cerr << "[" << name << "] Failed to create ENet host." << std::endl;
        return 1;
    }

    ENetAddress address;
    enet_address_set_host(&address, "127.0.0.1");
    address.port = 7777;

    std::cout << "[" << name << "] Connecting to dedicated server 127.0.0.1:7777..." << std::endl;
    ENetPeer* peer = enet_host_connect(client, &address, 2, 0);
    if (!peer) {
        std::cerr << "[" << name << "] Failed to initiate connection." << std::endl;
        enet_host_destroy(client);
        return 1;
    }

    bool connected = false;
    bool joined = false;
    uint32_t myClientId = 0;
    uint32_t seq = 1;

    // Phase 1: Wait up to 5 seconds for connection handshake
    for (int i = 0; i < 50; ++i) {
        ENetEvent event;
        while (enet_host_service(client, &event, 100) > 0) {
            if (event.type == ENET_EVENT_TYPE_CONNECT) {
                connected = true;
                std::cout << "[" << name << "] Socket connected! Sending JoinReq..." << std::endl;

                std::vector<uint8_t> reqBuf(sizeof(PacketHeader) + sizeof(MsgClientJoinReq));
                PacketHeader* hdr = reinterpret_cast<PacketHeader*>(reqBuf.data());
                hdr->magic = MMCP_MAGIC;
                hdr->opcode = OP_CLIENT_JOIN_REQ;
                hdr->payloadSize = sizeof(MsgClientJoinReq);
                hdr->sequence = seq++;
                hdr->clientId = 0;

                MsgClientJoinReq* req = reinterpret_cast<MsgClientJoinReq*>(reqBuf.data() + sizeof(PacketHeader));
                strncpy_s(req->playerName, name, sizeof(req->playerName));
                req->protocolVersion = 1;

                ENetPacket* pkt = enet_packet_create(reqBuf.data(), reqBuf.size(), ENET_PACKET_FLAG_RELIABLE);
                enet_peer_send(peer, 0, pkt);
            } else if (event.type == ENET_EVENT_TYPE_RECEIVE) {
                if (event.packet->dataLength >= sizeof(PacketHeader)) {
                    PacketHeader* hdr = reinterpret_cast<PacketHeader*>(event.packet->data);
                    if (hdr->magic == MMCP_MAGIC) {
                        uint8_t* payload = event.packet->data + sizeof(PacketHeader);
                        if (hdr->opcode == OP_CLIENT_JOIN_ACK) {
                            MsgClientJoinAck* ack = reinterpret_cast<MsgClientJoinAck*>(payload);
                            myClientId = ack->assignedClientId;
                            joined = true;
                            std::cout << "[" << name << "] Successfully joined Wasteland session!" << std::endl;
                            std::cout << "[" << name << "] Assigned ClientID: " << myClientId << " (" << ack->serverMotd << ")" << std::endl;
                        }
                    }
                }
                enet_packet_destroy(event.packet);
            }
        }
        if (joined) break;
    }

    if (!connected) {
        std::cerr << "\n[!] Could not connect to relay server at 127.0.0.1:7777." << std::endl;
        std::cerr << "[!] Make sure 'madmax_server.exe' is running first!" << std::endl;
        enet_peer_reset(peer);
        enet_host_destroy(client);
        return 2;
    }

    if (!joined) {
        std::cerr << "[!] Server connection established, but Join ACK timed out." << std::endl;
        enet_peer_disconnect(peer, 0);
        enet_host_destroy(client);
        return 3;
    }

    std::cout << "\n[" << name << "] Now broadcasting coordinates in Wasteland. (Press Ctrl+C to stop)" << std::endl;

    float angle = 0.0f;
    for (int t = 0; t < syncSeconds * 20; ++t) {
        ENetEvent event;
        while (enet_host_service(client, &event, 50) > 0) {
            if (event.type == ENET_EVENT_TYPE_RECEIVE) {
                enet_packet_destroy(event.packet);
            } else if (event.type == ENET_EVENT_TYPE_DISCONNECT) {
                std::cout << "[" << name << "] Disconnected from server." << std::endl;
                break;
            }
        }

        angle += 0.05f;
        float x = sinf(angle) * 35.0f; // Circling 35 meters out
        float z = cosf(angle) * 35.0f;
        float y = 10.0f;

        std::vector<uint8_t> syncBuf(sizeof(PacketHeader) + sizeof(MsgPlayerSync));
        PacketHeader* hdr = reinterpret_cast<PacketHeader*>(syncBuf.data());
        hdr->magic = MMCP_MAGIC;
        hdr->opcode = OP_PLAYER_SYNC;
        hdr->payloadSize = sizeof(MsgPlayerSync);
        hdr->sequence = seq++;
        hdr->clientId = myClientId;

        MsgPlayerSync* sync = reinterpret_cast<MsgPlayerSync*>(syncBuf.data() + sizeof(PacketHeader));
        sync->posX = x;
        sync->posY = y;
        sync->posZ = z;
        sync->health = 100.0f;

        ENetPacket* pkt = enet_packet_create(syncBuf.data(), syncBuf.size(), 0);
        enet_peer_send(peer, 1, pkt);

        if (t % 40 == 0) {
            std::cout << "[" << name << "] Syncing: X=" << x << ", Y=" << y << ", Z=" << z << std::endl;
        }
    }

    enet_peer_disconnect(peer, 0);
    enet_host_flush(client);
    enet_host_destroy(client);
    return 0;
}

int main(int argc, char** argv) {
    if (enet_initialize() != 0) return 1;
    const char* clientName = (argc > 1) ? argv[1] : "Furiosa";
    int res = run_client_simulation(clientName, 180); // Run for 3 minutes
    enet_deinitialize();
    return res;
}
