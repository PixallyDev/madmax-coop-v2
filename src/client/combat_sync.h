#pragma once
#include <windows.h>
#include <vector>
#include <iostream>
#include <imgui.h>
#include "../common/protocol.h"

namespace CombatSync {

struct CombatLogEntry {
    char description[128];
    DWORD timestamp;
    ImVec4 color;
};

static std::vector<CombatLogEntry> g_combatLog;
static uint32_t g_totalKillsShared = 0;
static uint32_t g_harpoonTethers = 0;

inline void LogAction(const char* desc, ImVec4 col = ImVec4(1.0f, 0.4f, 0.4f, 1.0f)) {
    CombatLogEntry entry;
    strncpy_s(entry.description, desc, sizeof(entry.description));
    entry.timestamp = GetTickCount();
    entry.color = col;
    g_combatLog.push_back(entry);
    if (g_combatLog.size() > 15) {
        g_combatLog.erase(g_combatLog.begin());
    }
}

inline void HandleRemoteCombat(const MsgCombatEvent* event, uint32_t senderClientId) {
    char buf[128];
    switch (event->actionType) {
        case COMBAT_ACTION_MELEE_HIT:
            sprintf_s(buf, "[P%u] Heavy Melee Punch dealt %.0f DMG to War Boy", senderClientId, event->damageAmount);
            LogAction(buf, ImVec4(1.0f, 0.6f, 0.2f, 1.0f));
            break;
        case COMBAT_ACTION_SHOTGUN_FIRE:
            sprintf_s(buf, "[P%u] Shotgun Blast! (Impact at %.0f, %.0f, %.0f)", senderClientId, event->impactPosX, event->impactPosY, event->impactPosZ);
            LogAction(buf, ImVec4(1.0f, 0.2f, 0.2f, 1.0f));
            break;
        case COMBAT_ACTION_PARRY:
            sprintf_s(buf, "[P%u] Perfectly Parried Wasteland Raider!", senderClientId);
            LogAction(buf, ImVec4(0.3f, 0.8f, 1.0f, 1.0f));
            break;
        case COMBAT_ACTION_HARPOON_TETHER:
            sprintf_s(buf, "[P%u] Magnum Opus Harpoon Tethered to Target!", senderClientId);
            LogAction(buf, ImVec4(0.2f, 1.0f, 0.4f, 1.0f));
            g_harpoonTethers++;
            break;
    }
}

inline void RenderCombatFeed() {
    ImDrawList* drawList = ImGui::GetBackgroundDrawList();
    if (!drawList || g_combatLog.empty()) return;

    DWORD now = GetTickCount();
    float startY = 120.0f;
    float startX = 20.0f;

    for (size_t i = 0; i < g_combatLog.size(); ++i) {
        const auto& entry = g_combatLog[i];
        DWORD age = now - entry.timestamp;
        if (age > 8000) continue; // Fade after 8s

        float alpha = 1.0f;
        if (age > 6000) {
            alpha = 1.0f - ((float)(age - 6000) / 2000.0f);
        }

        ImU32 col = ImGui::ColorConvertFloat4ToU32(ImVec4(entry.color.x, entry.color.y, entry.color.z, alpha));
        drawList->AddText(ImVec2(startX, startY + (i * 18.0f)), col, entry.description);
    }
}

}
