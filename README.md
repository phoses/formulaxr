# FormulaXR

Single-player slot car racing for **Quest 3 mixed reality** (primary) and PC: one player drives two toy cars at once on a two-lane figure-8 track, the left trigger driving car 1 and the right trigger car 2. Made with Unreal Engine 5.8.

The design, decisions and progress are in [docs/plan.md](docs/plan.md).

## Play

| | |
|---|---|
| `launch-vr.bat` | Mixed reality on Quest 3 over Meta Horizon Link (passthrough) |
| `launch.bat` | PC mode on the monitor |
| `build.bat` | Packages a Shipping build into `Build\Windows`, which both launchers then use |

**Controls (Xbox controller):** left trigger drives car 1 (red), right trigger car 2 (blue). Right stick up/down moves the track up or down. In mixed reality, pressing the right stick re-centers the track in front of you. Menu restarts the race, View switches between Race and Practice, and Escape quits.

## Setup after cloning

Some third-party content isn't in this repository because its licences don't allow redistributing it:

1. **Unreal Engine 5.8** and **Visual Studio 2022 Build Tools** (C++ workload). The scripts assume UE is in `D:\Epic Games\UE_5.8`; set `UE_ROOT` if yours is elsewhere.
2. **Toy car:** add *Toy Racing Car Low-poly PBR* (Max3d) to your Fab library and download it in the Epic Games Launcher (Fab → My Library). The import script reads it from the launcher's Fab cache.
3. **Meta XR plugin** (needed for passthrough): copy a Meta XR plugin built for UE 5.8 (tested: 1.209.0) to `Plugins\MetaXR`, so that `Plugins\MetaXR\OculusXR.uplugin` exists.
4. In Meta Horizon Link, turn on **Developer Runtime Features** and **Passthrough over Meta Quest Link**, and make Link the active OpenXR runtime.
5. Run once:

```powershell
Scripts\setup.ps1
```

This builds the editor, imports the toy car (meshes, textures, red/blue materials) and creates the two car presets.

## Tests

```powershell
& "D:\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" FormulaXR.uproject -ExecCmds="Automation RunTests FormulaXR; Quit" -unattended -nullrhi -nohmd
```

These cover the figure-8 lane geometry, the slot car physics and the race rules.
