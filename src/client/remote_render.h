#pragma once
#include <windows.h>
#include <d3d11.h>
#include <vector>
#include <unordered_map>
#include <cmath>
#include <imgui.h>
#include "../common/protocol.h"
#include "../common/camera_matrix.h"

namespace RemoteRenderer {

struct RemotePlayer {
    uint32_t clientId;
    char name[32];
    float posX, posY, posZ;
    float targetX, targetY, targetZ;
    float health;
    DWORD lastPacketTime;
};

static std::unordered_map<uint32_t, RemotePlayer> g_remotePlayers;

inline void UpdateOrAddPlayer(uint32_t clientId, float x, float y, float z, float health) {
    auto& p = g_remotePlayers[clientId];
    p.clientId = clientId;
    p.targetX = x;
    p.targetY = y;
    p.targetZ = z;
    p.health = health;
    p.lastPacketTime = GetTickCount();

    if (p.posX == 0.0f && p.posY == 0.0f && p.posZ == 0.0f) {
        p.posX = x;
        p.posY = y;
        p.posZ = z;
    }
}

inline void InterpolatePositions(float dt) {
    for (auto& pair : g_remotePlayers) {
        auto& p = pair.second;
        p.posX += (p.targetX - p.posX) * min(1.0f, dt * 15.0f);
        p.posY += (p.targetY - p.posY) * min(1.0f, dt * 15.0f);
        p.posZ += (p.targetZ - p.posZ) * min(1.0f, dt * 15.0f);
    }
}

// Renders the 3D in-world Wasteland compass and 3D puppet proxy
inline void RenderWorldHUD(float localX, float localY, float localZ) {
    ImDrawList* drawList = ImGui::GetBackgroundDrawList();
    if (!drawList) return;

    ImGuiIO& io = ImGui::GetIO();
    float screenW = io.DisplaySize.x;
    float screenH = io.DisplaySize.y;

    float compassCenterX = screenW * 0.5f;
    float compassCenterY = 50.0f;

    // Outer ring & North pointer
    drawList->AddCircleFilled(ImVec2(compassCenterX, compassCenterY), 24.0f, IM_COL32(15, 15, 15, 200));
    drawList->AddCircle(ImVec2(compassCenterX, compassCenterY), 24.0f, IM_COL32(230, 160, 40, 255), 32, 2.0f);
    drawList->AddText(ImVec2(compassCenterX - 4, compassCenterY - 36), IM_COL32(255, 255, 255, 255), "N");

    DWORD now = GetTickCount();

    for (const auto& pair : g_remotePlayers) {
        const auto& p = pair.second;
        if (now - p.lastPacketTime > 10000) continue;

        float dx = p.posX - localX;
        float dz = p.posZ - localZ;
        float dy = p.posY - localY;
        float dist = sqrtf(dx * dx + dz * dz + dy * dy);

        // Top compass blip
        float normX = dx / max(1.0f, dist);
        float normZ = dz / max(1.0f, dist);
        float blipDist = min(20.0f, (dist / 100.0f) * 20.0f);

        ImVec2 blipPos(compassCenterX + normX * blipDist, compassCenterY - normZ * blipDist);
        drawList->AddCircleFilled(blipPos, 4.5f, IM_COL32(50, 255, 50, 255));
        drawList->AddCircle(blipPos, 5.5f, IM_COL32(255, 255, 255, 220), 16, 1.0f);

        // Distance & Status banner
        char tag[128];
        sprintf_s(tag, "[P2 Peer #%u] %.1f m | Co-Op Sync Active", p.clientId, dist);
        
        ImVec2 textSize = ImGui::CalcTextSize(tag);
        float tagX = compassCenterX - textSize.x * 0.5f;
        float tagY = 85.0f;

        drawList->AddRectFilled(ImVec2(tagX - 8, tagY - 2), ImVec2(tagX + textSize.x + 8, tagY + textSize.y + 4), IM_COL32(20, 20, 20, 180), 4.0f);
        drawList->AddRect(ImVec2(tagX - 8, tagY - 2), ImVec2(tagX + textSize.x + 8, tagY + textSize.y + 4), IM_COL32(50, 255, 50, 255), 4.0f);
        drawList->AddText(ImVec2(tagX, tagY), IM_COL32(255, 255, 255, 255), tag);
    }
}

}
