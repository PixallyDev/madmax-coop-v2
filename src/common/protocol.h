#pragma once
#include <cstdint>

#pragma pack(push, 1)

enum PacketOpcode : uint16_t {
    OP_HEARTBEAT            = 0x0001,
    OP_CLIENT_JOIN_REQ      = 0x0002,
    OP_CLIENT_JOIN_ACK      = 0x0003,
    OP_CLIENT_LEAVE         = 0x0004,
    OP_PLAYER_SYNC          = 0x0010,
    OP_VEHICLE_SYNC         = 0x0020,
    OP_CHAT_MESSAGE         = 0x0030
};

struct PacketHeader {
    uint32_t magic;         // 0x4D4D4350 ('MMCP' - Mad Max Co-op Protocol)
    uint16_t opcode;        // PacketOpcode
    uint16_t payloadSize;   // Size of payload excluding header
    uint32_t sequence;      // Monotonically increasing sequence number
    uint32_t clientId;      // Sender client ID (assigned by server)
};

static constexpr uint32_t MMCP_MAGIC = 0x4D4D4350;

struct MsgClientJoinReq {
    char playerName[32];
    uint32_t protocolVersion; // 1
};

struct MsgClientJoinAck {
    uint32_t assignedClientId;
    uint32_t totalPeersConnected;
    char serverMotd[64];
};

struct MsgPlayerSync {
    float posX, posY, posZ;
    float quatX, quatY, quatZ, quatW;
    float velX, velY, velZ;
    uint32_t animState;
    float health;
};

struct MsgVehicleSync {
    uint32_t vehicleId;
    float posX, posY, posZ;
    float quatX, quatY, quatZ, quatW;
    float velX, velY, velZ;
    float throttle;
    float steering;
    uint8_t handbrake;
    uint8_t boost;
};

struct MsgChatMessage {
    char senderName[32];
    char message[128];
};

#pragma pack(pop)
