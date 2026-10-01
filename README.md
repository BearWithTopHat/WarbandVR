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
| Per-eye draw duplication into a double-width target | Implemented — **verified running on the real game** |
| Stereo depth-stencil substitution (D3D9 legality for the 2W target) | Implemented — verified on the real game |
| Side-by-side monitor output | Implemented |
| M0 capture tooling (view-source detector, VS register ranker) | **Implemented — human runs it** (docs/00, docs/04) |
| M0 memory scanner (camera-struct candidate finder) | **Implemented — human runs it** |
| UI mono-draw heuristic (kills double-vision HUD) | Implemented |
| Game viewport tracking, per-eye region clears | Implemented |
| Per-eye scissor rects (SetRenderState + SetScissorRect hooks) | Implemented (needs on-game verification) |
| Offscreen render-pass shadowing (per-eye shadow targets + sampler swap) | Implemented behind `OffscreenPasses=1` (untested on game) |
| Asymmetric (off-center) projection | Implemented (off by default) |
| Head-tracking pipeline (pose composition, OneEuro smoothing, menu lock, recenter key) | Implemented — waits on M3 pose source |
| Perf instrumentation (duplication cost logging) | Implemented |
| View matrix source (SetTransform vs shader constant) | Likely **shader-driven** (zero `SetTransform` calls in menu/loading) — M0 run confirms register |
| OpenXR output | Stub + implementation guide (M3) |
| Motion controls | M5 (design written, classifier to build) |

**Verified on the real game:** stock boot clean (`EnableStereo=0`), stereo
boot clean (`EnableStereo=1` — stereo target/depth substitution active,
survives `Reset`, no errors). Big fix this round: the Steam overlay swaps
the device object's vtable pointer, which defeated the original vtable
hooks; interception now happens in a full `DeviceProxy` wrapper. Note: the
game boots into a launcher dialog; `tools/launch_game.ps1` clicks through
it for automated test runs.

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
