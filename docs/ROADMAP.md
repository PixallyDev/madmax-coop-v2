# Mad Max Co-Op Multiplayer: Prioritized Tier 3 Roadmap

**Project Root:** `C:\Users\gm491\madmax-coop`  
**Architecture:** Dedicated Injected Client DLL (`dxgi.dll`) + Dedicated Server (`madmax_server.exe`)  
**Design Philosophy:** JC2-MP Inspired, Evidence-Driven, Co-Op Centric (Preserves Base AI & Missions)  

---

## Roadmap Milestones Overview

```
[Milestone A: Toolchain & Baseline Build] (COMPLETE)
        │
        ▼
[Milestone B: Verified Game Injection & Diagnostics] (NEXT)
        │
        ▼
[Milestone C: Reliable Packet Protocol & Two-Client Simulation]
        │
        ▼
[Milestone D: Live Local Player Telemetry Hook (XYZ Readout)]
        │
        ▼
[Milestone E: Remote Character & Vehicle Entity Spawning]
        │
        ▼
[Milestone F: Movement Replication & Interpolation]
        │
        ▼
[Milestone G: Vehicle Physics & Driver Authority Sync]
        │
        ▼
[Milestone H: Combat, Harpoon Tether, and Enemy Damage Sync]
        │
        ▼
[Milestone I: Co-op Quests, Camp Clears, & Wasteland World State]
```

---

## Detailed Milestone Specifications

### Milestone A: Reproducible Toolchain and Verified Baseline Build
- **Status:** **COMPLETE & VERIFIED**
- **Deliverables:**
  - Complete MSVC / CMake C++20 build pipeline.
  - Vendored libraries (MinHook, Dear ImGui, ENet, Lua 5.4).
  - Standalone dedicated server (`madmax_server.exe`).
  - Automated tests (`test_client.exe`, `test_lua.exe`).
- **Definition of Done:** CMake generates and builds all targets in Release mode with 0 errors.

---

### Milestone B: Confirmed Game-Process Loading, Diagnostics, and Safe Shutdown
- **Status:** **CURRENT FOCUS / READY FOR EXECUTION**
- **Objective:** Verify that `MadMax.exe` loads `C:\Users\gm491\madmax-coop\game\dxgi.dll`, forwards `CreateDXGIFactory1` cleanly to the system, initializes the DX11 swapchain hook, and renders the F5 ImGui overlay without crashing or leaking resources on shutdown.
- **Prerequisites:**
  - Milestone A complete.
  - `dxgi_proxy.cpp` and `dxgi_proxy.def` exporting `CreateDXGIFactory1`.
- **Implementation Tasks:**
  1. Add detailed file logging (`madmax_coop.log`) inside `dxgi.dll` to record initialization milestones (proxy load, D3D11 hook, ImGui context creation, shutdown).
  2. Test launch `MadMax.exe` in windowed mode.
  3. Verify F5 toggle reveals the *Mad Max Co-Op Multiplayer* menu.
  4. Verify clean exit when quitting to desktop.
- **Acceptance Criteria:**
  - `madmax_coop.log` records successful initialization of DX11 hook and ImGui.
  - Game renders normal 3D scene with the ImGui overlay on top.
  - Exiting the game terminates the process cleanly without an unhandled exception or hung thread.
- **Risks & Rollback:**
  - *Risk:* Crash during dummy window creation or fullscreen swapchain transitions.
  - *Rollback:* Remove `dxgi.dll` from `game/` folder; inspect crash minidump.

---

### Milestone C: Reliable Local Networking & Packet Protocol
- **Status:** Planned
- **Objective:** Replace raw packet relays with a structured, versioned binary packet protocol supporting client registration, peer discovery, and heartbeat validation.
- **Prerequisites:** Milestone B verified.
- **Implementation Tasks:**
  1. Define `NetworkProtocol.h` with packet headers:
     - `MSG_HEARTBEAT`, `MSG_CLIENT_JOIN`, `MSG_CLIENT_LEAVE`
     - `MSG_PLAYER_TRANSFORM`, `MSG_VEHICLE_TRANSFORM`
     - `MSG_COMBAT_EVENT`
  2. Implement packet validation and sequence numbering on `madmax_server.exe`.
  3. Create a multi-client headless harness running 2 synthetic clients exchanging packets through the server.
- **Acceptance Criteria:** Multi-client test sends 10,000 packets with 0 dropped reliable packets and correct peer routing.
- **Definition of Done:** Two independent client processes communicate through the server with validated message IDs and round-trip ping measurements.

---

