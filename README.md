# SCC — Ready or Not

[Türkçe](README.tr.md) · **English**

SCC is a configurable in-game control panel and ESP for the Steam version of **Ready or Not**. It combines an x64 internal DLL, a Dear ImGui DirectX 11/12 overlay, dynamic mission-objective discovery, persistent profiles and safe cleanup.

> Version-locked to Steam build `25784851`. Intended for educational, offline, single-player or private host-authorized sessions only. SCC is not affiliated with VOID Interactive and contains no anti-cheat bypass.

This project started from [Herkkomehtala/ReadyOrNot-Internal](https://github.com/Herkkomehtala/ReadyOrNot-Internal) and was substantially updated with a new overlay, gameplay controls, objective model, tests and launcher.

## Menu previews

![SCC overview](docs/images/overview.png)

| Player and team | Weapons |
|---|---|
| ![Player page](docs/images/player.png) | ![Weapons page](docs/images/weapons.png) |

| Movement | NPC and world |
|---|---|
| ![Movement page](docs/images/movement.png) | ![NPC page](docs/images/npc.png) |

| Visual filters | Mission objectives |
|---|---|
| ![Visuals page](docs/images/visuals.png) | ![Missions page](docs/images/missions.png) |

| ESP colors | System |
|---|---|
| ![Colors page](docs/images/colors.png) | ![System page](docs/images/system.png) |

## Features

### Player and team

- Configurable infinite player health
- Injury prevention and limb-injury cleanup
- Infinite health for living SWAT teammates

### Weapons and movement

- Recoil toggle with a configurable remaining-recoil ratio
- Keep the active magazine filled; safely restore weapon state on change/unload
- Maximum base movement speed
- Independent crouch, slow-walk and walk multipliers

### NPC and world

- Force suspects to surrender once or continuously
- Force suspects to drop weapons
- Configurable range: nearby NPCs or the complete map
- Unlock scanned doors while enabled and restore session-owned changes on disable
- Teleport the player to a selected NPC or bring that NPC near the player

### ESP and objectives

- Boxes, streamlined skeletons, type/name labels and distance
- Separate filters for suspects, civilians, allies, weapons, evidence, traps and reportable objects
- Pistols and long guns; inventory/held/allied weapons are filtered out
- Dynamic arrest/rescue targets, bombs, computers/terminals and location/report objectives
- Completed, failed, hidden or unrelated objectives stop receiving special markers
- Adjustable line thickness, text scale, opacity, label background and overlap reduction
- Per-category color editor and style preview

### Interface and lifecycle

- Nine-page SCC menu opened with `Insert`
- Mouse input capture while the menu is open; game input restored when hidden
- Save, reload and restore-default profile actions
- Safe unload from the System page without closing the game
- Game-build hash and live SDK-address validation before loading

## Quick installation

1. Download `SCC-ReadyOrNot-<version>.zip` from the repository's **Releases** page.
2. Extract the complete archive to a permanent folder.
3. Start Ready or Not through Steam and wait for the main menu.
4. Run `SCC-ReadyOrNot-Aktif-Et.cmd`.
5. Wait until the launcher confirms both the HUD hook and ImGui renderer.
6. Press `Insert` to show or hide SCC.
7. Save the profile from **System → Save**.
8. Use **System → Completely unload** before updating or deleting SCC files.

The launcher refuses unknown game builds and invalid live addresses. Do not remove those checks.

See [the detailed English installation and troubleshooting guide](docs/INSTALLATION.en.md).

## Build from source

Requirements:

- Windows 10/11 x64
- Visual Studio 2022 C++ Build Tools, MSVC v143 and Windows SDK
- PowerShell 5.1 or newer

```powershell
.\Build-RoN.ps1
```

Output:

```text
x64\Release\SCC-ReadyOrNot.dll
```

Read-only compatibility check:

```powershell
.\Activate-RoN.ps1 -CheckOnly
```

## Tests

```powershell
.\test\run-esp-tests.cmd
.\test\run-weapon-tests.cmd
.\test\run-gameplay-tests.cmd
.\test\run-menu-tests.ps1
```

The menu harness renders every page to PNG through a real hidden DirectX device and validates Insert toggling, resizing, save/load/default commands, menu-hidden ESP and safe unload behavior.

## Configuration and logs

- Profile: `x64\Release\SCC-ReadyOrNot.ini`
- Log: `x64\Release\SCC-ReadyOrNot.log`
- Menu toggle: `Insert`
- Developer clean unload: `.\Activate-RoN.ps1 -Unload`

## License and credits

MIT. Dear ImGui, MinHook and the generated SDK retain their respective notices; see [THIRD_PARTY.md](THIRD_PARTY.md). Credits to Herkkomehtala, Dumper-7 and the original component authors.

