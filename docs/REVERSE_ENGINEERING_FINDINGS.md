# Reverse-Engineering Findings: Mad Max (2015) Engine Analysis

**Binary Analyzed:** `C:\Users\gm491\madmax-coop\game\MadMax.exe`  
**File Size:** 76,618,184 bytes (73.07 MB)  
**Target Architecture:** PE32+ (64-bit x86_64, Little-Endian)  
**Image Base:** `0x0000000140000000` to `0x0000000145D57FFF`  
**Image Size:** `0x05D58000` (93.3 MB mapped in memory)  
**Entry Point RVA:** `0x01B36728` (`0x0000000141B36728`)  

---

## 1. Engine & Build Environment Evidence

- **Engine Family:** Avalanche Apex Engine (Internal project codename confirmed: `AvalancheFuryRoad` / `delivery_jpn`).
- **Internal PDB Symbol File:**
  ```
  d:\jenkins\workspace\mm-delivery_jpn-cpkg-win64\branch\delivery_jpn\source\build\Win64\Final\AVAMain_Steam_F.pdb
  ```
- **Source Code Structure Revealed by Jenkins Build Assertions:**
  - `source\project\game\gamestates\gamestaterun.cpp` -> Main game state loop.
  - `source\project\game\gamestates\gamestateload.cpp` -> World/level loading state machine.
  - `source\project\game\ai\aifragmentinstancemanager.cpp` -> AI instance manager.
  - `source\ctg\components\gamesystem\source\gamesystem\resourceloader.cpp` -> Dropzone and asset archive loader (`ResourceLoader::LoadDropzoneFile`, `GetAndCacheDropzoneFiles`).
  - `source\ctg\components\animation\source\ragdoll\ragdollinstance.cpp` -> Avalanche Ragdoll Proxies & Havok integration.
  - `source\ctg\components\physics\source\physics\customhavok\customvisualdebugger...` -> Havok Physics Visual Debugger hooks.

---

## 2. RTTI & Class Descriptors (Confirmed Evidence)

Over **1,776 unique RTTI type descriptors (`.?AV<Class>@@`)** are present inside `MadMax.exe`. This confirms that despite being a release build, the binary retains polymorphic class metadata, making reverse-engineering via Ghidra/x64dbg substantially faster than a fully stripped binary.

### Key Classes Identified:
1. **Player & Character State:**
   - `CPlayer` -> Core local player instance.
   - `CPlayerStateObject` -> Player health, movement state, and combat variables.
   - `CVisiblePlayerUpgradesManager` -> Max's jackets, knuckle dusters, headgear, shotgun holsters.
   - `AvaHkpCharacterStateManager`, `AvaHkpCharacterStateOnGround`, `AvaHkpCharacterStateInAir`, `AvaHkpCharacterStateJumping` -> Havok character controller states.
2. **Vehicle Entities & Physics:**
   - `CLandVehicle` -> Base class for wheeled wasteland vehicles.
   - `CPhysicsVehicleChassis` -> Havok rigid body vehicle chassis.
   - `CAiVehicleEntity` -> AI-driven wasteland enemy vehicles.
   - `CHarpoonVehicleTarget` -> Harpoon tether attachment points.
   - `CIsPlayerVehicleCondition`, `CIsSignatureVehicleCondition` -> Magnum Opus distinction checks.
3. **World, Scene & Spawning:**
   - `CGameWorld`, `IGameWorld` -> World container.
   - `CSpawnInfoDataContainer`, `CSpawnObjectCollection`, `CSpawnPoint` -> Entity and vehicle spawning system.
   - `CSpawnedEntityHandler` -> Tracks dynamically spawned entities in the world.

---

## 3. Findings Matrix & Research Questions

| Question | Engine Evidence | Finding Classification | Validation Method |
| :--- | :--- | :--- | :--- |
| **Q1: Apex Engine Implementation** | Confirmed Avalanche Engine branch `delivery_jpn` with Havok 2012 physics integration. | **CONFIRMED FACT** | String and RTTI extraction from binary. |
| **Q2: Main Game Loop** | `CGameStateRun` manages runtime frame ticks; imports `dxgi.dll` (`CreateDXGIFactory1`) and `d3d11.dll`. | **CONFIRMED FACT** | Source assert string + Import table analysis. |
| **Q3: Player & Vehicle Structures** | `CPlayer` and `CLandVehicle` / `CPhysicsVehicleChassis` represent local player and Magnum Opus. | **CONFIRMED (Classes)** / **CANDIDATE (Offsets)** | RTTI symbols found. Exact memory offsets require live Cheat Engine / x64dbg pointer walk. |
| **Q4: Spawning Remote Players** | `CSpawnInfoDataContainer` and `CSpawnedEntityHandler` handle dynamic entity creation. | **CANDIDATE** | Disassemble `CSpawnedEntityHandler` in Ghidra to locate calling convention for manual entity instantiation. |
| **Q5: Community Signatures** | Community Cheat Engine tables (e.g. Paul44) document float[3] coordinates for player position and waypoint teleportation. | **CORROBORATED** | FearLess CE database patterns confirm XYZ float structure. |

---

## 4. Deep Reverse Engineering Breakthroughs (REA Investigation)

### A. Denuvo Packaging on Disk vs. Decrypted Memory at Runtime
- Disassembly of raw `.idata` code sections on disk (`CCloneFactory::CloneEntity`, `CPlayer::VFunc`, `CSpawnedEntityHandler`) yields obfuscated/encrypted bytecode stubs (`loopne`, `mov esp, 0x8781b5ff`).
- `MadMax.exe` decrypts all engine code and data at runtime into the memory range `0x140000000` to `0x145D58000`.
- **Methodology Decision:** All pointer dereferencing, vtable walking, and struct inspection must be executed **dynamically in live memory** by the injected client DLL (`dxgi.dll`).

### B. Complete VMT Address Catalog (270 Engine Classes Resolved)
We systematically scanned the MSVC 64-bit RTTI Complete Object Locators (`COL`) in memory and resolved 270 Virtual Method Tables:
- `CPlayer`: VMT `0x1412C2CB0` (RVA `0x012C2CB0`)
- `CPlayerStateObject`: VMT `0x1412BB058` (RVA `0x012BB058`)
- `CVisiblePlayerUpgradesManager`: VMT `0x1412A57A8` (RVA `0x012A57A8`)
- `CLandVehicle`: VMT `0x1412D9A68` (RVA `0x012D9A68`)
- `CPhysicsVehicleChassis`: VMT `0x141424E28` (RVA `0x01424E28`)
- `CCallSignatureVehicle`: VMT `0x1412DB020` (RVA `0x012DB020`)
- `CSpawnedEntityHandler`: VMT `0x1412CE770` (RVA `0x012CE770`)
- `CSpawnObjectCollection`: VMT `0x14129D538` (RVA `0x0129D538`)
- `CGameObjectFactory`: VMT `0x1412928B0` (RVA `0x012928B0`)

### C. Live Entity Inspector Implementation
The client DLL now incorporates [`src/client/entity_inspector.h`](file:///C:/Users/gm491/madmax-coop/src/client/entity_inspector.h):
- When `AutoScanner` locks onto Max's XYZ coordinates in heap memory, `EntityInspector` walks backwards through memory up to 1,024 bytes (`0x400`).
- It matches each pointer against the 270 resolved VMTs via [`src/client/vmt_catalog.h`](file:///C:/Users/gm491/madmax-coop/src/client/vmt_catalog.h).
- Once matched, it isolates the true C++ instance base pointer (`this`), calculates the exact coordinate member offset, and displays the internal struct layout in the F5 ImGui menu.
