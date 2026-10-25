#include <iostream>
#include <vector>
#include <enet/enet.h>
#include "../src/common/protocol.h"

int main() {
    if (enet_initialize() != 0) return 1;

    // Start Server
    ENetAddress srvAddr;
    srvAddr.host = ENET_HOST_ANY;
    srvAddr.port = 7779;
    ENetHost* server = enet_host_create(&srvAddr, 4, 2, 0, 0);

    // Start Client 1
    ENetHost* client1 = enet_host_create(NULL, 1, 2, 0, 0);
    ENetAddress connAddr;
    enet_address_set_host(&connAddr, "127.0.0.1");
    connAddr.port = 7779;
    ENetPeer* peer1 = enet_host_connect(client1, &connAddr, 2, 0);

    // Start Client 2
    ENetHost* client2 = enet_host_create(NULL, 1, 2, 0, 0);
    ENetPeer* peer2 = enet_host_connect(client2, &connAddr, 2, 0);

    int client1Received = 0;
    int client2Received = 0;

    for (int frame = 0; frame < 20; ++frame) {
        ENetEvent srvEvt, c1Evt, c2Evt;

        // Server tick
        while (enet_host_service(server, &srvEvt, 10) > 0) {
            if (srvEvt.type == ENET_EVENT_TYPE_RECEIVE) {
                // Relay to all other peers
                for (size_t i = 0; i < server->peerCount; ++i) {
                    if (&server->peers[i] != srvEvt.peer && server->peers[i].state == ENET_PEER_STATE_CONNECTED) {
                        ENetPacket* fwd = enet_packet_create(srvEvt.packet->data, srvEvt.packet->dataLength, srvEvt.packet->flags);
                        enet_peer_send(&server->peers[i], srvEvt.channelID, fwd);
                    }
                }
                enet_packet_destroy(srvEvt.packet);
            }
        }

        // Client 1 tick
        while (enet_host_service(client1, &c1Evt, 10) > 0) {
            if (c1Evt.type == ENET_EVENT_TYPE_RECEIVE) {
                PacketHeader* h = (PacketHeader*)c1Evt.packet->data;
                if (h->magic == MMCP_MAGIC && h->opcode == OP_PLAYER_SYNC) {
                    client1Received++;
                }
                enet_packet_destroy(c1Evt.packet);
            }
        }

        // Client 2 tick
        while (enet_host_service(client2, &c2Evt, 10) > 0) {
            if (c2Evt.type == ENET_EVENT_TYPE_RECEIVE) {
                PacketHeader* h = (PacketHeader*)c2Evt.packet->data;
                if (h->magic == MMCP_MAGIC && h->opcode == OP_PLAYER_SYNC) {
                    client2Received++;
                }
                enet_packet_destroy(c2Evt.packet);
            }
        }

        // Client 1 sends a packet on frame 5
        if (frame == 5) {
            std::vector<uint8_t> buf(sizeof(PacketHeader) + sizeof(MsgPlayerSync));
            PacketHeader* h = (PacketHeader*)buf.data();
            h->magic = MMCP_MAGIC;
            h->opcode = OP_PLAYER_SYNC;
            h->payloadSize = sizeof(MsgPlayerSync);
            h->sequence = 1;
            h->clientId = 1;
            ENetPacket* p = enet_packet_create(buf.data(), buf.size(), ENET_PACKET_FLAG_RELIABLE);
            enet_peer_send(peer1, 0, p);
        }

        // Client 2 sends a packet on frame 10
        if (frame == 10) {
            std::vector<uint8_t> buf(sizeof(PacketHeader) + sizeof(MsgPlayerSync));
            PacketHeader* h = (PacketHeader*)buf.data();
            h->magic = MMCP_MAGIC;
            h->opcode = OP_PLAYER_SYNC;
            h->payloadSize = sizeof(MsgPlayerSync);
            h->sequence = 2;
            h->clientId = 2;
            ENetPacket* p = enet_packet_create(buf.data(), buf.size(), ENET_PACKET_FLAG_RELIABLE);
            enet_peer_send(peer2, 0, p);
        }
    }

    std::cout << "[TestSim] Client1 received from Client2: " << client1Received << std::endl;
    std::cout << "[TestSim] Client2 received from Client1: " << client2Received << std::endl;

    enet_host_destroy(client1);
    enet_host_destroy(client2);
    enet_host_destroy(server);
    enet_deinitialize();

    return (client1Received > 0 && client2Received > 0) ? 0 : 1;
}
