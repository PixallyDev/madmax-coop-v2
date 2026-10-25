#pragma once
#include <windows.h>
#include <vector>
#include <cmath>
#include <algorithm>
#include <imgui.h>

namespace Model3D {

struct Vec3 {
    float x, y, z;

    Vec3() : x(0), y(0), z(0) {}
    Vec3(float _x, float _y, float _z) : x(_x), y(_y), z(_z) {}

    Vec3 operator+(const Vec3& o) const { return Vec3(x + o.x, y + o.y, z + o.z); }
    Vec3 operator-(const Vec3& o) const { return Vec3(x - o.x, y - o.y, z - o.z); }
    Vec3 operator*(float s) const { return Vec3(x * s, y * s, z * s); }

    float Dot(const Vec3& o) const { return x * o.x + y * o.y + z * o.z; }

    Vec3 Cross(const Vec3& o) const {
        return Vec3(y * o.z - z * o.y, z * o.x - x * o.z, x * o.y - y * o.x);
    }

    Vec3 Normalized() const {
        float len = sqrtf(x * x + y * y + z * z);
        return len > 0.0001f ? Vec3(x / len, y / len, z / len) : Vec3(0, 1, 0);
    }
};

struct PolyFace {
    int v[4];
    int count;
    uint32_t baseColor;
    bool doubleSided;
};

struct RenderableFace {
    ImVec2 screenPts[4];
    int count;
    float depth;
    uint32_t shadedColor;
    uint32_t edgeColor;
};

// =========================================================================
// 1. MAGNUM OPUS 3D VEHICLE MODEL (Wasteland Combat Car)
// =========================================================================
class VehicleModel {
public:
    static void BuildMesh(std::vector<Vec3>& outVertices, std::vector<PolyFace>& outFaces) {
        outVertices.clear();
        outFaces.clear();

        // --- Main Chassis & Body Vertices (0 - 15) ---
        outVertices.push_back(Vec3(-0.95f, 0.35f, -2.10f)); // 0: BL rear
        outVertices.push_back(Vec3( 0.95f, 0.35f, -2.10f)); // 1: BR rear
        outVertices.push_back(Vec3( 0.95f, 0.35f,  2.10f)); // 2: BR front
        outVertices.push_back(Vec3(-0.95f, 0.35f,  2.10f)); // 3: BL front

        outVertices.push_back(Vec3(-0.90f, 0.72f,  2.10f)); // 4: Hood front L
        outVertices.push_back(Vec3( 0.90f, 0.72f,  2.10f)); // 5: Hood front R
        outVertices.push_back(Vec3( 0.85f, 0.88f,  0.55f)); // 6: Hood base R
        outVertices.push_back(Vec3(-0.85f, 0.88f,  0.55f)); // 7: Hood base L

        outVertices.push_back(Vec3(-0.68f, 1.38f,  0.10f)); // 8: Roof front L
        outVertices.push_back(Vec3( 0.68f, 1.38f,  0.10f)); // 9: Roof front R
        outVertices.push_back(Vec3( 0.68f, 1.32f, -0.90f)); // 10: Roof rear R
        outVertices.push_back(Vec3(-0.68f, 1.32f, -0.90f)); // 11: Roof rear L

        outVertices.push_back(Vec3(-0.88f, 0.78f, -2.10f)); // 12: Trunk rear L
        outVertices.push_back(Vec3( 0.88f, 0.78f, -2.10f)); // 13: Trunk rear R
        outVertices.push_back(Vec3( 0.85f, 0.82f, -1.10f)); // 14: Deck mid R
        outVertices.push_back(Vec3(-0.85f, 0.82f, -1.10f)); // 15: Deck mid L

        // --- Supercharger / Engine Blower Scoop Vertices (16 - 23) ---
        outVertices.push_back(Vec3(-0.25f, 0.82f, 1.35f));  // 16
        outVertices.push_back(Vec3( 0.25f, 0.82f, 1.35f));  // 17
        outVertices.push_back(Vec3( 0.25f, 0.82f, 0.75f));  // 18
        outVertices.push_back(Vec3(-0.25f, 0.82f, 0.75f));  // 19
        outVertices.push_back(Vec3(-0.22f, 1.10f, 1.40f));  // 20
        outVertices.push_back(Vec3( 0.22f, 1.10f, 1.40f));  // 21
        outVertices.push_back(Vec3( 0.22f, 1.05f, 0.70f));  // 22
        outVertices.push_back(Vec3(-0.22f, 1.05f, 0.70f));  // 23

        // --- Front Bull-Bar / Ramming Cowcatcher (24 - 29) ---
        outVertices.push_back(Vec3(-0.95f, 0.40f, 2.45f));  // 24: Bullbar bottom L
        outVertices.push_back(Vec3( 0.95f, 0.40f, 2.45f));  // 25: Bullbar bottom R
        outVertices.push_back(Vec3( 0.70f, 0.95f, 2.48f));  // 26: Bullbar top R
        outVertices.push_back(Vec3(-0.70f, 0.95f, 2.48f));  // 27: Bullbar top L
        outVertices.push_back(Vec3( 0.00f, 1.05f, 2.55f));  // 28: Bullbar spike center
        outVertices.push_back(Vec3( 0.00f, 0.38f, 2.50f));  // 29: Bullbar bottom center

        // Colors (RGBA)
        uint32_t colBodyDark   = IM_COL32(45, 48, 52, 255);    // Weathered Wasteland Steel
        uint32_t colHood       = IM_COL32(85, 55, 38, 255);    // Rust & Matte Primer
        uint32_t colGlass      = IM_COL32(20, 32, 42, 240);    // Reinforced Windshield Louvers
        uint32_t colRoof       = IM_COL32(52, 54, 58, 255);    // Steel Roof Panel
        uint32_t colChrome     = IM_COL32(195, 200, 210, 255); // Supercharger Chrome
        uint32_t colBullbar    = IM_COL32(32, 32, 35, 255);    // Heavy Iron Ram
        uint32_t colGrill      = IM_COL32(25, 25, 25, 255);    // Front Radiator Mesh
        uint32_t colTail       = IM_COL32(180, 35, 25, 255);   // Red Tail Lamps

        // Faces (Double-sided for 100% reliable visibility from any angle)
        outFaces.push_back({ {4, 5, 6, 7}, 4, colHood, true });         // Hood
        outFaces.push_back({ {7, 6, 9, 8}, 4, colGlass, true });        // Windshield
        outFaces.push_back({ {8, 9, 10, 11}, 4, colRoof, true });       // Roof
        outFaces.push_back({ {11, 10, 14, 15}, 4, colGlass, true });    // Rear Windshield
        outFaces.push_back({ {15, 14, 13, 12}, 4, colBodyDark, true }); // Trunk Deck
        outFaces.push_back({ {3, 2, 5, 4}, 4, colGrill, true });        // Front Grille
        outFaces.push_back({ {1, 0, 12, 13}, 4, colTail, true });       // Rear Tail Panel

        // Side Panels
        outFaces.push_back({ {0, 3, 4, 12}, 4, colBodyDark, true });    // Left Side Lower
        outFaces.push_back({ {2, 1, 13, 5}, 4, colBodyDark, true });    // Right Side Lower
        outFaces.push_back({ {7, 8, 11, 15}, 4, colGlass, true });      // Left Cabin Window
        outFaces.push_back({ {9, 6, 14, 10}, 4, colGlass, true });      // Right Cabin Window

        // Supercharger Scoop
        outFaces.push_back({ {20, 21, 22, 23}, 4, colChrome, true });   // Blower Top
        outFaces.push_back({ {16, 17, 21, 20}, 4, colChrome, true });   // Blower Front
        outFaces.push_back({ {19, 16, 20, 23}, 4, colChrome, true });   // Blower Left
        outFaces.push_back({ {17, 18, 22, 21}, 4, colChrome, true });   // Blower Right

        // Ramming Bull-Bar
        outFaces.push_back({ {24, 29, 28, 27}, 4, colBullbar, true });   // Ram Left Wedge
        outFaces.push_back({ {29, 25, 26, 28}, 4, colBullbar, true });   // Ram Right Wedge

        // --- 4 Independent 3D Wheels ---
        Vec3 wheelCenters[4] = {
            Vec3(-1.02f, 0.42f,  1.35f), // Front Left
            Vec3( 1.02f, 0.42f,  1.35f), // Front Right
            Vec3(-1.02f, 0.46f, -1.35f), // Rear Left
            Vec3( 1.02f, 0.46f, -1.35f)  // Rear Right
        };

        for (int w = 0; w < 4; w++) {
            float r = (w >= 2) ? 0.46f : 0.42f;
            float width = 0.28f;
            float sideSign = (wheelCenters[w].x > 0.0f) ? 1.0f : -1.0f;

            int baseIdx = (int)outVertices.size();
            for (int s = 0; s < 8; s++) {
                float angle = s * (6.283185f / 8.0f);
                float dy = sinf(angle) * r;
                float dz = cosf(angle) * r;

                outVertices.push_back(Vec3(wheelCenters[w].x + sideSign * width, wheelCenters[w].y + dy, wheelCenters[w].z + dz));
                outVertices.push_back(Vec3(wheelCenters[w].x, wheelCenters[w].y + dy, wheelCenters[w].z + dz));
            }

            int hubIdx = (int)outVertices.size();
            outVertices.push_back(Vec3(wheelCenters[w].x + sideSign * (width + 0.02f), wheelCenters[w].y, wheelCenters[w].z));

            uint32_t colTread = IM_COL32(24, 24, 26, 255);
            uint32_t colRim   = IM_COL32(150, 155, 165, 255);

            for (int s = 0; s < 8; s++) {
                int next = (s + 1) % 8;
                int v0 = baseIdx + s * 2;
                int v1 = baseIdx + s * 2 + 1;
                int v2 = baseIdx + next * 2 + 1;
                int v3 = baseIdx + next * 2;

                outFaces.push_back({ {v0, v1, v2, v3}, 4, colTread, true });
                outFaces.push_back({ {hubIdx, v0, v3, 0}, 3, colRim, true });
            }
        }
    }
};

// =========================================================================
// 2. MAD MAX 3D CHARACTER MODEL (On-Foot Wasteland Warrior)
// =========================================================================
class CharacterModel {
public:
    static void BuildMesh(std::vector<Vec3>& outVertices, std::vector<PolyFace>& outFaces) {
        outVertices.clear();
        outFaces.clear();

        // Max is ~1.85m tall. Ground is y = 0.
        // Head (1.60 - 1.85m)
        outVertices.push_back(Vec3(-0.14f, 1.62f, -0.12f)); // 0: Head BL
        outVertices.push_back(Vec3( 0.14f, 1.62f, -0.12f)); // 1: Head BR
        outVertices.push_back(Vec3( 0.14f, 1.62f,  0.14f)); // 2: Head FR
        outVertices.push_back(Vec3(-0.14f, 1.62f,  0.14f)); // 3: Head FL
        outVertices.push_back(Vec3(-0.14f, 1.86f, -0.12f)); // 4: Top BL
        outVertices.push_back(Vec3( 0.14f, 1.86f, -0.12f)); // 5: Top BR
        outVertices.push_back(Vec3( 0.14f, 1.86f,  0.14f)); // 6: Top FR
        outVertices.push_back(Vec3(-0.14f, 1.86f,  0.14f)); // 7: Top FL

        // Torso & Leather Jacket (1.00 - 1.60m)
        outVertices.push_back(Vec3(-0.28f, 1.05f, -0.14f)); // 8: Pelvis BL
        outVertices.push_back(Vec3( 0.28f, 1.05f, -0.14f)); // 9: Pelvis BR
        outVertices.push_back(Vec3( 0.28f, 1.05f,  0.14f)); // 10: Pelvis FR
        outVertices.push_back(Vec3(-0.28f, 1.05f,  0.14f)); // 11: Pelvis FL
        outVertices.push_back(Vec3(-0.32f, 1.60f, -0.16f)); // 12: Shoulder BL
        outVertices.push_back(Vec3( 0.32f, 1.60f, -0.16f)); // 13: Shoulder BR
        outVertices.push_back(Vec3( 0.32f, 1.60f,  0.16f)); // 14: Shoulder FR
        outVertices.push_back(Vec3(-0.32f, 1.60f,  0.16f)); // 15: Shoulder FL

        // Left Shoulder Pauldron (16 - 19)
        outVertices.push_back(Vec3(-0.32f, 1.55f,  0.18f)); // 16
        outVertices.push_back(Vec3(-0.44f, 1.68f,  0.12f)); // 17
        outVertices.push_back(Vec3(-0.44f, 1.68f, -0.14f)); // 18
        outVertices.push_back(Vec3(-0.32f, 1.55f, -0.18f)); // 19

        // Left Leg & Knee Brace (20 - 23)
        outVertices.push_back(Vec3(-0.25f, 0.00f, -0.10f)); // 20: Boot L rear
        outVertices.push_back(Vec3(-0.10f, 0.00f, -0.10f)); // 21: Boot L inner
        outVertices.push_back(Vec3(-0.10f, 0.00f,  0.18f)); // 22: Boot L toe
        outVertices.push_back(Vec3(-0.25f, 0.00f,  0.18f)); // 23: Boot L heel

        // Right Leg (24 - 27)
        outVertices.push_back(Vec3( 0.10f, 0.00f, -0.10f)); // 24
        outVertices.push_back(Vec3( 0.25f, 0.00f, -0.10f)); // 25
        outVertices.push_back(Vec3( 0.25f, 0.00f,  0.18f)); // 26
        outVertices.push_back(Vec3( 0.10f, 0.00f,  0.18f)); // 27

        // Colors
        uint32_t colSkin    = IM_COL32(220, 180, 145, 255);
        uint32_t colGoggles = IM_COL32(25, 25, 28, 255);
        uint32_t colJacket  = IM_COL32(75, 52, 35, 255);
        uint32_t colVest    = IM_COL32(38, 40, 42, 255);
        uint32_t colSteel   = IM_COL32(170, 175, 185, 255);
        uint32_t colPants   = IM_COL32(42, 45, 48, 255);
        uint32_t colBoots   = IM_COL32(28, 26, 24, 255);

        // Head Faces
        outFaces.push_back({ {3, 2, 6, 7}, 4, colGoggles, true }); // Face / Goggles
        outFaces.push_back({ {2, 1, 5, 6}, 4, colSkin, true });    // Right Head
        outFaces.push_back({ {0, 3, 7, 4}, 4, colSkin, true });    // Left Head
        outFaces.push_back({ {1, 0, 4, 5}, 4, colSkin, true });    // Back Head
        outFaces.push_back({ {7, 6, 5, 4}, 4, colSkin, true });    // Top Head

        // Torso Faces
        outFaces.push_back({ {11, 10, 14, 15}, 4, colVest, true });   // Chest / Vest
        outFaces.push_back({ {9, 8, 12, 13}, 4, colJacket, true });   // Back Jacket
        outFaces.push_back({ {8, 11, 15, 12}, 4, colJacket, true });  // Left Torso
        outFaces.push_back({ {10, 9, 13, 14}, 4, colJacket, true });  // Right Torso

        // Shoulder Pauldron
        outFaces.push_back({ {16, 17, 18, 19}, 4, colSteel, true });   // Metal Pauldron Plate

        // Legs & Boots
        outFaces.push_back({ {20, 21, 9, 8}, 4, colPants, true });    // Left Leg Back
        outFaces.push_back({ {23, 22, 10, 11}, 4, colPants, true });  // Left Leg Front
        outFaces.push_back({ {24, 25, 9, 8}, 4, colPants, true });    // Right Leg Back
        outFaces.push_back({ {27, 26, 10, 11}, 4, colPants, true });  // Right Leg Front
        outFaces.push_back({ {20, 21, 22, 23}, 4, colBoots, true });  // Left Boot Sole
        outFaces.push_back({ {24, 25, 26, 27}, 4, colBoots, true });  // Right Boot Sole
    }
};

}
