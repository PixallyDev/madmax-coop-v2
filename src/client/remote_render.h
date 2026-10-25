#pragma once
#include <windows.h>
#include <d3d11.h>
#include <vector>
#include <unordered_map>
#include <cmath>
#include <algorithm>
#include <imgui.h>
#include "../common/protocol.h"
#include "../common/camera_matrix.h"
#include "model_mesh.h"

namespace RemoteRenderer {

struct RemotePlayer {
    uint32_t clientId;
    char name[32];
    float posX, posY, posZ;
    float targetX, targetY, targetZ;
    float quatX, quatY, quatZ, quatW;
    float velX, velY, velZ;
    float speed;
    float yaw;
    float health;
    uint8_t isInVehicle;
    DWORD lastPacketTime;
};

static std::unordered_map<uint32_t, RemotePlayer> g_remotePlayers;
static int g_renderMode = 0; // 0 = Auto (Vehicle when moving/in-car, Character on foot), 1 = Force Vehicle, 2 = Force Character

inline void UpdateOrAddPlayer(uint32_t clientId, float x, float y, float z, float health,
                              float qx = 0.0f, float qy = 0.0f, float qz = 0.0f, float qw = 1.0f,
                              float vx = 0.0f, float vy = 0.0f, float vz = 0.0f, uint8_t inVeh = 0) {
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
    p.speed = sqrtf(vx * vx + vy * vy + vz * vz);
    p.health = health;
    p.isInVehicle = inVeh;
    p.lastPacketTime = GetTickCount();

    // Compute yaw from quaternion or velocity
    if (fabsf(qw) > 0.001f || fabsf(qy) > 0.001f) {
        p.yaw = atan2f(2.0f * (qw * qy + qx * qz), 1.0f - 2.0f * (qy * qy + qz * qz));
    } else if (p.speed > 0.5f) {
        p.yaw = atan2f(vx, vz);
    }

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

// -----------------------------------------------------------------------------
// Renders the Full 3D Model (Magnum Opus Vehicle or Mad Max Character)
// -----------------------------------------------------------------------------
inline void Render3DModelForPeer(ImDrawList* drawList, const RemotePlayer& p,
                                 float camX, float camY, float camZ, float camYaw,
                                 float screenW, float screenH) {
    using namespace Model3D;

    // 1. Select Mesh: Default to Magnum Opus Wasteland Vehicle in desert driving
    bool isVehicle = true;
    if (g_renderMode == 1) isVehicle = true;
    else if (g_renderMode == 2) isVehicle = false;
    else {
        isVehicle = (p.isInVehicle != 0) || (p.speed > 1.2f) || (p.posY < -0.5f || p.posY > 5.0f);
    }

    static std::vector<Vec3> s_vMeshVertices;
    static std::vector<PolyFace> s_vMeshFaces;
    static std::vector<Vec3> s_cMeshVertices;
    static std::vector<PolyFace> s_cMeshFaces;
    static bool s_bMeshesInitialized = false;

    if (!s_bMeshesInitialized) {
        VehicleModel::BuildMesh(s_vMeshVertices, s_vMeshFaces);
        CharacterModel::BuildMesh(s_cMeshVertices, s_cMeshFaces);
        s_bMeshesInitialized = true;
    }

    const std::vector<Vec3>& meshVertices = isVehicle ? s_vMeshVertices : s_cMeshVertices;
    const std::vector<PolyFace>& meshFaces = isVehicle ? s_vMeshFaces : s_cMeshFaces;

    // 2. Build Peer Rotation Matrix
    float R[3][3];
    float qx = p.quatX, qy = p.quatY, qz = p.quatZ, qw = p.quatW;
    float qLenSq = qx * qx + qy * qy + qz * qz + qw * qw;

    if (qLenSq > 0.5f && qLenSq < 2.0f) {
        // Construct from quaternion
        R[0][0] = 1.0f - 2.0f * (qy * qy + qz * qz);
        R[0][1] = 2.0f * (qx * qy - qz * qw);
        R[0][2] = 2.0f * (qx * qz + qy * qw);

        R[1][0] = 2.0f * (qx * qy + qz * qw);
        R[1][1] = 1.0f - 2.0f * (qx * qx + qz * qz);
        R[1][2] = 2.0f * (qy * qz - qx * qw);

        R[2][0] = 2.0f * (qx * qz - qy * qw);
        R[2][1] = 2.0f * (qy * qz + qx * qw);
        R[2][2] = 1.0f - 2.0f * (qx * qx + qy * qy);
    } else {
        // Fallback: Heading yaw
        float cosY = cosf(p.yaw);
        float sinY = sinf(p.yaw);
        R[0][0] =  cosY; R[0][1] = 0.0f; R[0][2] = sinY;
        R[1][0] =  0.0f; R[1][1] = 1.0f; R[1][2] = 0.0f;
        R[2][0] = -sinY; R[2][1] = 0.0f; R[2][2] = cosY;
    }

    // 3. Transform All 3D Vertices to World Space and Screen Space
    struct ProjectedVertex {
        Vec3 world;
        Vec3 cam;
        ImVec2 screen;
        bool visible;
    };

    std::vector<ProjectedVertex> projectedVertices(meshVertices.size());
    float aspect = screenW / screenH;
    float fovFactor = 0.767f; // ~75 deg FOV
    float cosCam = cosf(camYaw);
    float sinCam = sinf(camYaw);

    for (size_t i = 0; i < meshVertices.size(); i++) {
        const Vec3& v = meshVertices[i];
        ProjectedVertex& pv = projectedVertices[i];

        // A. Local to World
        pv.world.x = p.posX + (R[0][0] * v.x + R[0][1] * v.y + R[0][2] * v.z);
        pv.world.y = p.posY + (R[1][0] * v.x + R[1][1] * v.y + R[1][2] * v.z);
        pv.world.z = p.posZ + (R[2][0] * v.x + R[2][1] * v.y + R[2][2] * v.z);

        // B. World to Camera Space
        float dx = pv.world.x - camX;
        float dy = pv.world.y - camY;
        float dz = pv.world.z - camZ;

        pv.cam.x =  dx * cosCam - dz * sinCam;
        pv.cam.y =  dy;
        pv.cam.z =  dx * sinCam + dz * cosCam;

        // C. Near-Plane Clipping
        if (pv.cam.z < 0.6f) {
            pv.visible = false;
            continue;
        }

        // D. Perspective Projection to Screen Pixels
        pv.screen.x = (screenW * 0.5f) + (pv.cam.x / (pv.cam.z * fovFactor)) * (screenW * 0.5f);
        pv.screen.y = (screenH * 0.5f) - (pv.cam.y / (pv.cam.z * fovFactor * (1.0f / aspect))) * (screenH * 0.5f);
        pv.visible = true;
    }

    // 4. Directional Sun Vector in Wasteland Sky
    Vec3 sunDir = Vec3(0.35f, 0.85f, -0.40f).Normalized();

    // 5. Transform & Shade Visible Faces
    std::vector<RenderableFace> renderQueue;
    renderQueue.reserve(meshFaces.size());

    for (const auto& face : meshFaces) {
        // Check if all vertices are visible
        bool faceVisible = true;
        for (int k = 0; k < face.count; k++) {
            if (!projectedVertices[face.v[k]].visible) {
                faceVisible = false;
                break;
            }
        }
        if (!faceVisible) continue;

        const auto& pv0 = projectedVertices[face.v[0]];
        const auto& pv1 = projectedVertices[face.v[1]];
        const auto& pv2 = projectedVertices[face.v[2]];

        // World-space Face Normal
        Vec3 edge1 = pv1.world - pv0.world;
        Vec3 edge2 = pv2.world - pv0.world;
        Vec3 norm = edge1.Cross(edge2).Normalized();

        // View Vector from Face to Camera
        Vec3 viewVec = Vec3(camX - pv0.world.x, camY - pv0.world.y, camZ - pv0.world.z);

        // Back-Face Culling (Skip faces facing away from camera unless double-sided)
        if (!face.doubleSided && norm.Dot(viewVec) <= 0.0f) {
            continue;
        }

        // Sunlight Illumination Calculation
        float dotLight = max(0.0f, norm.Dot(sunDir));
        float lightFactor = 0.30f + 0.70f * dotLight;

        // Extract RGBA and apply lighting
        uint32_t a = (face.baseColor >> 24) & 0xFF;
        uint32_t b = (face.baseColor >> 16) & 0xFF;
        uint32_t g = (face.baseColor >> 8)  & 0xFF;
        uint32_t r = (face.baseColor)       & 0xFF;

        r = (uint32_t)min(255.0f, r * lightFactor);
        g = (uint32_t)min(255.0f, g * lightFactor);
        b = (uint32_t)min(255.0f, b * lightFactor);

        RenderableFace rf;
        rf.count = face.count;
        rf.shadedColor = IM_COL32(r, g, b, a);
        rf.edgeColor   = IM_COL32((uint32_t)(r * 0.5f), (uint32_t)(g * 0.5f), (uint32_t)(b * 0.5f), a);

        float depthSum = 0.0f;
        for (int k = 0; k < face.count; k++) {
            rf.screenPts[k] = projectedVertices[face.v[k]].screen;
            depthSum += projectedVertices[face.v[k]].cam.z;
        }
        rf.depth = depthSum / (float)face.count;

        renderQueue.push_back(rf);
    }

    // 6. Sort Faces by Depth (Painter's Algorithm: Draw Farthest Faces First)
    std::sort(renderQueue.begin(), renderQueue.end(), [](const RenderableFace& a, const RenderableFace& b) {
        return a.depth > b.depth;
    });

    // 7. Render 3D Model Faces to ImGui DrawList
    for (const auto& rf : renderQueue) {
        drawList->AddConvexPolyFilled(rf.screenPts, rf.count, rf.shadedColor);
        drawList->AddPolyline(rf.screenPts, rf.count, rf.edgeColor, ImDrawFlags_Closed, 1.2f);
    }

    // 8. Dynamic Wasteland Particle Effects (Tire Dust & Heat Exhaust)
    if (isVehicle && p.speed > 1.0f) {
        // Draw trailing dust clouds behind rear tires
        for (int s = 0; s < 3; s++) {
            float trailDist = 1.5f + s * 1.2f;
            float dustX = p.posX - sinCam * trailDist + ((s % 2 == 0) ? -0.8f : 0.8f) * cosCam;
            float dustZ = p.posZ - cosCam * trailDist;
            float dustY = p.posY + 0.3f + s * 0.2f;

            float ddx = dustX - camX;
            float ddz = dustZ - camZ;
            float dCamZ = ddx * sinCam + ddz * cosCam;
            if (dCamZ > 1.0f) {
                float dCamX = ddx * cosCam - ddz * sinCam;
                float dScreenX = (screenW * 0.5f) + (dCamX / (dCamZ * fovFactor)) * (screenW * 0.5f);
                float dScreenY = (screenH * 0.5f) - ((dustY - camY) / (dCamZ * fovFactor * (1.0f / aspect))) * (screenH * 0.5f);
                float dustRadius = min(35.0f, max(8.0f, (60.0f / dCamZ) * (1.0f + s * 0.6f)));

                drawList->AddCircleFilled(ImVec2(dScreenX, dScreenY), dustRadius, IM_COL32(180, 150, 110, 45 - s * 12));
            }
        }
    }

    // 9. Overhead In-World 3D Name Tag & Health Bar
    float tagWorldY = p.posY + (isVehicle ? 2.1f : 2.05f);
    float tdx = p.posX - camX;
    float tdy = tagWorldY - camY;
    float tdz = p.posZ - camZ;

    float tCamZ = tdx * sinCam + tdz * cosCam;
    if (tCamZ > 0.8f) {
        float tCamX = tdx * cosCam - tdz * sinCam;
        float tagScreenX = (screenW * 0.5f) + (tCamX / (tCamZ * fovFactor)) * (screenW * 0.5f);
        float tagScreenY = (screenH * 0.5f) - (tdy / (tCamZ * fovFactor * (1.0f / aspect))) * (screenH * 0.5f);

        char nameTag[128];
        float distToPeer = sqrtf((p.posX - camX) * (p.posX - camX) + (p.posZ - camZ) * (p.posZ - camZ));
        sprintf_s(nameTag, "%s #%u | %.1f m", isVehicle ? "Magnum Opus" : "Max (Peer)", p.clientId, distToPeer);

        ImVec2 tagSize = ImGui::CalcTextSize(nameTag);
        ImVec2 tagPos(tagScreenX - tagSize.x * 0.5f, tagScreenY - tagSize.y - 12.0f);

        drawList->AddRectFilled(ImVec2(tagPos.x - 6, tagPos.y - 3), ImVec2(tagPos.x + tagSize.x + 6, tagPos.y + tagSize.y + 3), IM_COL32(18, 18, 20, 220), 4.0f);
        drawList->AddRect(ImVec2(tagPos.x - 6, tagPos.y - 3), ImVec2(tagPos.x + tagSize.x + 6, tagPos.y + tagSize.y + 3), isVehicle ? IM_COL32(230, 160, 40, 255) : IM_COL32(50, 220, 50, 255), 4.0f, 0, 1.5f);
        drawList->AddText(tagPos, IM_COL32(255, 255, 255, 255), nameTag);

        // Health bar
        float barW = tagSize.x + 12.0f;
        float barH = 4.0f;
        ImVec2 barPos(tagScreenX - barW * 0.5f, tagPos.y + tagSize.y + 5.0f);
        drawList->AddRectFilled(barPos, ImVec2(barPos.x + barW, barPos.y + barH), IM_COL32(50, 10, 10, 220));
        drawList->AddRectFilled(barPos, ImVec2(barPos.x + barW * (p.health / 100.0f), barPos.y + barH), IM_COL32(40, 220, 60, 255));
    }
}

// -----------------------------------------------------------------------------
// Main In-World Render Entrypoint
// -----------------------------------------------------------------------------
inline void RenderWorldHUD(float localX, float localY, float localZ, float localYaw = 0.0f) {
    ImDrawList* drawList = ImGui::GetBackgroundDrawList();
    if (!drawList) return;

    ImGuiIO& io = ImGui::GetIO();
    float screenW = io.DisplaySize.x;
    float screenH = io.DisplaySize.y;

    float compassCenterX = screenW * 0.5f;
    float compassCenterY = 50.0f;

    // 1. Top Wasteland Compass
    drawList->AddCircleFilled(ImVec2(compassCenterX, compassCenterY), 24.0f, IM_COL32(15, 15, 15, 200));
    drawList->AddCircle(ImVec2(compassCenterX, compassCenterY), 24.0f, IM_COL32(230, 160, 40, 255), 32, 2.0f);
    drawList->AddText(ImVec2(compassCenterX - 4, compassCenterY - 36), IM_COL32(255, 255, 255, 255), "N");

    DWORD now = GetTickCount();

    // 2. Camera Placement: 3rd-person follow camera behind local vehicle/character
    float camDist = 4.5f;
    float camHeight = 1.8f;
    float camX = localX - sinf(localYaw) * camDist;
    float camZ = localZ - cosf(localYaw) * camDist;
    float camY = localY + camHeight;
    float camYaw = localYaw;

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

        // Render the True 3D Model for this peer
        Render3DModelForPeer(drawList, p, camX, camY, camZ, camYaw, screenW, screenH);
    }
}

}
