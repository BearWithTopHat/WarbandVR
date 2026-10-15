# WarbandVR

An open-source, single-player VR mod for **Mount & Blade: Warband** (2010),
built as a 32-bit Direct3D 9 proxy DLL. Phase 1 targets PC VR streamed to a
Steam Frame headset through Steam Link; the same rendering core also provides a
monitor side-by-side mode for bring-up and debugging.

WarbandVR is **not** a game engine replacement and does not use engine memory
writes. It intercepts Direct3D 9 rendering, renders each frame separately for
both eyes, submits the result through OpenXR, and optionally emulates keyboard
and mouse input from VR controllers.

- License: GPLv3
- Game: Mount & Blade: Warband, 32-bit Windows build
- Scope: **single-player only**
- Default state: **disabled**, stock-game behavior preserved
- Uninstall: delete `d3d9.dll` and optionally `WarbandVR.ini` from the game
  folder

## Safety model

The shipped `WarbandVR.ini` has:

```ini
[Stereo]
EnableStereo=0
```

With that setting the proxy is intended to be a transparent pass-through:
no stereo targets, no duplicated draws, no OpenXR session, and no input
emulation. Verify the stock boot first every time you install or update the
mod. If anything goes wrong, setting `EnableStereo=0` or deleting `d3d9.dll`
returns the game to stock behavior.

## Current feature set

### Rendering

- Full Direct3D 9 device proxy (`DeviceProxy`) rather than in-place vtable
  patching, so Steam's overlay cannot silently bypass the hooks.
- Per-eye, game-sized color and depth surfaces.
- Per-eye draw duplication for world geometry.
- Flat UI duplication without eye separation.
- Offscreen pass handling for shadow-style scenes.
- Monitor side-by-side output for development and fallback play.
- OpenXR projection-layer output for PC VR.
- Optional D3D9Ex shared-surface fast path, with a compatible slow path.
- Monitor spectator mirror while the headset session is active.
- Cinema mode: the game appears as a fixed virtual screen instead of a fully
  immersive projection.
- Optional world-scale presets and motion vignette.

### Tracking

- OpenXR head pose sampling.
- OneEuro smoothing.
- Recenter hotkey (F8) plus an in-headset chord: hold the left stick click
  ~3 s with the right stick centered.
- Menu tracking lock.
- Per-axis inversion configuration.
- Snap turning and continuous stick look.
- Optional controller presets (`generic`/`psvr2`/`quest` ini bundles) so
  per-controller calibration is an ini edit, not a rebuild.

### Motion input

The OpenXR build can turn controllers into ordinary keyboard and mouse input:

- left stick → WASD movement
- right stick → mouse look or snap turn
- trigger → attack
- grip → block
- face buttons → configurable Use/Jump keys
- swing classification → overhead, left slash, right slash, thrust
- held attacks can be steered by continued hand motion
- bow draw owns the left mouse button until release
- grip grabs at body zones emit configurable keys for a companion module

No multiplayer, engine memory patching, packet manipulation, or online feature
is included or planned.

### Companion module-system mod

`module_system/warbandvr_companion.py` turns the proxy's synthetic F1–F6
gesture keys into actual Warband module behavior:

- F1/F2: hip equipment slots
- F3: shoulder slot
- F4: quiver / bolt ritual step
- F5: two-hand or crossbow-spanning ritual step
- F6: holster/stow hook
- optional melee direction assist
- optional manual crossbow reload ritual

See `module_system/README.md` for installation and limitations.

## Quick start

### 1. Build

Requirements:

- Windows
- Visual Studio 2022 with Desktop C++ workload
- Windows 10/11 SDK
- Platform **Win32/x86** — mandatory because Warband is 32-bit

Monitor side-by-side build:

```bat
msbuild WarbandVR.sln /p:Configuration=Release /p:Platform=Win32
```

Headset/OpenXR build:

```bat
set OPENXR_SDK=C:\path\to\OpenXR-SDK
msbuild WarbandVR.sln /p:Configuration=Release-OpenXR /p:Platform=Win32
```

