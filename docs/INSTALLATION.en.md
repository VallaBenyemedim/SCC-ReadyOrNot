# SCC — Ready or Not installation

## Supported game version

The current loader expects Steam build `25784851` and this executable SHA-256:

```text
FF445331E7144A2DC79EC50C2E6FD42F2A2E7B49AE7B03AE1A6076DCFC769262
```

It also reads the SDK constants from `RoN-Internal/SDK/Basic.hpp` and validates `GObjects`, `GWorld` and `ProcessEvent` against the running game before loading the DLL.

## Release installation

1. Download `SCC-ReadyOrNot-<version>.zip` from GitHub Releases.
2. Extract the complete archive. Keep the launcher, PowerShell script, DLL and included `RoN-Internal/SDK/Basic.hpp` path together.
3. Start Ready or Not through Steam and wait for the main menu or enter a private/single-player session.
4. Run `SCC-ReadyOrNot-Aktif-Et.cmd`.
5. The launcher must report:
   - compatibility verified;
   - ImGui menu and HUD connection verified.
6. Press `Insert` to toggle SCC.

The launcher does not modify the game executable or save files on disk.

## Controls and profile

- `Insert`: show/hide the menu.
- System → **Save**: persist the current options.
- System → **Reload**: restore the saved profile.
- System → **Defaults**: restore safe defaults.
- System → **Completely unload**: restore session-owned runtime changes, detach graphics/input hooks and unload the DLL.

The profile and log are created next to the DLL as `SCC-ReadyOrNot.ini` and `SCC-ReadyOrNot.log`.

## Updating or removing SCC

Use the in-menu unload action or:

```powershell
.\Activate-RoN.ps1 -Unload
```

Wait for `REMOVED` before replacing the DLL. The game can stay open. To remove the project permanently, delete the extracted folder only after SCC has unloaded.

## Build from source

Install Visual Studio 2022 Build Tools with MSVC v143 and a Windows SDK, then:

```powershell
git clone --recurse-submodules https://github.com/VallaBenyemedim/SCC-ReadyOrNot.git
cd SCC-ReadyOrNot
.\Build-RoN.ps1
.\Activate-RoN.ps1 -CheckOnly
```

If compatibility succeeds, activate with:

```powershell
.\Activate-RoN.ps1
```

## Troubleshooting

- **DLL not found:** build the Release x64 configuration or use the complete release ZIP.
- **Game version mismatch:** do not force-load; update SCC's generated SDK/offsets for the new build.
- **Object table / ProcessEvent mismatch:** the live SDK layout differs from this release.
- **World not ready:** wait for the main menu or load a mission, then retry.
- **HUD/menu not verified:** check `SCC-ReadyOrNot.log`; switch to the game window and allow a few rendered frames.
- **Already active:** use the existing menu; duplicate loading is intentionally blocked.
- **A control has no authority:** gameplay writes require the local host/authorized session. ESP remains independent.

Do not disable Windows security controls to run SCC. Build from source if your environment does not trust the provided binary.

## Responsible use

Use SCC only in offline, single-player or explicitly authorized private sessions. The repository does not provide anti-cheat evasion, public-server deployment instructions or support for competitive use.

