#include <iostream>
#include <vector>
#include <unordered_map>
#include <enet/enet.h>
#include "../src/common/protocol.h"
extern "C" {
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}

struct ConnectedPeerInfo {
    uint32_t clientId;
    std::string playerName;
    uint32_t lastSequence;
};

static std::unordered_map<ENetPeer*, ConnectedPeerInfo> g_peers;
static uint32_t g_nextClientId = 100;

int main(int argc, char** argv) {
    std::cout << "============================================" << std::endl;
    std::cout << "  Mad Max Co-Op Dedicated Relay Server v1.0 " << std::endl;
    std::cout << "  Protocol Magic: MMCP (0x4D4D4350)         " << std::endl;
    std::cout << "============================================" << std::endl;

    if (enet_initialize() != 0) {
        std::cerr << "[Server] Failed to initialize ENet!" << std::endl;
        return 1;
    }

    lua_State* L = luaL_newstate();
    luaL_openlibs(L);
    std::cout << "[Server] Lua 5.4 scripting environment initialized." << std::endl;

    ENetAddress address;
    address.host = ENET_HOST_ANY;
    address.port = 7777;

    ENetHost* server = enet_host_create(&address, 32, 2, 0, 0);
    if (!server) {
        std::cerr << "[Server] Failed to bind to port 7777!" << std::endl;
        enet_deinitialize();
        return 1;
    }

    std::cout << "[Server] Server listening on 0.0.0.0:7777 for Wasteland peers..." << std::endl;

    ENetEvent event;
    while (enet_host_service(server, &event, 1000) >= 0) {
        switch (event.type) {
            case ENET_EVENT_TYPE_CONNECT: {
                uint32_t newId = g_nextClientId++;
                g_peers[event.peer] = { newId, "Wastelander", 0 };
                std::cout << "[Server] Peer connected. Assigned ClientID: " << newId << std::endl;
                break;
            }
            case ENET_EVENT_TYPE_RECEIVE: {
                if (event.packet->dataLength >= sizeof(PacketHeader)) {
                    PacketHeader* hdr = reinterpret_cast<PacketHeader*>(event.packet->data);
                    if (hdr->magic == MMCP_MAGIC) {
                        uint8_t* payload = event.packet->data + sizeof(PacketHeader);
                        if (hdr->opcode == OP_CLIENT_JOIN_REQ && hdr->payloadSize == sizeof(MsgClientJoinReq)) {
                            MsgClientJoinReq* req = reinterpret_cast<MsgClientJoinReq*>(payload);
                            g_peers[event.peer].playerName = req->playerName;
                            std::cout << "[Server] Received JoinReq from '" << req->playerName << "' (ClientID " << g_peers[event.peer].clientId << ")" << std::endl;

                            // Send ACK
                            std::vector<uint8_t> ackBuf(sizeof(PacketHeader) + sizeof(MsgClientJoinAck));
                            PacketHeader* ackHdr = reinterpret_cast<PacketHeader*>(ackBuf.data());
                            ackHdr->magic = MMCP_MAGIC;
                            ackHdr->opcode = OP_CLIENT_JOIN_ACK;
                            ackHdr->payloadSize = sizeof(MsgClientJoinAck);
                            ackHdr->sequence = 1;
                            ackHdr->clientId = g_peers[event.peer].clientId;

                            MsgClientJoinAck* ack = reinterpret_cast<MsgClientJoinAck*>(ackBuf.data() + sizeof(PacketHeader));
                            ack->assignedClientId = g_peers[event.peer].clientId;
                            ack->totalPeersConnected = static_cast<uint32_t>(g_peers.size());
                            strncpy_s(ack->serverMotd, "Welcome to the Wasteland Co-Op Server!", sizeof(ack->serverMotd));

                            ENetPacket* resp = enet_packet_create(ackBuf.data(), ackBuf.size(), ENET_PACKET_FLAG_RELIABLE);
                            enet_peer_send(event.peer, 0, resp);
                        } else {
                            // Valid MMCP packet: update sequence and relay to all other peers
                            g_peers[event.peer].lastSequence = hdr->sequence;
                            hdr->clientId = g_peers[event.peer].clientId; // Enforce correct client id attribution

                            for (size_t i = 0; i < server->peerCount; ++i) {
                                ENetPeer* currentPeer = &server->peers[i];
                                if (currentPeer != event.peer && currentPeer->state == ENET_PEER_STATE_CONNECTED) {
                                    ENetPacket* fwd = enet_packet_create(event.packet->data, event.packet->dataLength, event.packet->flags);
                                    enet_peer_send(currentPeer, event.channelID, fwd);
                                }
                            }
                        }
                    } else {
                        std::cerr << "[Server] Dropped malformed packet with invalid magic: 0x" << std::hex << hdr->magic << std::endl;
                    }
                }
                enet_packet_destroy(event.packet);
                break;
            }
            case ENET_EVENT_TYPE_DISCONNECT: {
                auto it = g_peers.find(event.peer);
                if (it != g_peers.end()) {
                    std::cout << "[Server] ClientID " << it->second.clientId << " (" << it->second.playerName << ") disconnected." << std::endl;
                    g_peers.erase(it);
                }
                break;
            }
            case ENET_EVENT_TYPE_NONE:
                break;
        }
    }

    lua_close(L);
    enet_host_destroy(server);
    enet_deinitialize();
    return 0;
}
