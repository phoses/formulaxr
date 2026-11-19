# FormulaXR — Slot Car Racing: Project Plan

_Last updated: 2026-11-19_

## 1. Goal

A single-player slot car game for one person driving two cars at once on a two-lane figure-8 track. Each trigger on the controller drives one car. It runs in two modes from the same build:

| Mode | Launcher | What you see |
|---|---|---|
| **PC** | `launch.bat` | Track in a virtual room, fixed overhead camera, on the monitor |
| **Mixed reality** | `launch-vr.bat` | Track placed in your real room via Quest 3 passthrough (Meta Horizon Link) |

## 2. Requirements (from brief)

| # | Requirement | Notes |
|---|---|---|
| R1 | Slot car game | Cars locked to lanes, speed is the only input, too fast in a corner = de-slot |
| R2 | Single player, two cars | One player controls both cars at the same time |
| R3 | Track has 2 lanes | Lane 1 = car 1, lane 2 = car 2 |
| R4 | Track is a figure 8 | Two loops joined by two straights that cross in the middle |
| R5 | Controller required | Xbox controller (an Xbox One Controller is already connected to this PC) |
| R5a | Left trigger → car 1 power, right trigger → car 2 power | Analog 0–1 |
| R5b | Right stick vertical → move track up/down | Both modes |
| R5c | VR mode: right stick press → re-center track | MR mode only |
| R6 | Unreal Engine 5.8, installed locally | Found: **UE 5.8.4** at `D:\Epic Games\UE_5.8` |
| R7 | Use cars from the Fab library | **Toy Racing Car Low-poly PBR** (Max3d), used for both cars in two colours, see §6 |
| R8 | `launch.bat` → PC mode | §9 |
| R9 | `launch-vr.bat` → mixed reality on Quest 3 | §9 |

## 3. What's on this computer (checked 2026-11-19)

| Item | Status |
|---|---|
| Unreal Engine 5.8.4 | ✅ `D:\Epic Games\UE_5.8` (also has the Fab plugin and Quixel Bridge for 5.8) |
| Engine plugins needed | ✅ `OpenXR`, `XRBase`, `EnhancedInput` all present in the 5.8 install |
| OpenXR active runtime | ✅ Meta Horizon Link (`C:\Program Files\Meta Horizon\Support\oculus-runtime\oculus_openxr_64.json`) |
| Gamepad | ✅ Xbox One Controller connected |
| C++ toolchain | ✅ Visual Studio 2022 Build Tools (MSVC 14.44 and 14.38). Another 5.8 C++ project here (`handletest\Creator`) compiled with it today |
| Fab car assets | ✅ Toy Racing Car Low-poly PBR downloaded to the Fab cache (`C:\ProgramData\Epic\EpicGamesLauncher\VaultCache\FabLibrary\Toy_Racing_Car_Low-poly_PBR-e29223c8`). Two Low Poly F1 models are also there but aren't used |
| Meta XR plugin (passthrough) | ✅ **v1.209.0** (built for 5.8.0), installed per project in `D:\workspace\handletest\Creator\Plugins\MetaXR`. **Passthrough over Link on UE 5.8 is confirmed working** in that project (you tested it). It gets copied into this project, see §7 |

## 4. Game design

### 4.1 Track: two-lane figure 8

- **Scale:** 1:32, the standard slot car scale. World units are real centimetres, so the track is physically sized for MR.
- **Centerline geometry:** two circles with radius `R` and centers at `(±D, 0)`, joined by the two internal tangent straights that cross at the origin. Defaults: `R = 45 cm` and `D = 1.6·R`, giving a crossing angle of about 77° and an overall footprint of roughly 2.6 m × 1.1 m. All values are tunable, plus one `TrackScale` multiplier so it can shrink to fit a desk in MR.
- **Lanes:** each lane is offset ±3.9 cm from the centerline (7.8 cm lane spacing, standard for 1:32). A figure 8 runs one loop clockwise and the other counter-clockwise, so a fixed-side offset lane is the inside lane on one loop and the outside lane on the other. **Both lanes come out exactly the same length**, so no staggered start is needed.
- **Path representation:** an analytic piecewise path (arc → straight → arc → straight) with `GetLocation(s)`, `GetTangent(s)`, `GetCurvature(s)` by distance `s`. It's exact and cheap. Curvature is `1/r` on arcs and `0` on straights. The visible track mesh is generated from the same path, so what you see always matches the physics.
- **Crossover:** a flat X crossing (the classic slot-set piece). Each lane passes through the crossing twice per lap, and the lanes intersect at 4 points. If both cars reach the same intersection point at the same time (within half a car length), **both crash**. Setting `bFlatCrossover = false` raises one straight into a bridge instead and turns off collisions.
- **Visuals:** grey track surface with a slot groove per lane, red/white curbs on the loops, a chequered start/finish line on straight A about 30 cm past the crossing, lane colour stripes matching the cars (lane 1 = red, lane 2 = blue), and a thin base board. The base board is hidden in MR, which shows a shadow-catcher plane instead (§4.6).

