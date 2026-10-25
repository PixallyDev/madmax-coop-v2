#include <iostream>
#include <vector>
#include <cstring>
#include <enet/enet.h>
#include "../src/common/protocol.h"

int run_client_simulation(const char* name, int syncTicks) {
    std::cout << "[" << name << "] Initializing..." << std::endl;
    ENetHost* client = enet_host_create(NULL, 1, 2, 0, 0);
    if (!client) {
        std::cerr << "[" << name << "] Failed to create ENet host." << std::endl;
        return 1;
    }

    ENetAddress address;
    enet_address_set_host(&address, "127.0.0.1");
    address.port = 7777;

    ENetPeer* peer = enet_host_connect(client, &address, 2, 0);
    if (!peer) {
        std::cerr << "[" << name << "] Failed to connect." << std::endl;
        enet_host_destroy(client);
        return 1;
    }

    bool joined = false;
    uint32_t myClientId = 0;
    uint32_t seq = 1;
    int receivedSyncs = 0;

    for (int t = 0; t < 40; ++t) {
        ENetEvent event;
        while (enet_host_service(client, &event, 50) > 0) {
            if (event.type == ENET_EVENT_TYPE_CONNECT) {
                std::cout << "[" << name << "] Connected to server! Sending JoinReq..." << std::endl;
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
                            std::cout << "[" << name << "] Join ACK received! Assigned ClientID=" << myClientId << " MOTD: " << ack->serverMotd << std::endl;
                        } else if (hdr->opcode == OP_PLAYER_SYNC) {
                            MsgPlayerSync* sync = reinterpret_cast<MsgPlayerSync*>(payload);
                            receivedSyncs++;
                            std::cout << "[" << name << "] Received PlayerSync from Peer " << hdr->clientId 
                                      << " Pos=(" << sync->posX << ", " << sync->posY << ", " << sync->posZ << ")" << std::endl;
                        }
                    }
                }
                enet_packet_destroy(event.packet);
            }
        }

        // Send dummy sync packet if joined
        if (joined && t % 2 == 0) {
            std::vector<uint8_t> syncBuf(sizeof(PacketHeader) + sizeof(MsgPlayerSync));
            PacketHeader* hdr = reinterpret_cast<PacketHeader*>(syncBuf.data());
            hdr->magic = MMCP_MAGIC;
            hdr->opcode = OP_PLAYER_SYNC;
            hdr->payloadSize = sizeof(MsgPlayerSync);
            hdr->sequence = seq++;
            hdr->clientId = myClientId;

            MsgPlayerSync* sync = reinterpret_cast<MsgPlayerSync*>(syncBuf.data() + sizeof(PacketHeader));
            sync->posX = 100.0f + t;
            sync->posY = 50.0f;
            sync->posZ = 200.0f + t;
            sync->health = 100.0f;

            ENetPacket* pkt = enet_packet_create(syncBuf.data(), syncBuf.size(), 0);
            enet_peer_send(peer, 1, pkt);
        }
    }

    enet_peer_disconnect(peer, 0);
    enet_host_flush(client);
    enet_host_destroy(client);
    return joined ? 0 : 1;
}

int main(int argc, char** argv) {
    if (enet_initialize() != 0) return 1;
    const char* clientName = (argc > 1) ? argv[1] : "MaxRockatansky";
    int res = run_client_simulation(clientName, 10);
    enet_deinitialize();
    return res;
}
