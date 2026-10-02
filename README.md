# WarbandVR

A VR mod scaffold for **Mount & Blade: Warband** (single-player), targeting
**PC VR streamed to a Steam Frame** headset.

The mod is a `d3d9.dll` proxy that sits next to `mb_warband.exe`, wraps the
game's Direct3D 9 device, renders every frame twice (once per eye) with a
shifted view matrix, and outputs either side-by-side to the monitor
(milestone M1) or to an OpenXR runtime (milestone M3). From there, streaming
to the Steam Frame works exactly like the flat game already does for you.

## Current state

| Piece | Status |
|---|---|
| d3d9.dll proxy + export forwarding | Implemented |
| IDirect3D9 **and IDirect3D9Ex** wrappers, device capture | Implemented |
| DeviceProxy: full device wrapper (134 methods), 18 intercepted | Implemented — **immune to Steam overlay vtable swaps** |
| **Per-eye game-sized render targets + depth (draw rerouting)** | Implemented — **the architecture that finally worked** |
| Side-by-side monitor output (battle + menu, correct occlusion) | **Verified on the real game** |
| Flat-UI passes (ortho-projection detection) | Implemented — text aligns with boxes |
| Per-draw VS register readback (no captured-matrix stamping) | Implemented — main menu + most windows correct; **encounter dialog option layout still wrong under stereo** |
| M0 capture tooling (VS register shadowing, mode 2/3 ranking + dumps) | Implemented — register map known (c0 = per-draw transform stream) |
| M0 memory scanner (camera-struct candidate finder) | Implemented — unused so far |
| Per-eye scissor, per-eye clears, viewport tracking | Implemented (superseded by per-eye model) |
| Offscreen render-pass shadowing (`OffscreenPasses`) | Implemented, dormant (per-eye model covers it) |
| Asymmetric (off-center) projection | Implemented (off by default) |
| Head-tracking pipeline (pose composition, OneEuro smoothing, menu lock, recenter key) | Implemented — waits on M3 pose source |
| Perf instrumentation (duplication cost logging) | Implemented — ≤0.2 ms/frame measured |
| View matrix source (SetTransform vs shader constant) | **Shader-driven confirmed** (`ViewSource=1`, register c0) |
| OpenXR output | Stub + implementation guide (M3) |
| Motion controls | M5 (design written, classifier to build) |

**M1 achieved on the real game:** stock boot clean (`EnableStereo=0`), and
`EnableStereo=1` renders battles and menus side-by-side in both halves
with correct per-eye occlusion. The road there ran through two
architecture changes: vtable hooks → full `DeviceProxy` wrapper (the
Steam overlay swaps device vtable pointers), and one double-width stereo
target → per-eye game-sized targets (the game's mono Z-prepass plus
ZFUNC_EQUAL color pass made a shared double-width depth buffer
unworkable). Full story in `docs/06`. Note: the game boots into a
launcher dialog; `tools/launch_game.ps1` clicks through it for automated
runs.

## Repo layout

```
WarbandVR/
  WarbandVR.sln / .vcxproj   Visual Studio 2022 solution (Win32 only)
  WarbandVR.def              d3d9 export list
  WarbandVR.ini              runtime config (all features flagged)
  src/                       proxy + hooks + stereo renderer + M0 tooling
    dllmain.cpp              exports, forwarding, startup, exe version tagging
    D3D9Proxy / D3D9ExProxy  IDirect3D9(Ex) wrappers -> device capture
    DeviceProxy              full IDirect3DDevice9Ex wrapper (134 methods)
    DeviceHooks              18 intercepted methods + per-eye draw duplication
    StereoRenderer           stereo target, eye math, head-delta composition
    Capture                  M0 view-source detector + VS register ranker
    CameraScan               M0 camera-struct memory scanner
    OneEuroFilter.h          M4 pose smoothing (header-only)
    OpenXRBridge             M3 stub with implementation guide
  docs/                      00 runbook, 01 design, 02 milestones,
                             03 agent prompts, 04 RE playbook, 05 build/test,
                             06 risks & decisions
  tools/deploy.ps1           copies the built dll + ini into the game folder
  tools/launch_game.ps1      launches the game, clicks through the launcher
```

## Quick start

1. Open `WarbandVR.sln` in Visual Studio 2022, build **Release | Win32**.
   Output is `Release/d3d9.dll` (name matters).
2. Deploy:
   `powershell -File tools\deploy.ps1 -GameDir "C:\...\MountBlade Warband"`
3. Launch the game. With the shipped `WarbandVR.ini` (`EnableStereo=0`) the
   game is completely stock — verify `WarbandVR_log.txt` appears and shows
   `CreateDevice` + `hooks installed`.
4. Read `AGENTS.md`, then `docs/02_Milestones.md` and start at M0.

## For autonomous coding agents

This repo is designed to be worked on by an AI agent. Start with
**`AGENTS.md`** (operating manual: build/test loop, invariants, coding rules),
then pick a task from **`docs/03_Agent_Prompts.md`** (copy-paste task briefs
with acceptance criteria per milestone).

## Safety & legal

- Single-player only. Never run this against multiplayer servers.
- The proxy never modifies game files; uninstall = delete `d3d9.dll` and
  `WarbandVR.ini` from the game folder (`tools\deploy.ps1 -Uninstall`).
- Modding Warband is well-established and tolerated by TaleWorlds; DLL mods
  (like WSE2) are a normal part of its ecosystem.