### 4.2 Slot car physics (per car, fixed 240 Hz sub-steps)

State: distance `s` along the lane, speed `v`, `Throttle ∈ [0,1]`.

```
motor   = Throttle · MaxAccel · (1 − v / Vmax)          // accelerates toward top speed
brake   = (1 − Throttle) · MotorBrake · v                // slot motors brake when power is off
drag    = Drag · v
v      += (motor − brake − drag) · dt
s      += v · dt
latAcc  = v² · |curvature(s)|
```

- **Grip / de-slot:** magnet grip limit `GripLimit` (≈ 1.7 g = 1650 cm/s², tuned after the first playtest). When `latAcc > GripLimit`, a `SlideMeter` fills in proportion to the overshoot and drains when grip returns. The car body yaws outward as the meter fills (a visible tail slide) and **de-slots** when the meter reaches 1. Players get a short, readable warning instead of an instant crash.
- **Playtested values (2026-11-19):** `SlideFillRate 3.0`, `SlideDrainRate 2.5`, which gives ~0.45 s of visible slide flat out into a loop, enough to lift and save it. The first values (`6.0`/`2.0`, 1570 grip) felt sudden and uncatchable.
- **Default feel targets:** `Vmax ≈ 400 cm/s` on straights. Corner limit is ≈ 260 cm/s on the inside radius and ≈ 284 cm/s on the outside radius. You must lift for every loop, and each lane takes a given loop at a different speed, which is the core skill of driving two cars at once.
- **De-slot:** the car switches to a simulated physics body (Chaos rigid body). It leaves along the tangent with its current velocity and tumbles. After 2 s a "marshal" puts it back on its lane at the point where it left, with `v = 0`. The race clock keeps running, so the lost time is the penalty.
- **Crossover crash:** both cars de-slot at the crossing.
- All tuning values live in one `UFXRSlotCarTuning` data asset so they can change without recompiling.

### 4.3 Race flow

`PreRace → Countdown (5 red lights, lights out = go) → Racing → Finished`

- **Race (default):** **both** cars must complete `N` laps (default 10). The clock stops when the second car finishes. Score = total time; best lap per lane is also shown. Local best results are saved (`USaveGame`).
- **Practice:** no lap target, live lap times per lane.
- Pressing a trigger during the countdown is allowed (real slot racing has no jump-start detection), but the car doesn't move until lights out.
- **Controller buttons:** Menu/Start restarts the race, View/Back toggles Race/Practice.

### 4.4 HUD

- One UMG widget, built in C++ (no Blueprint widget asset), showing per car: lap `x/N`, current lap time, best lap, de-slot count, plus the race clock and countdown lights.
- **PC:** screen-space overlay.
- **MR:** the same widget on a `UWidgetComponent` as a "lap tower" standing at the end of the track, facing the player.
- If no gamepad is detected, show a full-screen "Connect a controller" prompt and pause input (R5).

### 4.5 Controls (Enhanced Input)

| Action | Gamepad key | Type | Modes | Effect |
|---|---|---|---|---|
| `IA_ThrottleCar1` | Left Trigger Axis | float | both | Car 1 power |
| `IA_ThrottleCar2` | Right Trigger Axis | float | both | Car 2 power |
| `IA_TrackHeight` | Right Thumbstick Y | float, dead zone 0.2 | both | Move track up/down: squared stick response, up to ±75 cm/s, clamped to [−150, +150] cm from the 75 cm default (floor and below). Widened after a playtest: the old −50 cm limit kept the track off the floor |
| `IA_Recenter` | Right Thumbstick press | bool | MR only | Re-center the track in front of the player |
| `IA_Restart` | Special Right (Menu) | bool | both | Restart race |
| `IA_ToggleMode` | Special Left (View) | bool | both | Race ↔ Practice |

