#pragma once
#include <windows.h>
#include <d3d11.h>
#include <cmath>

namespace MatrixMath {

struct Vector3 {
    float x, y, z;
};

struct Vector4 {
    float x, y, z, w;
};

struct Matrix4x4 {
    float m[4][4];
};

inline bool WorldToScreen(const Vector3& worldPos, const Matrix4x4& viewProj, float screenWidth, float screenHeight, Vector3& outScreen) {
    Vector4 clipCoords;
    clipCoords.x = worldPos.x * viewProj.m[0][0] + worldPos.y * viewProj.m[1][0] + worldPos.z * viewProj.m[2][0] + viewProj.m[3][0];
    clipCoords.y = worldPos.x * viewProj.m[0][1] + worldPos.y * viewProj.m[1][1] + worldPos.z * viewProj.m[2][1] + viewProj.m[3][1];
    clipCoords.z = worldPos.x * worldPos.z * viewProj.m[2][2] + viewProj.m[3][2];
    clipCoords.w = worldPos.x * viewProj.m[0][3] + worldPos.y * viewProj.m[1][3] + worldPos.z * viewProj.m[2][3] + viewProj.m[3][3];

    if (clipCoords.w < 0.1f) return false;

    Vector3 ndc;
    ndc.x = clipCoords.x / clipCoords.w;
    ndc.y = clipCoords.y / clipCoords.w;
    ndc.z = clipCoords.z / clipCoords.w;

    outScreen.x = (screenWidth / 2.0f * ndc.x) + (ndc.x + screenWidth / 2.0f);
    outScreen.y = -(screenHeight / 2.0f * ndc.y) + (ndc.y + screenHeight / 2.0f);
    outScreen.z = ndc.z;

    return true;
}

}
