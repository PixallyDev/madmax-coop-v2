# Mad Max Co-Op: Prototype Status & Phase 2 Audit Report

**Date:** 2026-10-25  
**Target:** `C:\Users\gm491\madmax-coop\build\Release\` & `C:\Users\gm491\madmax-coop\game\`  

---

## 1. Prototype Capabilities & Actual Verification Results

| Subsystem | Initial Status | Audited / Remediated Status | Verification Method |
| :--- | :--- | :--- | :--- |
| **CMake Build System** | Broken (circular target link, duplicate symbols in Lua) | **FIXED & VERIFIED** | Reconfigured CMakeLists.txt to link `dxgi.lib` cleanly, filter out `onelua.c` / `lua.c` / `luac.c`. Release build succeeds with 0 errors. |
| **dxgi.dll Loading Mechanism** | **CRITICAL DEFECT** (Missing exported functions) | **FIXED & VERIFIED** | `MadMax.exe` PE import table requires `CreateDXGIFactory1`. Added `dxgi_proxy.cpp` and `dxgi_proxy.def` forwarding `CreateDXGIFactory1` to `C:\Windows\System32\dxgi.dll`. Verified export with `dumpbin /EXPORTS`. |
| **DirectX 11 Present Hook** | Implemented (Dummy swapchain vtable query index 8 + MinHook) | **UNTESTED IN-GAME** | Code structure matches standard DirectX hooking pattern. Safe initialization check added. |
| **F5 ImGui Overlay** | Implemented (WndProc hook + ImGui DX11 backend) | **UNTESTED IN-GAME** | Renders telemetry placeholders and connection inputs when F5 toggled. |
| **ENet Local Networking** | Initial server & client skeleton | **VERIFIED OPERATIONAL** | Created `server/test_client.cpp`, launched background server, and successfully connected over `127.0.0.1:7777`. |
| **Embedded Lua Scripting** | Header included | **VERIFIED OPERATIONAL** | Created `server/test_lua.cpp`, compiled against vendored `lua.lib`, evaluated dynamic string script calling `on_player_connect(42, "RoadWarrior")`. |
| **Game Engine Reverse Engineering** | Speculative / Planned | **CONFIRMED UNHOOKED** | The prototype currently does **NOT** read or write any actual `MadMax.exe` memory addresses or pointers. Telemetry strings are placeholders. |

---

## 2. Identified Vulnerabilities, Risks & Gaps

1. **DllMain Concurrency Risk:**
   - Spawning a thread (`CreateThread`) in `DllMain` to sleep and hook DirectX is fragile if `MadMax.exe` loads `dxgi.dll` before creating its primary render window.
   - *Mitigation for Phase 3:* Hook `CreateDXGIFactory1` directly to intercept the factory and swap chain creation gracefully rather than relying on a dummy window and sleep timer.

2. **No Packet Serialization/Deserialization Protocol:**
   - The current server simply blindly relays raw packet buffers to all other connected peers.
   - *Risk:* No validation, no packet framing, no endianness check, no opcode routing.
   - *Mitigation:* Define explicit struct headers: `PacketHeader { uint16_t opcode; uint16_t size; uint32_t sequence; }`.

3. **Engine Memory State Still Unknown:**
   - No validated pointers exist in the code yet for Max's coordinates, vehicle velocity, or entity spawn tables.
'@