- Mapping contexts are created in C++ at startup, so there are no binary input assets to manage. `IMC_Common` applies everywhere and `IMC_MR` adds `IA_Recenter` only in MR mode.
- **Trigger response:** a small dead zone (0.03) plus an optional response curve (`Throttle = trigger^1.4`) for finer control at low throttle. Both are in the tuning asset.
- **Height adjustment:** in PC mode the camera is fixed, so moving the track up/down moves it within the view. In MR it matches the track to a real table or the floor. The height offset is saved between sessions.
- **Re-center (MR):** places the track's center 1.0 m in front of the headset along the headset's horizontal facing direction, rotates the track yaw to face the player, and keeps the current height offset.
- **Optional:** mirror the same actions on Quest Touch controllers (L/R trigger, right thumbstick) so the player can drive without the gamepad in MR. Not required.

### 4.6 Modes

**Priority:** mixed reality is the primary experience. PC mode only needs to work well, not look polished.

**PC mode (`launch.bat`)**
- HMD disabled (`-nohmd`), so a connected Quest can't take over.
- Minimal setting (a floor and a table under the track, lighting), and a fixed 3/4 overhead camera framing the whole track.
- Actors tagged `PCOnly` (room, table, base board) are visible.

**Mixed reality mode (`launch-vr.bat`)**
- Starts the HMD (`-vr`) on the Meta Horizon Link OpenXR runtime with Quest 3 passthrough as the background.
- `PCOnly` actors are hidden and the sky/background renders transparent to passthrough (post-process alpha propagation on, §7).
- The track spawns at table height (default 75 cm above the floor-level tracking origin), 1.0 m in front of the player. Right stick adjusts the height and a stick press re-centers.
- A shadow-catcher plane under the track (renders only shadows) visually grounds the track on the real surface.
- The game picks its mode at startup in `UFXRModeSubsystem`: **MR** if an HMD is enabled and connected, otherwise **PC**.

## 5. Technical approach

- **C++ project** (`FormulaXR.uproject`, module `FormulaXR`). Logic lives in C++ text files, not binary Blueprints, which makes it reviewable, diffable and practical for an AI coding agent to write. Blueprints are used only for thin data/asset subclasses where convenient.
- Builds with the VS 2022 Build Tools already installed (§3); no IDE needed.
- **Editor Python scripts** (`Scripts/`, run via `UnrealEditor-Cmd.exe -run=pythonscript`) handle repeatable asset setup: create the map, the tuning data asset, and car presets that point at the Fab meshes.
- **Rendering:** deferred renderer with `PropagateAlpha` for passthrough, Lumen off, **TAA** (not TSR) and no SSAO, both chosen by measurement in Phase 7. That gives ~3.2 ms GPU at Quest 3 stereo resolution on this PC.
- **Performance target:** 90 Hz in MR (≤ 11 ms frame), 60+ fps in PC mode.

### 5.1 Plugins

| Plugin | Why |
|---|---|
| `EnhancedInput` | Controller input (built in) |
| `OpenXR`, `XRBase` | Headset (built in) |
| Meta XR plugin (`OculusXR` 1.209.0) | Passthrough over Link. Project plugin in `Plugins/MetaXR`, copied from the working project (§7) |
| `ChaosVehiclesPlugin` | **Not used.** Slot cars don't steer, so a custom lane-following model is simpler and more accurate |

### 5.2 Main classes

| Class | Responsibility |
|---|---|
| `FFXRFigure8Path` | Analytic figure-8 lane path: length, location/tangent/curvature at `s`, crossover intersection points |
| `AFXRTrack` | Builds track meshes from the path, owns both lanes, start/finish line, crossover, base board. Exposes `Lane(int)` |
| `AFXRSlotCar` | Car body (Fab mesh), physics step, slide meter, de-slot → physics body → marshal respawn, wheel spin |
| `UFXRSlotCarTuning` | Data asset with every physics/feel number |
| `UFXRCarPreset` | Data asset: Fab mesh reference, scale to 1:32, guide-pin offset, wheel bone names (if any), body colour |
| `AFXRRaceGameMode` | Race state machine, countdown, laps, timing, crossover collision check, results |
| `AFXRPlayerController` | Input mapping contexts, routes throttle to both cars, track height and re-center |
| `AFXRPlayerPawn` | PC: fixed camera. MR: VR origin + HMD camera |
| `UFXRModeSubsystem` | Detects PC vs MR, toggles passthrough, hides `PCOnly` actors |
| `UFXRHudWidget` | Lap/timing widget used in screen space (PC) and world space (MR) |
| `UFXRSaveGame` | Best results, track height offset |

