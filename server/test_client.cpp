#include <iostream>
#include <enet/enet.h>

int main() {
    std::cout << "[TestClient] Initializing ENet..." << std::endl;
    if (enet_initialize() != 0) {
        std::cerr << "[TestClient] Failed to initialize ENet." << std::endl;
        return 1;
    }

    ENetHost* client = enet_host_create(NULL, 1, 2, 0, 0);
    if (!client) {
        std::cerr << "[TestClient] Failed to create ENet client host." << std::endl;
        enet_deinitialize();
        return 1;
    }

    ENetAddress address;
    enet_address_set_host(&address, "127.0.0.1");
    address.port = 7777;

    ENetPeer* peer = enet_host_connect(client, &address, 2, 0);
    if (!peer) {
        std::cerr << "[TestClient] No available peers for initiating an ENet connection." << std::endl;
        enet_host_destroy(client);
        enet_deinitialize();
        return 1;
    }

    ENetEvent event;
    bool connected = false;
    for (int i = 0; i < 20; ++i) {
        if (enet_host_service(client, &event, 250) > 0) {
            if (event.type == ENET_EVENT_TYPE_CONNECT) {
                std::cout << "[TestClient] Successfully connected to 127.0.0.1:7777!" << std::endl;
                connected = true;
                break;
            }
        }
    }

    if (!connected) {
        std::cout << "[TestClient] Connection timed out (expected if server not running)." << std::endl;
    }

    enet_peer_reset(peer);
    enet_host_destroy(client);
    enet_deinitialize();
    return connected ? 0 : 2;
}
