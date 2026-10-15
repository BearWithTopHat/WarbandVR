# AGENTS.md — operating manual for autonomous coding agents

You are working on **WarbandVR**, a 32-bit Direct3D 9 proxy DLL that adds VR
to Mount & Blade: Warband. Read this file fully before touching anything.
Then read `docs/01_Design.md` for architecture and `docs/02_Milestones.md`
for where the project stands.

## Prime invariants (never violate)

1. **The game must always boot stock.** With `EnableStereo=0` in
   `WarbandVR.ini`, the proxy must be a perfect pass-through. Any change that
   breaks this is a release-blocking defect.
2. **Win32/x86 only.** Warband is a 32-bit process. Never add x64 configs,
   64-bit-only APIs, or libraries without 32-bit support.
3. **No work in DllMain.** Loader-lock rules: initialization happens lazily on
   `Direct3DCreate9`.
4. **All interceptions check `DeviceHooks::Get().m_internal` first** and pass
   straight through when it's set. Internal D3D calls (our own
   viewport/matrix/target changes) must always run with `m_internal = true`
   or they re-enter the interceptions and infinite-loop.
5. **Log generously, cheaply.** Every failure path logs with `LOG_ERROR` /
   `LOG_WARN`. The log is the only debugging tool once inside the game.
6. **Single-player only.** Never add networking code, and refuse tasks that
   target multiplayer.

## Build

```
msbuild WarbandVR.sln /p:Configuration=Release /p:Platform=Win32
```

Output: `Release/d3d9.dll` (the `TargetName` is intentionally `d3d9` —
do not change it).

Requirements: Visual Studio 2022 (v143 toolset), Windows 10 SDK. No external
dependencies in the default build. The optional OpenXR path needs
`-D WBVR_WITH_OPENXR` (add to preprocessor definitions), the OpenXR SDK, and
`openxr_loader.lib`. The `Release-OpenXR|Win32` solution configuration already
carries these; the CI workflow builds the x86 loader from a pinned SDK tag.

## Deploy & test loop

```
powershell -ExecutionPolicy Bypass -File tools\deploy.ps1 -GameDir "<folder with mb_warband.exe>"
```

Then launch the game via Steam **or** `mb_warband.exe` directly. Steam launch
works fine with the proxy in place.

Test ritual after every change (details in `docs/05_Build_Test_Deploy.md`):

1. Build clean (0 errors, 0 new warnings).
2. Deploy, set `EnableStereo=0`, boot to the main menu. **Must not crash,
   must not change rendering.** Check `WarbandVR_log.txt` for errors.
3. Only then test the feature with its flag on.
4. If the game crashes on boot: restore `EnableStereo=0`; if it still crashes,
   the regression is in proxy/hook plumbing — bisect your diff.

## Coding conventions

- C++17, MSVC, `/W3`. No exceptions thrown across COM/DLL boundaries
  (internally fine). No RTTI dependencies. No C++20 modules.
- Static CRT (`/MT`) — the dll must not require VC++ redistributables.
- All globals live in singletons (`Config::Get()`, `StereoRenderer::Get()`,
  `DeviceHooks::Get()`). No naked static objects with non-trivial destructors
  (DLL unload ordering).
- New intercepted device methods: add an override in `DeviceProxy`
  (forwarding to `hkXxx`), write `hkXxx` in `DeviceHooks.cpp`, and from it
  call `dev->Xxx(...)` on the real device (NOT on the proxy — that would
  recurse). We do NOT patch vtables: the Steam overlay swaps the device
  object's vtable pointer to a heap shadow, which silently bypasses in-place
  vtable patches (see docs/06, 2026-10-01 iteration 4). The full
  `DeviceProxy` wrapper is immune to that.
- Keep detours tiny. Logic belongs in `StereoRenderer` (rendering) or a new
  module (input, OpenXR) — hooks just route.

## What "done" means per task

Each task brief in `docs/03_Agent_Prompts.md` has an **Acceptance criteria**
list. A task is done when every box passes on a real machine with the real
game, the log is clean, and invariant 1 still holds. "Compiles" is not done.

## Housekeeping

- Update `docs/02_Milestones.md` checkboxes and `README.md`'s status table
  whenever a task lands.
- Log significant decisions in `docs/06_Risks_Decisions.md` (append-only).
- Commit early and often; commit messages start with the milestone tag
  (`[M1]`, `[M2]`, ...).

## Tests

Headless unit tests for the pure logic units live in `tests/` (see
`docs/07_AI_Onboarding.md` section 8). After any change to `src/Mat4.h`,
`src/DupGate.h`, `src/MeleeCore.h`, `src/GestureZones.h`, `src/ParseUtil.h`,
`src/Comfort.h`, `src/OneEuroFilter.h`, `src/MathUtil.h`,
or the duplication/swing/gesture logic that uses them: run the suite and
`tools/check_parity.py`, and update tests to match intended behavior.
