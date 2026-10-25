# Toolchain and REA Integration Audit

**Date:** 2026-10-25  
**Project Root:** `C:\Users\gm491\madmax-coop`  
**Host Architecture:** Windows 11 x64 (Build 26200+)  

---

## 1. REA (Reverse Engineer Anything) Status

- **Repository Reference:** `https://github.com/morluto/rea`
- **Skill Status:** The markdown skill instructions (`reverse-engineer-anything`, `rea-tool-design`, etc.) are installed in `~/.gemini/skills/`.
- **CLI / Package Version:** `rea-agents@6.8.0` evaluated via `npx -y rea-agents@latest`.
- **Diagnostic Run Result:**
  ```json
  "healthy": false, "environment_healthy": false
  ```
  - **Node.js Compatibility:** Current Node.js is `v24.18.0`. REA warns: `required: { node: ^22.23.4 || ^24.22.1 || >=26.12.1 }`.
  - **MCP Registration:** Currently missing in active Antigravity MCP config (`C:\Users\gm491\.gemini\config\mcp_config.json`).
  - **Ghidra Engine Provider:** Detected at `C:\Users\gm491\madmax-coop\tools\ghidra\ghidra_12.2_PUBLIC`. REA currently targets verified Ghidra releases `12.1.x`, triggering a version drift notice for `12.2`.
- **Minimum Additional Setup for Full REA MCP Integration:**
  1. Upgrade Node.js from `24.18.0` to `>=24.22.1` (or modern LTS).
  2. Run `npx -y rea-agents@latest setup --client antigravity --yes` to populate `mcp_config.json`.
  3. Restart the Antigravity agent process to load the newly registered MCP server.

---

## 2. Reverse Engineering Toolchain Inventory

| Tool | Version / Path | Verification Method | Status |
| :--- | :--- | :--- | :--- |
| **Ghidra** | 12.2 PUBLIC (`tools/ghidra/ghidra_12.2_PUBLIC`) | Launched `analyzeHeadless.bat`, verified class search and initialization | **Verified & Operational** |
| **Java JDK** | Eclipse Adoptium OpenJDK 25.0.4.101 & 21.0.12.101 | Verified via `where.exe java` and Ghidra launcher probe | **Verified & Operational** |
| **x64dbg** | Snapshot 2026-05-27 (`tools/x64dbg/release/x64/x64dbg.exe`) | Executable unpacked and verified on disk | **Verified & Operational** |
| **Cheat Engine** | Version 7.5 (`C:\Program Files\Cheat Engine\Cheat Engine.exe`) | Executable presence and CLI probe verified | **Verified & Operational** |
| **MSVC x64** | VS 2022 BuildTools v19.44.35229 (`cl.exe`, `vcvars64.bat`) | Full C++20 compilation of client DLL and server | **Verified & Operational** |
| **CMake & Ninja** | CMake 3.20+ (`CommonExtensions/Microsoft/CMake`) | CMake generation and build completed with 0 errors | **Verified & Operational** |
| **Windows SDK** | 10.0.26100.0 | Linked against `d3d11.lib`, `dxgi.lib`, `ws2_32.lib` | **Verified & Operational** |

---

## 3. Third-Party Dependencies (Vendored)

| Library | Source & Location | Build Mode | Verification Test |
| :--- | :--- | :--- | :--- |
| **MinHook** | Git clone (`src/vendor/minhook`) | Static Library x64 (`minhook.x64.lib`) | Compiled cleanly in Release |
| **Dear ImGui** | Git clone (`src/vendor/imgui`, DX11 + Win32) | Static Library x64 (`imgui.lib`) | Compiled cleanly in Release |
| **ENet** | Git clone (`src/vendor/enet`) | Static Library x64 (`enet.lib`) | Tested with `test_client.exe` connecting to `madmax_server.exe` |
| **Lua** | Git clone (`src/vendor/lua`, 5.4 core) | Static Library x64 (`lua.lib`) | Tested with `test_lua.exe` executing scripted functions |
