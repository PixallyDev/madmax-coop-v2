# Architecture Comparison: JC2-MP vs. Mad Max Co-Op

**Reference:** Just Cause 2 Multiplayer Mod (JC2-MP / jc2mp)  
**Target Mod:** Mad Max Co-Op Multiplayer (Avalanche Apex Engine)  

---

## 1. High-Level Architectural Comparison

| Architectural Pillar | Just Cause 2 Multiplayer (JC2-MP) | Mad Max Co-Op (This Project) | Feasibility & Evidence in Mad Max |
| :--- | :--- | :--- | :--- |
| **Engine Lineage** | Avalanche Engine 2.0 (32-bit x86, DirectX 10) | Avalanche Apex Engine (64-bit x64, DirectX 11) | **Confirmed.** Same architectural heritage, but Mad Max is 64-bit, requiring 64-bit calling conventions (`__fastcall`, RCX/RDX/R8/R9). |
| **Stripping Base Game AI** | Stripped out story, civil AI, and native pathfinding to support 1000+ players in Panau. | **DO NOT STRIP BASE AI.** Co-op target is 2-4 players. Wasteland enemies, camps, convoys, and Storm events must remain intact. | **Evidence:** Stripping AI is unnecessary and counter-productive for a co-op experience. We want players fighting wastelanders together. |
| **Client Hook / Injection** | Custom DLL injected into `JustCause2.exe`. | `dxgi.dll` proxy loaded directly alongside `MadMax.exe` in isolated folder. | **Verified.** Proxy DLL forwards `CreateDXGIFactory1` cleanly into system DXGI and hooks swap chain. |
| **Relay Server Architecture** | Dedicated headless server (`jc2mp-server`) tracking player states, vehicles, and broadcasting updates. | Dedicated headless server (`madmax_server.exe`) utilizing UDP ENet and packet routing. | **Verified.** Standalone server compiled and verified with automated test client. |
| **Scripting Layer** | Embedded Lua runtime allowing custom server game modes, teleport commands, and UI scripts. | Embedded Lua 5.4 in server and client. | **Verified.** Lua 5.4 compiled cleanly into static library; script evaluation verified. |
| **Vehicle Physics Sync** | Driver-authoritative state replication with dead-reckoning and interpolation on peers. | Driver-authoritative sync of Magnum Opus / Archangels (`CLandVehicle` + `CPhysicsVehicleChassis`). | **Candidate.** Requires syncing transform matrix, linear velocity, angular velocity, and steering/throttle. |
| **Character Sync & Spawning** | Custom puppet entity spawned in world; animation state synchronized over UDP. | Spawn dummy actor via `CSpawnInfoDataContainer` / `CSpawnedEntityHandler` and override transform with network packet. | **Candidate.** Requires identifying spawn functions in Ghidra. |

---

## 2. Lessons Learned & Design Principles from JC2-MP

1. **Avoid Reinventing Single-Player Loops:**
   - JC2-MP was designed for an MMO-scale sandbox (1,000+ players) where single-player AI was a performance liability.
   - For Mad Max Co-Op (2-4 players), the host can act as the authority for wasteland enemies and world storms, while peer clients replicate enemy damage and ragdoll states.

2. **Driver Authority on Vehicles:**
   - In JC2-MP, the player driving the vehicle must be the authority on that vehicle's physics. Attempting to simulate vehicle physics on the server or on non-driving clients leads to severe jitter and desync.
   - The non-driving player interpolates the vehicle's position using Hermite spline interpolation over the latest 3-5 received state packets.

3. **Separation of High-Frequency vs. Low-Frequency State:**
   - **High-Frequency (Unreliable UDP, 30-60 Hz):** Character coordinates, rotation quaternion, velocity vectors, steering angle, throttle.
   - **Low-Frequency (Reliable UDP):** Weapon changes, harpoon fire/detach, car repairs, camp progress, player chat commands.
'@