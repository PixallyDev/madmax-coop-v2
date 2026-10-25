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
    float quatX, quatY, quatZ, quatW;
    float velX, velY, velZ;
    float yaw;
    float health;
    DWORD lastPacketTime;
};

static std::unordered_map<uint32_t, RemotePlayer> g_remotePlayers;

inline void UpdateOrAddPlayer(uint32_t clientId, float x, float y, float z, float health,
                              float qx = 0.0f, float qy = 0.0f, float qz = 0.0f, float qw = 1.0f,
                              float vx = 0.0f, float vy = 0.0f, float vz = 0.0f) {
    auto& p = g_remotePlayers[clientId];
    p.clientId = clientId;
    p.targetX = x;
    p.targetY = y;
    p.targetZ = z;
    p.quatX = qx;
    p.quatY = qy;
    p.quatZ = qz;
    p.quatW = qw;
    p.velX = vx;
    p.velY = vy;
    p.velZ = vz;
    p.health = health;
    p.lastPacketTime = GetTickCount();

    // Compute yaw from quaternion
    p.yaw = atan2f(2.0f * (qw * qy + qx * qz), 1.0f - 2.0f * (qy * qy + qz * qz));

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

// Renders the 3D in-world Wasteland compass and in-world 3D Puppet Rig
inline void RenderWorldHUD(float localX, float localY, float localZ, float localYaw = 0.0f) {
    ImDrawList* drawList = ImGui::GetBackgroundDrawList();
    if (!drawList) return;

    ImGuiIO& io = ImGui::GetIO();
    float screenW = io.DisplaySize.x;
    float screenH = io.DisplaySize.y;

    float compassCenterX = screenW * 0.5f;
    float compassCenterY = 50.0f;

    // Top Wasteland Compass
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

        // Compass blip
        float normX = dx / max(1.0f, dist);
        float normZ = dz / max(1.0f, dist);
        float blipDist = min(20.0f, (dist / 100.0f) * 20.0f);

        ImVec2 blipPos(compassCenterX + normX * blipDist, compassCenterY - normZ * blipDist);
        drawList->AddCircleFilled(blipPos, 4.5f, IM_COL32(50, 255, 50, 255));
        drawList->AddCircle(blipPos, 5.5f, IM_COL32(255, 255, 255, 220), 16, 1.0f);

        // --- 3D IN-WORLD PUPPET RIG PROJECTION ---
        // Compute relative angle to camera
        float angleToPeer = atan2f(dx, dz);
        float relAngle = angleToPeer - localYaw;
        while (relAngle > 3.14159f) relAngle -= 6.28318f;
        while (relAngle < -3.14159f) relAngle += 6.28318f;

        // In front of camera (FOV approx 90 degrees = +/- 45 deg = 0.78 rad)
        if (fabsf(relAngle) < 1.2f && dist > 0.5f) {
            float screenX = (screenW * 0.5f) + (relAngle / 0.8f) * (screenW * 0.5f);
            float screenY = (screenH * 0.5f) - (dy / max(1.0f, dist * 0.5f)) * 200.0f;

            // Perspective scale factor
            float puppetScale = max(12.0f, min(120.0f, 1500.0f / max(3.0f, dist)));

            // 1. Draw 3D Puppet Head
            ImVec2 headPos(screenX, screenY - puppetScale * 0.9f);
            drawList->AddCircleFilled(headPos, puppetScale * 0.18f, IM_COL32(240, 200, 160, 255));
            drawList->AddCircle(headPos, puppetScale * 0.18f, IM_COL32(20, 20, 20, 255), 16, 1.5f);

            // 2. Draw 3D Puppet Torso & Spine
            ImVec2 neckPos(screenX, screenY - puppetScale * 0.72f);
            ImVec2 pelvisPos(screenX, screenY - puppetScale * 0.35f);
            drawList->AddLine(neckPos, pelvisPos, IM_COL32(40, 40, 40, 255), puppetScale * 0.28f);

            // 3. Draw Shoulders & Arms
            float shoulderW = puppetScale * 0.32f;
            ImVec2 shoulderL(screenX - shoulderW, screenY - puppetScale * 0.68f);
            ImVec2 shoulderR(screenX + shoulderW, screenY - puppetScale * 0.68f);
            ImVec2 handL(screenX - shoulderW * 1.1f, screenY - puppetScale * 0.40f);
            ImVec2 handR(screenX + shoulderW * 1.1f, screenY - puppetScale * 0.40f);
            drawList->AddLine(shoulderL, shoulderR, IM_COL32(180, 130, 80, 255), 3.0f);
            drawList->AddLine(shoulderL, handL, IM_COL32(200, 150, 90, 255), 2.5f);
            drawList->AddLine(shoulderR, handR, IM_COL32(200, 150, 90, 255), 2.5f);

            // 4. Draw Legs & Feet
            float legW = puppetScale * 0.16f;
            ImVec2 footL(screenX - legW, screenY);
            ImVec2 footR(screenX + legW, screenY);
            drawList->AddLine(pelvisPos, footL, IM_COL32(50, 50, 60, 255), 3.0f);
            drawList->AddLine(pelvisPos, footR, IM_COL32(50, 50, 60, 255), 3.0f);

            // 5. Draw Facing Direction Indicator
            float peerHeadingX = sinf(p.yaw) * puppetScale * 0.45f;
            float peerHeadingY = -cosf(p.yaw) * puppetScale * 0.2f;
            drawList->AddLine(headPos, ImVec2(headPos.x + peerHeadingX, headPos.y + peerHeadingY), IM_COL32(255, 230, 50, 255), 2.0f);

            // 6. Overhead In-World Name Tag & Health
            char nameTag[128];
            sprintf_s(nameTag, "Max #%u (Peer) | %.1f m", p.clientId, dist);
            ImVec2 tagSize = ImGui::CalcTextSize(nameTag);
            ImVec2 tagPos(screenX - tagSize.x * 0.5f, screenY - puppetScale * 1.35f);

            drawList->AddRectFilled(ImVec2(tagPos.x - 6, tagPos.y - 2), ImVec2(tagPos.x + tagSize.x + 6, tagPos.y + tagSize.y + 2), IM_COL32(20, 20, 20, 200), 3.0f);
            drawList->AddRect(ImVec2(tagPos.x - 6, tagPos.y - 2), ImVec2(tagPos.x + tagSize.x + 6, tagPos.y + tagSize.y + 2), IM_COL32(50, 255, 50, 255), 3.0f);
            drawList->AddText(tagPos, IM_COL32(255, 255, 255, 255), nameTag);

            // Health bar
            float barW = tagSize.x + 12.0f;
            float barH = 4.0f;
            ImVec2 barPos(screenX - barW * 0.5f, tagPos.y + tagSize.y + 4.0f);
            drawList->AddRectFilled(barPos, ImVec2(barPos.x + barW, barPos.y + barH), IM_COL32(60, 10, 10, 220));
            drawList->AddRectFilled(barPos, ImVec2(barPos.x + barW * (p.health / 100.0f), barPos.y + barH), IM_COL32(40, 220, 60, 255));
        }
    }
}

}
