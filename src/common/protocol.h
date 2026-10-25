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
    OP_COMBAT_EVENT         = 0x0030,
    OP_HARPOON_SYNC         = 0x0040,
    OP_CHAT_MESSAGE         = 0x0050
};

static constexpr uint32_t MMCP_MAGIC = 0x4D4D4350; // 'MMCP'

struct PacketHeader {
    uint32_t magic;         // 0x4D4D4350 ('MMCP')
    uint16_t opcode;        // PacketOpcode
    uint16_t payloadSize;   // Size of payload excluding header
    uint32_t sequence;      // Monotonically increasing sequence number
    uint32_t clientId;      // Sender client ID
};

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
    uint8_t isInVehicle;
};

struct MsgVehicleSync {
    uint32_t vehicleId;
    float posX, posY, posZ;
    float quatX, quatY, quatZ, quatW;
    float velX, velY, velZ;
    float speedKmh;
    float throttle;
    float steering;
    uint8_t handbrake;
    uint8_t boost;
    float vehicleHealth;
};

enum CombatActionType : uint8_t {
    COMBAT_ACTION_MELEE_HIT     = 1,
    COMBAT_ACTION_PARRY         = 2,
    COMBAT_ACTION_SHOTGUN_FIRE  = 3,
    COMBAT_ACTION_HARPOON_TETHER= 4
};

struct MsgCombatEvent {
    uint8_t actionType;       // CombatActionType
    uint32_t targetEntityId;  // Enemy/Car ID
    float impactPosX, impactPosY, impactPosZ;
    float damageAmount;
};

struct MsgHarpoonSync {
    uint32_t ownerClientId;
    uint8_t isAttached;
    float anchorX, anchorY, anchorZ;
    float tetherTension;
};

struct MsgChatMessage {
    char senderName[32];
    char message[128];
};

#pragma pack(pop)
