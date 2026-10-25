#include <iostream>
#include <enet/enet.h>
extern "C" {
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}

int main(int argc, char** argv) {
    std::cout << "========================================" << std::endl;
    std::cout << "  Mad Max Co-Op Dedicated Relay Server  " << std::endl;
    std::cout << "  Inspired by JC2-MP Architecture       " << std::endl;
    std::cout << "========================================" << std::endl;

    if (enet_initialize() != 0) {
        std::cerr << "Failed to initialize ENet!" << std::endl;
        return 1;
    }
    std::cout << "[Server] ENet initialized successfully." << std::endl;

    // Initialize Lua scripting vm
    lua_State* L = luaL_newstate();
    luaL_openlibs(L);
    std::cout << "[Server] Lua scripting runtime initialized." << std::endl;
    lua_close(L);

    ENetAddress address;
    address.host = ENET_HOST_ANY;
    address.port = 7777;

    ENetHost* server = enet_host_create(&address, 32, 2, 0, 0);
    if (!server) {
        std::cerr << "[Server] Failed to create ENet server on port 7777!" << std::endl;
        enet_deinitialize();
        return 1;
    }

    std::cout << "[Server] Listening for Wasteland warriors on port 7777..." << std::endl;

    ENetEvent event;
    while (enet_host_service(server, &event, 1000) >= 0) {
        switch (event.type) {
            case ENET_EVENT_TYPE_CONNECT:
                std::cout << "[Server] Client connected from " 
                          << event.peer->address.host << ":" << event.peer->address.port << std::endl;
                break;
            case ENET_EVENT_TYPE_RECEIVE:
                // Relay packet to other connected peers
                for (size_t i = 0; i < server->peerCount; ++i) {
                    ENetPeer* currentPeer = &server->peers[i];
                    if (currentPeer != event.peer && currentPeer->state == ENET_PEER_STATE_CONNECTED) {
                        ENetPacket* packet = enet_packet_create(event.packet->data, event.packet->dataLength, event.packet->flags);
                        enet_peer_send(currentPeer, event.channelID, packet);
                    }
                }
                enet_packet_destroy(event.packet);
                break;
            case ENET_EVENT_TYPE_DISCONNECT:
                std::cout << "[Server] Client disconnected." << std::endl;
                break;
            case ENET_EVENT_TYPE_NONE:
                break;
        }
    }

    enet_host_destroy(server);
    enet_deinitialize();
    return 0;
}