### 5.3 Folder layout

```
D:\workspace\formulaXR\
├─ FormulaXR.uproject
├─ launch.bat            ← PC mode
├─ launch-vr.bat         ← Quest 3 mixed reality
├─ build.bat             ← package a Windows build into Build\Windows
├─ Config\               ← DefaultEngine/Game/Input.ini
├─ Source\FormulaXR\     ← C++
├─ Content\
│  ├─ FormulaXR\         ← maps, materials, data assets
│  └─ Fab\ToyRacingCar\  ← imported toy car meshes, textures, materials
├─ SourceArt\ToyRacingCar\ ← copy of the original FBX + textures (Git LFS)
├─ Scripts\              ← editor Python setup scripts
├─ Build\Windows\        ← packaged game (not in source control)
└─ docs\plan.md
```

Source control: `git init` with a UE `.gitignore` (`Binaries/`, `Intermediate/`, `Saved/`, `DerivedDataCache/`, `Build/`) and Git LFS for `*.uasset` / `*.umap`.

## 6. Cars: Toy Racing Car (Fab)

Both cars use **Toy Racing Car Low-poly PBR** by Max3d: car 1 in its original red, car 2 recoloured blue.

Source files: `C:\ProgramData\Epic\EpicGamesLauncher\VaultCache\FabLibrary\Toy_Racing_Car_Low-poly_PBR-e29223c8\fbx\car_extracted\car\`

| Property | Value (checked from the files) |
|---|---|
| Format | `car.fbx` (binary FBX). The same model also comes as `.obj`, `.3ds` and `.max` |
| Units / axes | 1 unit = 1 cm, Y-up (UE converts to Z-up on import) |
| Meshes | `car` (body) and `wheels` (all four wheels in **one** mesh). No skeleton |
| Size as authored | 183.5 cm long × 106.6 cm wide × 71.1 cm tall; wheel diameter 36.8 cm; wheelbase ≈ 99 cm |
| Triangles | 5,737 total, so it's no burden for MR |
| Material | One material `car` with a 4096 × 4096 PBR set: Albedo, Normal, Roughness, Metallic, AO |

### 6.1 Import (scripted: `Scripts/import_toy_car.py`)

1. Copy `car.fbx` and `textures\` into `SourceArt/ToyRacingCar/` so the project doesn't depend on the launcher cache.
2. Import to `/Game/Fab/ToyRacingCar/` as static meshes, keeping `car` and `wheels` as **separate** meshes (combine meshes = off). Textures are capped at 2048 in game, which is plenty for an 11 cm car even when you lean in close in MR.
3. Split `wheels` into `SM_ToyCar_WheelsFront` and `SM_ToyCar_WheelsRear` (Geometry Script, same script). It splits by connected parts, groups them by which side of the car they're on, and sets each mesh's pivot on its own axle so each pair can spin.
4. Material `M_ToyCar` uses the PBR textures plus a **body recolour**: where the albedo is strongly red (`R − max(G, B)` above a threshold), the colour is replaced by a `BodyColor` parameter, keeping the shading. Tyres, windows and yellow details are untouched.
   - `MI_ToyCar_Red` keeps the original colour for car 1.
   - `MI_ToyCar_Blue` is car 2.
   - Both keep the "7" decal; different numbers are out of scope.
5. The mesh is nearly symmetric front-to-back, so confirm which end is the nose after import and store the yaw correction in the preset.

### 6.2 Scale to slot size

The toy car is chunky (width ≈ 58% of length), so **width** sets the scale, not length. Two cars side by side on 7.8 cm lane spacing must not overlap.

- Target width 6.5 cm → uniform scale **≈ 0.061**. That gives a car about **11.2 cm long × 6.5 cm wide × 4.3 cm tall**, with 2.25 cm wheels and 1.3 cm clearance between cars side by side.
- Wheel spin: `ω = v / 1.12 cm` (scaled wheel radius).
- Guide-pin pivot: on the centre line just ahead of the front axle. The body yaws around this point during the tail slide (§4.2).

### 6.3 Presets

Two `UFXRCarPreset` data assets, `DA_Car1_Red` and `DA_Car2_Blue`. They use the same body and wheel meshes, scale, pivot and yaw correction; only the material instance differs.

## 7. Mixed reality setup (Quest 3 over Link)

**Already working on this PC:** passthrough over Link runs on UE 5.8 in `handletest\Creator`. This project reuses that setup instead of working it out again.

Headset/PC (done already, listed for reference):
1. Quest 3 connected with **Meta Horizon Link** (USB-C cable or Air Link).
2. Meta Horizon Link app → Settings → Beta: **Developer Runtime Features** and **Passthrough over Meta Quest Link** on.
3. Meta Horizon Link is the active OpenXR runtime.

Project setup:
- Copy `D:\workspace\handletest\Creator\Plugins\MetaXR` → `D:\workspace\formulaXR\Plugins\MetaXR` and enable `OculusXR` in `FormulaXR.uproject`.
- Copy the proven settings into `Config/DefaultEngine.ini`:

```ini
[/Script/Engine.RendererSettings]
; empty pixels keep alpha 0 so the passthrough underlay shows through
r.PostProcessing.PropagateAlpha=True

