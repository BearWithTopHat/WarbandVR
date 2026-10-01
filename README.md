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
| 15 vtable hooks (draw calls, transforms, scene lifecycle) | Implemented |
| Per-eye draw duplication into a double-width target | Implemented |
| Side-by-side monitor output | Implemented |
| M0 capture tooling (view-source detector, VS register ranker) | **Implemented — human runs it** (docs/00, docs/04) |
| M0 memory scanner (camera-struct candidate finder) | **Implemented — human runs it** |
| UI mono-draw heuristic (kills double-vision HUD) | Implemented |
| Game viewport tracking, per-eye region clears | Implemented |
| Asymmetric (off-center) projection | Implemented (off by default) |
| Head-tracking pipeline (pose composition, OneEuro smoothing, menu lock, recenter key) | Implemented — waits on M3 pose source |
| Perf instrumentation (duplication cost logging) | Implemented |
| View matrix source (SetTransform vs shader constant) | **M0 run pending** — one capture session answers it |
| Offscreen render-pass handling, per-eye scissor | M2 (TODOs in code) |
| OpenXR output | Stub + implementation guide (M3) |
| Motion controls | M5 (design written, classifier to build) |

**The scaffold builds clean and the game boots stock with `EnableStereo=0`.**
Every risky feature is behind a config flag. **Start at
`docs/00_Runbook.md`** — it's the shortest path from here to first stereo.

## Repo layout

```
WarbandVR/
  WarbandVR.sln / .vcxproj   Visual Studio 2022 solution (Win32 only)
  WarbandVR.def              d3d9 export list
  WarbandVR.ini              runtime config (all features flagged)
  src/                       proxy + hooks + stereo renderer + M0 tooling
    dllmain.cpp              exports, forwarding, startup, exe version tagging
    D3D9Proxy / D3D9ExProxy  IDirect3D9(Ex) wrappers -> device capture
    DeviceHooks              15 vtable hooks + per-eye draw duplication
    StereoRenderer           stereo target, eye math, head-delta composition
    Capture                  M0 view-source detector + VS register ranker
    CameraScan               M0 camera-struct memory scanner
    OneEuroFilter.h          M4 pose smoothing (header-only)
    OpenXRBridge             M3 stub with implementation guide
  docs/                      00 runbook, 01 design, 02 milestones,
                             03 agent prompts, 04 RE playbook, 05 build/test,
                             06 risks & decisions
  tools/deploy.ps1           copies the built dll + ini into the game folder
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