### Milestone D: Validated Local Player & Vehicle Coordinate Reading
- **Status:** Planned
- **Objective:** Hook the engine's local player coordinate update routine and vehicle transform matrix, feeding real-time XYZ positions into the ImGui overlay and network sync buffer.
- **Prerequisites:** Milestones B & C.
- **Implementation Tasks:**
  1. Use Cheat Engine / x64dbg to locate the memory address holding Max's world coordinates (float[3]).
  2. Find the instruction that accesses/updates these coordinates using a hardware read breakpoint (`Find out what accesses this address`).
  3. Implement an AOB (Array of Bytes) signature scanner in `dxgi.dll` to locate this instruction dynamically across game reboots.
  4. Read the base pointer from the register (e.g. `RCX` or `RBX`) and display real-time coordinates on the ImGui overlay.
- **Acceptance Criteria:** Walking, running, jumping, and driving in-game updates the XYZ floats on the ImGui overlay with 100% accuracy.
- **Definition of Done:** Memory reads verified over a 15-minute play session across multiple wasteland regions with zero crashes.

---

### Milestone E: Remote Character & Vehicle Entity Spawning
- **Status:** Planned
- **Objective:** Spawn a duplicate character model (puppet) and vehicle in the world to represent connected peers.
- **Prerequisites:** Milestone D.
- **Implementation Tasks:**
  1. Disassemble `CSpawnedEntityHandler` and `CSpawnInfoDataContainer` in Ghidra to find the engine's internal `SpawnCharacter` / `SpawnVehicle` functions.
  2. Hook the spawn function via MinHook to allow programmatic invocation.
  3. Strip or disable the AI brain/controller on the spawned puppet so it does not attack or flee.
- **Acceptance Criteria:** A second Max or wasteland warrior model appears in the host's world upon client connection.
- **Definition of Done:** Remote entity exists in the world scene graph without crashing the rendering or physics engine.

---

### Milestone F: Movement Replication & Interpolation
- **Status:** Planned
- **Objective:** Continuously replicate the remote player's position, rotation quaternion, and animation states onto their spawned puppet entity.
- **Prerequisites:** Milestone E.
- **Implementation Tasks:**
  1. Transmit `MSG_PLAYER_TRANSFORM` at 30-60 Hz over unreliable UDP.
  2. Implement Hermite spline interpolation and dead-reckoning on the receiving client to eliminate visual stutter.
  3. Synchronize basic animation states (idle, walk, run, sprint, jump, aim).
- **Acceptance Criteria:** Player 1 sees Player 2 running and turning smoothly in the wasteland.
- **Definition of Done:** Less than 50ms visual discrepancy between clients on a local LAN connection.

---

### Milestone G: Vehicle Synchronization & Driver Authority
- **Status:** Planned
- **Objective:** Synchronize the Magnum Opus and other wasteland vehicles across clients.
- **Prerequisites:** Milestone F.
- **Implementation Tasks:**
  1. Implement driver-authoritative replication (the driver's client simulates Havok vehicle physics; non-drivers receive and interpolate state).
  2. Replicate steering angle, throttle, brakes, and nitrous boost triggers.
  3. Replicate passenger states (Chumbucket in the back, peer player riding shotgun).
- **Acceptance Criteria:** Both players can drive alongside each other in the wasteland, race, and collide without physics explosion.
- **Definition of Done:** Smooth convoy driving across rough terrain with synchronized vehicle damage and tire effects.

---

### Milestone H: Combat, Harpoon Tether, and Enemy Damage Sync
- **Status:** Planned
- **Objective:** Synchronize vehicle harpoons, shotgun blasts, melee combat, and wasteland enemy health.
- **Prerequisites:** Milestone G.
- **Implementation Tasks:**
  1. Designate host as the authoritative simulator for NPC wastelanders and war boys.
  2. Replicate player melee hits, parries, and shotgun blasts to the host.
  3. Replicate the vehicle harpoon tether (attaching to gates, scarecrows, enemy wheels).
- **Acceptance Criteria:** Both players can team up to take down enemy vehicles and clearing enemy camps.
- **Definition of Done:** Shared enemy kills, synchronized ragdolls, and harpoon physics.

---

### Milestone I: Co-Op Quests, Camp Clears, & Wasteland World State
- **Status:** Long-term Research
- **Objective:** Synchronize wasteland map markers, stronghold projects, threat levels, and mission triggers.
- **Prerequisites:** Milestone H.
- **Acceptance Criteria:** Clearing a camp or destroying a scarecrow by Player 2 lowers the threat level for both players.
