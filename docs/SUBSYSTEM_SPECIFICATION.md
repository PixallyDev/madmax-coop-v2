# Mad Max Co-Op Multiplayer: Subsystem Reverse Engineering Specification

This document maps all 9 gameplay, engine, and network subsystems of *Mad Max* (Avalanche Apex Engine, Havok 2012) required for the full co-op experience.

See complete architectural diagram and class mapping in the master architecture artifact:
[MADMAX_COOP_ENGINE_ARCHITECTURE.md](file:///C:/Users/gm491/.gemini/antigravity-cli/brain/6dac760c-82ca-4706-b5ce-02d847319238/MADMAX_COOP_ENGINE_ARCHITECTURE.md).

## Subsystems Overview:
1. **Player Physics & Telemetry:** `CPlayer` (`0x1412C2CB0`), `AvaHkpCharacterStateManager` (`0x141324960`).
2. **3D Character & Puppet Rig:** `CVisiblePlayerUpgradesManager` (`0x1412A57A8`), `CBoneTransformEventSender` (`0x14123C338`).
3. **Driving & Vehicle Physics:** `CLandVehicle` (`0x1412D9A68`), `CPhysicsVehicleChassis` (`0x141424E28`), `hkpVehicleManager` (`0x141427028`).
4. **Fighting & Combat:** `CCharacterVehicleInTakedownCondition` (`0x141243EA0`), `CAiCharacterEntity` (`0x14123D7E0`), Shotgun, Harpoon.
5. **Loot, Inventory & Economy:** `CSpawnPickup` (`0x14127F0C8`), `CPlayerSettings` (`0x1412BBE78`), Scrap, Fuel cans, Water.
6. **Skills, Upgrades & Gear Points:** `CGuiPlayerUpgrade` (`0x141262E00`), `CVisiblePlayerUpgradesManager` (`0x1412A57A8`), Griffa tokens.
7. **Quests & Wasteland World State:** `CGameWorld` (`0x1413F9D40`), Camps, Scarecrows, Threat levels, Convoys.
8. **UI, Map & Markers:** `GuiFragmentBundle_F.dll`, Cooperative waypoints, Compass radar, Team health.
9. **Networking & Server Relay:** `madmax_server.exe` (UDP ENet @ Port 7777), binary protocol MMCP (`0x4D4D4350`).