[/Script/OculusXRHMD.OculusXRHMDRuntimeSettings]
bInsightPassthroughEnabled=True
XrApi=NativeOpenXR

[/Script/WindowsTargetPlatform.WindowsTargetSettings]
DefaultGraphicsRHI=DefaultGraphicsRHI_DX12
```

- DX12 works (proven), so **no Vulkan is needed**. Both modes use the same RHI.
- `Start in VR` = off. Mode comes from the launcher flags: `-vr` for MR and `-nohmd` for PC, the same flags the working project uses.
- The working project uses TSR (`r.AntiAliasingMethod=4`) with Lumen and ray tracing. FormulaXR targets forward shading + MSAA for 90 Hz instead (§5). If passthrough alpha breaks under forward shading, fall back to the proven deferred + TSR settings.

## 8. Testing

- **Automated (UE Automation, run headless):** path tests (lane lengths equal, curvature values, crossover points), physics tests (accelerates to `Vmax` on a straight, de-slots above the corner limit, survives just below it), lap counting and race-finish logic.
  ```
  "D:\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" FormulaXR.uproject -ExecCmds="Automation RunTests FormulaXR; Quit" -unattended -nullrhi
  ```
- **Manual checklist per milestone:** each trigger drives only its own car; full throttle in a loop de-slots; feathering keeps it on; crossover crash happens only when timing overlaps; right stick moves the track in both modes; stick press re-centers only in MR; launch files work from a fresh Explorer double-click.

## 9. Launch files

Both scripts:
- Use the packaged game `Build\Windows\FormulaXR.exe` if it exists.
- Otherwise fall back to running the project through the editor binary with `-game`, which is useful during development.
- Read the engine path from `%UE_ROOT%`, defaulting to `D:\Epic Games\UE_5.8`.

**`launch.bat`** (PC mode)
```bat
@echo off
setlocal
set "ROOT=%~dp0"
if not defined UE_ROOT set "UE_ROOT=D:\Epic Games\UE_5.8"
set "ARGS=-nohmd -fullscreen"
if exist "%ROOT%Build\Windows\FormulaXR.exe" (
  start "" "%ROOT%Build\Windows\FormulaXR.exe" %ARGS%
) else (
  start "" "%UE_ROOT%\Engine\Binaries\Win64\UnrealEditor.exe" "%ROOT%FormulaXR.uproject" -game %ARGS%
)
```

**`launch-vr.bat`** (mixed reality)
```bat
@echo off
setlocal
set "ROOT=%~dp0"
if not defined UE_ROOT set "UE_ROOT=D:\Epic Games\UE_5.8"
rem Preflight: active OpenXR runtime should be Meta Horizon Link
reg query "HKLM\SOFTWARE\Khronos\OpenXR\1" /v ActiveRuntime | findstr /i "oculus" >nul || (
  echo WARNING: Meta Horizon Link is not the active OpenXR runtime.
  echo Open the Meta Horizon Link app and set it as the OpenXR runtime.
  pause
)
echo Put on your Quest 3 and make sure Link is active (cable or Air Link).
set "ARGS=-vr"
if exist "%ROOT%Build\Windows\FormulaXR.exe" (
  start "" "%ROOT%Build\Windows\FormulaXR.exe" %ARGS%
) else (
  start "" "%UE_ROOT%\Engine\Binaries\Win64\UnrealEditor.exe" "%ROOT%FormulaXR.uproject" -game %ARGS%
)
```

**`build.bat`** (helper) runs `RunUAT.bat BuildCookRun -project=... -platform=Win64 -clientconfig=Shipping -build -cook -stage -pak -archive -archivedirectory=Build`.

## 10. Milestones

| Phase | Deliverable | Done when |
|---|---|---|
| **0. Setup** | C++ project created and compiling against 5.8.4; Meta XR plugin and passthrough settings copied (§7); toy car imported (§6.1); draft `launch.bat` / `launch-vr.bat` | Both launchers open an empty level, and `launch-vr.bat` shows passthrough in the Quest |
| **1. Track** | `FFXRFigure8Path`, `AFXRTrack` mesh generation, debug draw of lanes and crossover points | Figure 8 visible, lanes equal length (automated test) |
| **2. Driving** | Slot car physics, Enhanced Input, two toy cars at slot scale | PC mode: each trigger drives its car, cars de-slot and respawn |
| **3. Car polish** | Red/blue material instances, car presets, guide-pin pivot, tail slide, front/rear wheel spin | Both toy cars race on the track, are easy to tell apart, and look right at scale |
| **4. Race loop** | Countdown, laps, timing, crossover crash, HUD, save game, restart/mode buttons | A full 10-lap race can be finished and the result is saved |
| **5. Packaged build** | Minimal PC setting (floor + table; a full room was dropped because **mixed reality is the primary target**), camera, track height control, `launch.bat`, `build.bat`, packaged Shipping build | Double-clicking `launch.bat` plays the packaged game |
| **6. MR mode ship** | Passthrough mode, hidden `PCOnly` actors, track placement, height + re-center, lap tower HUD, shadow catcher, `launch-vr.bat` | Double-clicking `launch-vr.bat` with the Quest on Link shows the track in the real room and is playable |
| **7. Polish** | Motor whine audio (pitch follows speed), de-slot sound, tuning pass, 90 Hz perf pass | Feels good; MR holds 90 Hz |

**Status (2026-11-19):** Phase 0 is built. The C++ project compiles against 5.8.4 with Meta XR 1.209.0; the toy car is imported with red/blue materials; the scene is built at runtime; `launch.bat`, `launch-vr.bat` and `build.bat` exist. PC mode is verified by an automated screenshot (`-FXRShot=<png>`). Mixed reality is confirmed in the headset: `launch-vr.bat` shows the cars over passthrough on the Quest 3. With no Quest connected, the game falls back to PC mode. **Phase 0 is done.**

**Phase 1 is done.** `FFXRFigure8Path` gives exact lanes: a 629.1 cm lap for both, crossing twice in the crossover. `AFXRTrack` generates the track from it as a procedural mesh with vertex colours: surface, slots, lane-coloured braids, curbs, start line, and a PC-only board 268 × 124 cm. `fxr.ShowTrackDebug 1` draws the lanes and crossing points. Automated tests pass (`FormulaXR.Track.*`: equal lane length, closed path, curvature, crossings).

**Phase 2 is done.** `FFXRSlotCarModel` is the speed model: 240 Hz sub-steps, motor/brake/drag, a slide meter and de-slot. `AFXRSlotCar` follows its lane from the guide pin, swings its tail out while sliding, becomes a damped physics body when it de-slots, and is marshalled back after 2 s. The left trigger drives car 1 and the right trigger car 2. Tuning lives in `FFXRSlotCarParams` (`UFXRSlotCarTuning` asset optional). `fxr.ShowCarDebug` (on by default) shows speed, slide %, lap and de-slots on screen. Automated runs use `-FXRAutoThrottle=a,b` and `-FXRShotDelay=s`. All 9 tests pass (`FormulaXR.Car.*` adds straight-line speed, corner limit, slide recovery, throttle shape, and lapping on both lanes). Measured: about 4.9 s laps at 60% trigger; flat out de-slots in every loop at 330–360 cm/s.

**Phase 3 is done.** `UFXRCarPreset` assets `/Game/FormulaXR/Cars/DA_Car1_Red` and `DA_Car2_Blue` (`Scripts/create_car_presets.py`) hold the meshes, paint, scale, nose direction and guide position. If the assets are missing, the toy-car defaults are used. The chassis box comes from the mesh bounds: 11.2 × 6.5 × 4.3 cm. The combined wheel mesh is split into front and rear axles **at runtime in C++** rather than with Geometry Script, because it's simpler and needs no extra plugin. The wheels mesh therefore has CPU access enabled at import. Each axle spins at `v / 1.12 cm`. If the split isn't possible, the static wheels are shown instead. Spin direction is derived from the engine's rotation convention at startup; it hasn't been watched in motion yet. `-FXRCamTarget=x,y,z` points the PC camera for close-up checks.

**Phase 4 is done.**
- **Race rules:** `FFXRRace` is a pure, tested struct. A light comes on every second, five in all, then lights out after a random 0.4–1.6 s hold. The first line crossing starts lap 1 (the grid is behind the line). Cool-down laps don't count, and the clock stops when the second car finishes. Practice has no lights and no finish.
- **Crossover crashes:** checked each frame against the two lane crossing points, using each car's body length plus 3 cm.
- **Playtest fix:** a car put back by the marshal ignores the crossover until it has driven 25 cm. Before this, two cars put back at the crossing crashed again immediately, forever.
- **HUD:** `UFXRHudWidget` is pure C++ UMG. In PC mode it's on screen; in mixed reality it's a 112 cm "lap tower" behind the track (not yet seen in the headset).
- **Save game:** `UFXRSaveGame` (`Saved/SaveGames/FormulaXR.sav`) stores the best race time per lap count, the best lap per lane, and the track height.
- **Buttons:** Menu restarts, View switches Race/Practice.
- **Automation:** `-FXRLaps=n`, `-FXRLightsOut=s`, `-FXRPractice`; screenshots now include the HUD.
- **Tests:** 14 pass (`FormulaXR.Race.*` adds start lights, laps and finish, practice, no laps before the start, and the crossover point check).
- **Automated 3-lap race:** car 1 finished in 11.30 s; car 2 survived one crossover crash and finished in 15.18 s, a new record.

**Phase 5 is done.**
- **PC setting:** kept minimal (floor and table, `MI_RoomFloor`, `MI_TableTop` and `MI_TableLeg` from `Scripts/create_room_materials.py`). The full room was dropped because mixed reality is the primary target.
- **Rendering and HUD:** motion blur is off. The HUD shows a "Connect a controller" prompt when no gamepad is attached.
- **Packaging:** the engine folders the game loads at runtime (`/Engine/BasicShapes`, `/Engine/MapTemplates/Sky`) are added to `DirectoriesToAlwaysCook`. `build.bat` (Shipping, about 662 MB) packages successfully to `Build\Windows\FormulaXR.exe`.
- **Verification:** the packaged game was run with an automated race and screenshot, and all assets are present. Its saves live in `%LOCALAPPDATA%\FormulaXR\Saved`, separate from editor runs.
- **Playtest data:** player laps of 2.0–2.3 s (best 1.91 s) were checked in the logs and are genuine.
- **Mixed reality:** `launch-vr.bat` with the packaged build is confirmed in the Quest 3: passthrough works and the lap tower shows the race.

**Phase 6 is done.** The grounding shadows are confirmed in the Quest 3 over a real table.

**Phase 7 is done.** Sound, passthrough with TAA, and the wider track-height range are confirmed in the Quest 3. **All planned phases are complete.**
- **Sound:** all sounds are our own, synthesised by `Scripts/generate_sounds.py` (standard library only) and imported by `Scripts/import_sounds.ps1` to `/Game/FormulaXR/Audio`:
  - Each car has a 3D motor whine loop. Pitch goes 0.55→1.9× with speed (car 2 is 8% higher so the two can be told apart by ear), volume follows speed and trigger, and it falls silent while the car is off.
  - A crash plays a plastic clatter at the car, louder the faster it went off.
  - There's a beep per start light, a higher "go" tone at lights out, and a chime at the finish.
  - Falloff is sized for a table: full volume within 40 cm, silent at 4 m.
- **Performance (90 Hz):** `-FXRPerfLog` logs frame, game, render and GPU time. I measured the MR preview at the Quest 3 stereo pixel count (2064×1104 at 200% ≈ 9 Mpx) on the RTX 4070, editor build, with both cars racing.
  - Baseline: 15.4 ms GPU, over the 11.1 ms budget. TSR cost ~8 ms and SSAO ~4 ms; virtual shadows and SSR cost almost nothing.
  - Now: **TAA instead of TSR, and SSAO off: 3.2 ms GPU (worst 4.4 ms), 1.3 ms game thread.**
  - Risk: passthrough alpha was proven with TSR. If it breaks with TAA, use FXAA (`r.AntiAliasingMethod=1`).
- **Tuning:** the driving numbers are unchanged. The playtested feel was approved, and player laps of 2.0–2.3 s show the speed range works.
- **No shadow catcher:** Unreal has no built-in one, so grounding is faked with `M_SoftShadow`. It's translucent unlit black with opacity = vertex alpha × `Strength`, drawn on procedural meshes (`FXRSoftShadow`).
- **Track:** a 6 cm soft band along both track edges, starting 0.5 cm under the track so no seam shows.
- **Cars:** each car has a soft shadow on the track plane that follows it, including off the track. It grows and fades with height while the car tumbles. In mixed reality this replaces the cars' light-cast shadows, so they look the same on and off the track.
- **Strength:** measured from screenshots. Translucent darkening reads far weaker after tone mapping than its alpha suggests, so both use full strength.
- **`-FXRMRPreview`:** shows the mixed reality look on the monitor over a grey stand-in floor, so MR visuals (shadows, lap tower) can be checked by screenshot without a headset. Use it with `-FXRCam=0,190,110` to view from the player's side.

Passthrough is already proven on this PC, so Phase 0 only checks that the copied plugin and settings work in this project. Both launchers exist from Phase 0 on, so every later phase can be tested in both modes.

## 11. Risks

| Risk | Impact | Mitigation |
|---|---|---|
| Passthrough settings that work in `handletest` behave differently here (e.g. forward shading vs the deferred renderer there) | Black background instead of passthrough | Use the proven renderer settings (§7); Phase 0 catches this before any game work |
| Meta XR plugin updates break 5.8 compatibility | MR stops working | Pin 1.209.0 in `Plugins/MetaXR` under source control; upgrade only on purpose |
| Splitting the combined `wheels` mesh into front/rear doesn't come out clean | No wheel spin | Fall back to static wheels (at 11 cm the spin is hard to see anyway), or split once by hand in UE Modeling Mode and commit the result |
| Red-mask recolour also catches red details that should stay | Car 2 looks wrong | Tune the mask threshold, or paint a separate body mask texture once |
| Driving two cars at once is too hard | Fun | Tuning asset: slide meter warning length, grip limit, throttle curve; optional "assist" preset |
| Gamepad input while the headset is in use | UE may ignore gamepad input when its desktop window isn't focused, which can happen in MR | Verify in Phase 0 (keep the game window focused, or find a fix); Touch-controller mapping (§4.5) is the fallback |

## 12. Open questions (defaults used until answered)

1. ~~Which Fab cars?~~ **Resolved:** Toy Racing Car for both, red and blue (§6).
2. **Flat crossover with crashes, or a bridge?** _Default: flat crossover with crashes._
3. **MR placement: table or floor?** _Default: table height (75 cm), adjustable with the right stick._
4. **Race length?** _Default: 10 laps per car, both must finish._
5. **Mirror controls on Quest Touch controllers?** _Default: no, gamepad only (the brief says controller required)._

## References

- Meta: [Passthrough over Link (Unreal)](https://developers.meta.com/horizon/documentation/unreal/unreal-passthrough-use-over-link/)
- Meta: [Passthrough troubleshooting (Unreal)](https://developers.meta.com/horizon/documentation/unreal/unreal-pt-troubleshooting/)
- Working local reference: `D:\workspace\handletest` (`Creator\Config\DefaultEngine.ini`, `tools\run.ps1`)