The OpenXR loader must be the **x86** build. Copy `openxr_loader.dll` beside
`mb_warband.exe` once.

### 2. Deploy

```powershell
powershell -ExecutionPolicy Bypass -File tools\deploy.ps1 -GameDir "C:\Program Files (x86)\Steam\steamapps\common\MountBlade Warband"
```

The deploy script copies `d3d9.dll` and `WarbandVR.ini` next to
`mb_warband.exe`. It does not overwrite an existing ini unless asked.

### 3. Verify stock boot

Launch the game with the shipped ini unchanged:

```ini
[Stereo]
EnableStereo=0
```

Reach the main menu and load a save. Rendering should be indistinguishable
from stock, and `WarbandVR_log.txt` should contain no unexplained errors.

### 4. Enable one feature at a time

Recommended order:

1. `EnableStereo=1`, `OutputMode=0` — monitor side-by-side.
2. OpenXR build, `OutputMode=1` — headset image.
3. `SharedSurface=1` — optional fast submission path.
4. `HeadTracking=1` — head tracking.
5. `EmulateControllers=1` — controller-to-keyboard/mouse mode.
6. `MotionMelee=1` — swing-based attacks.
7. Companion module — gesture equipment switching.

Detailed setup lives in `docs/08_Controls_and_Setup.md`.

## Comfort

`WarbandVR.ini` supports friendly world-scale presets:

```ini
[Comfort]
WorldScalePreset=realistic  ; compact, realistic, large, giant
Vignette=1
VignetteStrength=0.55
```

A preset chooses the world scale for you. For expert tuning, uncomment
`[Stereo] WorldScale`; an explicit numeric value overrides the preset.

The vignette is a subtle radial darkening rendered into both eye targets. It
is intended to reduce visual discomfort during motion without blocking the
center of view. Strength is clamped to `0..1`.

## Documentation map

- `docs/01_Design.md` — rendering and input architecture
- `docs/02_Milestones.md` — milestone status
- `docs/05_Build_Test_Deploy.md` — build, deploy, and test ritual
- `docs/08_Controls_and_Setup.md` — controls, ini reference, setup order
- `docs/manual_test_checklist.md` — hardware test checklist
- `module_system/README.md` — companion module installation
- `docs/13_Native_SteamFrame.md` — Phase 2 native investigation
- `docs/14_FAQ_Troubleshooting.md` — FAQ and troubleshooting

## Repository layout

```text
src/             Proxy DLL, renderer, OpenXR bridge, motion input
module_system/   Warband Module System companion source
tests/           Headless unit, integration, fuzz, and motion-input tests
tools/           Build, deploy, packaging, and parity-check scripts
docs/            Design, setup, risks, and testing documentation
```

## Verification status

This repository has an extensive **headless** regression gate:

```sh
sh tests/run_all.sh
```

GitHub Actions runs that gate on Ubuntu and performs real MSVC Win32 builds on
Windows for both `Release` and `Release-OpenXR`, with compiler warnings treated
as errors. The Windows jobs also verify that both produced DLLs are x86.

The local gate currently covers:

- pure logic unit tests
- fake Direct3D 9 integration tests through the real hooks and renderer
- reset and fault-injection fuzzing
- OpenXR motion-input tests with fake actions and spaces
- host compile checks
- x86 Windows cross-compile checks for both stub and OpenXR flavors
- companion module source checks
- config/source/bridge parity checks

Passing that gate means the code compiles and the testable logic behaves as
specified. It does **not** replace hardware verification. Before publishing or
tagging a release, run `docs/manual_test_checklist.md` on the real game and
headset, starting with the release-blocking stock-boot section.

## Contributing

Read `AGENTS.md` before changing code. The prime invariants are:

1. `EnableStereo=0` must preserve stock behavior.
2. Win32/x86 only.
3. No initialization work in `DllMain`.
4. Internal D3D calls must not re-enter interception paths.
5. Failure paths must be logged.
6. Single-player only.

Behavior changes require matching test updates. Do not weaken tests to make a
change pass.
