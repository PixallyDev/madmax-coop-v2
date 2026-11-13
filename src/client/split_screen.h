#pragma once
#include <windows.h>
#include <xinput.h>
#include <MinHook.h>
#include <iostream>
#include <fstream>
#include <string>

namespace SplitScreen {

    enum class InputDeviceMode {
        DualGamepad,    // Instance 1 = Pad 0, Instance 2 = Pad 1
        KeyboardMouse,  // Instance 1 = KBM only (Blocks Gamepad)
        GamepadOnly     // Instance 2 = Gamepad only (Blocks KBM)
    };

    static InputDeviceMode g_InputMode = InputDeviceMode::DualGamepad;
    static int g_PlayerIndex = 0;       // 0 = Player 1 (Left), 1 = Player 2 (Right)
    static std::string g_ScreenHalf = "Left";
    static bool g_bHooksInstalled = false;

    // Original XInput functions
    typedef DWORD(WINAPI* PFN_XInputGetState)(DWORD dwUserIndex, XINPUT_STATE* pState);
    typedef DWORD(WINAPI* PFN_XInputSetState)(DWORD dwUserIndex, XINPUT_VIBRATION* pVibration);

    static PFN_XInputGetState oXInputGetState = nullptr;
    static PFN_XInputSetState oXInputSetState = nullptr;

    // Hooked XInputGetState: Handles KBM isolation and controller routing
    inline DWORD WINAPI Hooked_XInputGetState(DWORD dwUserIndex, XINPUT_STATE* pState) {
        if (!oXInputGetState) return ERROR_DEVICE_NOT_CONNECTED;

        // If this instance is designated as KBM ONLY (Player 1), hide the controller completely!
        if (g_InputMode == InputDeviceMode::KeyboardMouse) {
            return ERROR_DEVICE_NOT_CONNECTED;
        }

        // If this instance is GamepadOnly (Player 2 using the single controller):
        // Query physical controller 0 (the only controller connected)
        DWORD targetPad = 0;
        if (g_InputMode == InputDeviceMode::DualGamepad) {
            targetPad = (dwUserIndex == 0) ? (DWORD)g_PlayerIndex : dwUserIndex;
        }

        return oXInputGetState(targetPad, pState);
    }

    // Hooked XInputSetState: Remaps Vibration
    inline DWORD WINAPI Hooked_XInputSetState(DWORD dwUserIndex, XINPUT_VIBRATION* pVibration) {
        if (!oXInputSetState) return ERROR_DEVICE_NOT_CONNECTED;

        if (g_InputMode == InputDeviceMode::KeyboardMouse) {
            return ERROR_DEVICE_NOT_CONNECTED;
        }

        DWORD targetPad = 0;
        if (g_InputMode == InputDeviceMode::DualGamepad) {
            targetPad = (dwUserIndex == 0) ? (DWORD)g_PlayerIndex : dwUserIndex;
        }

        return oXInputSetState(targetPad, pVibration);
    }

    // Load configuration from split_config.ini
    inline void LoadConfig() {
        char buffer[64];
        DWORD res = GetPrivateProfileStringA("SplitScreen", "PlayerIndex", "0", buffer, sizeof(buffer), ".\\split_config.ini");
        if (res > 0) {
            g_PlayerIndex = atoi(buffer);
        }

        res = GetPrivateProfileStringA("SplitScreen", "ScreenHalf", "Left", buffer, sizeof(buffer), ".\\split_config.ini");
        if (res > 0) {
            g_ScreenHalf = buffer;
        }

        res = GetPrivateProfileStringA("SplitScreen", "InputMode", "Auto", buffer, sizeof(buffer), ".\\split_config.ini");
        if (res > 0) {
            if (_stricmp(buffer, "KBM") == 0) {
                g_InputMode = InputDeviceMode::KeyboardMouse;
            } else if (_stricmp(buffer, "Gamepad") == 0) {
                g_InputMode = InputDeviceMode::GamepadOnly;
            } else {
                g_InputMode = InputDeviceMode::DualGamepad;
            }
        }
    }

    // Automatic borderless window snapping to split-screen half
    inline void SnapWindowToHalf(HWND hWnd) {
        if (!hWnd) return;

        int screenW = GetSystemMetrics(SM_CXSCREEN);
        int screenH = GetSystemMetrics(SM_CYSCREEN);

        int targetX = 0;
        int targetY = 0;
        int targetW = screenW / 2;
        int targetH = screenH;

        if (g_ScreenHalf == "Right") {
            targetX = screenW / 2;
        } else if (g_ScreenHalf == "Top") {
            targetX = 0;
            targetY = 0;
            targetW = screenW;
            targetH = screenH / 2;
        } else if (g_ScreenHalf == "Bottom") {
            targetX = 0;
            targetY = screenH / 2;
            targetW = screenW;
            targetH = screenH / 2;
        }

        // Strip borders and title bar
        LONG_PTR style = GetWindowLongPtr(hWnd, GWL_STYLE);
        style &= ~(WS_CAPTION | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_SYSMENU);
        SetWindowLongPtr(hWnd, GWL_STYLE, style);

        // Position window borderless
        SetWindowPos(hWnd, HWND_TOP, targetX, targetY, targetW, targetH, SWP_FRAMECHANGED | SWP_SHOWWINDOW);
    }

    // Filter Window messages to bypass focus loss and isolate KBM
    inline bool HandleWindowMessages(HWND hWnd, UINT uMsg, WPARAM& wParam, LPARAM& lParam) {
        // 1. If this instance is Gamepad Only (Player 2), block all Keyboard & Mouse events
        // so Player 1 typing or clicking never affects Player 2!
        if (g_InputMode == InputDeviceMode::GamepadOnly) {
            if ((uMsg >= WM_KEYFIRST && uMsg <= WM_KEYLAST) ||
                (uMsg >= WM_MOUSEFIRST && uMsg <= WM_MOUSELAST)) {
                return true; // Discard KBM input
            }
        }

        // 2. Prevent engine from pausing or dropping ticks when window loses focus
        switch (uMsg) {
            case WM_KILLFOCUS:
                return true; // Swallowed so engine never realizes focus was lost
            case WM_ACTIVATE:
                wParam = MAKEWPARAM(WA_ACTIVE, HIWORD(wParam));
                return false;
            case WM_ACTIVATEAPP:
                wParam = TRUE;
                return false;
            default:
                return false;
        }
    }

    // Install XInput Hooks
    inline void InstallInputHooks() {
        if (g_bHooksInstalled) return;

        LoadConfig();

        const char* xinputDlls[] = { "xinput1_4.dll", "xinput1_3.dll", "xinput9_1_0.dll" };
        HMODULE hXInput = nullptr;

        for (const char* dllName : xinputDlls) {
            hXInput = GetModuleHandleA(dllName);
            if (!hXInput) {
                hXInput = LoadLibraryA(dllName);
            }
            if (hXInput) break;
        }

        if (hXInput) {
            void* pGetState = (void*)GetProcAddress(hXInput, "XInputGetState");
            void* pSetState = (void*)GetProcAddress(hXInput, "XInputSetState");

            if (pGetState) {
                MH_CreateHook(pGetState, &Hooked_XInputGetState, (LPVOID*)&oXInputGetState);
                MH_EnableHook(pGetState);
            }
            if (pSetState) {
                MH_CreateHook(pSetState, &Hooked_XInputSetState, (LPVOID*)&oXInputSetState);
                MH_EnableHook(pSetState);
            }
        }

        g_bHooksInstalled = true;
    }
}
