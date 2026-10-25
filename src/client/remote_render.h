#pragma once
#include <windows.h>
#include <d3d11.h>
#include <vector>
#include <unordered_map>
#include <imgui.h>
#include "../common/protocol.h"

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

    // If new, initialize position
    if (p.posX == 0.0f && p.posY == 0.0f && p.posZ == 0.0f) {
        p.posX = x;
        p.posY = y;
        p.posZ = z;
    }
}

// Hermite / linear interpolation on client tick (smooth 60fps)
inline void InterpolatePositions(float dt) {
    for (auto& pair : g_remotePlayers) {
        auto& p = pair.second;
        // Smooth lerp 15.0f factor
        p.posX += (p.targetX - p.posX) * min(1.0f, dt * 15.0f);
        p.posY += (p.targetY - p.posY) * min(1.0f, dt * 15.0f);
        p.posZ += (p.targetZ - p.posZ) * min(1.0f, dt * 15.0f);
    }
}

// Render 3D Radar / Compass & World Markers
inline void RenderWorldHUD(float localX, float localY, float localZ) {
    ImDrawList* drawList = ImGui::GetBackgroundDrawList();
    if (!drawList) return;

    ImGuiIO& io = ImGui::GetIO();
    float screenW = io.DisplaySize.x;
    float screenH = io.DisplaySize.y;

    // Draw Wasteland Compass HUD at top center
    float compassCenterX = screenW * 0.5f;
    float compassCenterY = 50.0f;

    drawList->AddCircleFilled(ImVec2(compassCenterX, compassCenterY), 24.0f, IM_COL32(20, 20, 20, 180));
    drawList->AddCircle(ImVec2(compassCenterX, compassCenterY), 24.0f, IM_COL32(200, 150, 50, 255), 32, 2.0f);
    drawList->AddText(ImVec2(compassCenterX - 4, compassCenterY - 36), IM_COL32(255, 255, 255, 255), "N");

    DWORD now = GetTickCount();

    for (const auto& pair : g_remotePlayers) {
        const auto& p = pair.second;
        if (now - p.lastPacketTime > 10000) continue; // Timed out

        float dx = p.posX - localX;
        float dz = p.posZ - localZ;
        float dy = p.posY - localY;
        float dist = sqrtf(dx * dx + dz * dz + dy * dy);

        // Compass blip
        float normX = dx / max(1.0f, dist);
        float normZ = dz / max(1.0f, dist);
        float blipDist = min(20.0f, (dist / 100.0f) * 20.0f);

        ImVec2 blipPos(compassCenterX + normX * blipDist, compassCenterY - normZ * blipDist);
        drawList->AddCircleFilled(blipPos, 4.0f, IM_COL32(50, 255, 50, 255));

        // Draw In-World Tag / Floating Marker on Screen
        // Simple screen projection preview
        char tag[64];
        sprintf_s(tag, "[P2] Wastelander #%u (%.1fm)", p.clientId, dist);
        drawList->AddText(ImVec2(screenW * 0.5f - 50.0f, 90.0f), IM_COL32(50, 255, 50, 255), tag);
    }
}

}
