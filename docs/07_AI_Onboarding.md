# 07 — AI onboarding: read this first

Purpose: give an AI agent (or a new human) a complete mental model of this
codebase in one document, so bug-fixing starts from facts, not discovery.
Read this, then `AGENTS.md` (invariants), then the module you are touching.

## 1. What this is

A **d3d9.dll proxy** for Mount & Blade: Warband (2010, 32-bit, DX9,
single-player) that renders the game stereoscopically and (optionally) pipes
frames + head pose + motion input through OpenXR. The game is closed-source;
everything is interception at the D3D9 API boundary. There is **no engine
memory patching** except the optional, unfinished `CameraScan` experiment.

Output artifact: a 32-bit DLL named `d3d9.dll` placed next to
`mb_warband.exe`. Build config `Release|Win32` (no OpenXR) or
`Release-OpenXR|Win32` (defines `WBVR_WITH_OPENXR`).

## 2. Execution model (the spine)

```
Warband -> d3d9.dll (this DLL)
  -> dllmain.cpp      Direct3DCreate9[/Ex] exports; Startup(); Ex steering
  -> D3D9Proxy / D3D9ExProxy    wrap IDirect3D9[Ex]
  -> DeviceProxy       wraps IDirect3DDevice9; ALL 134 methods; the hook point
                       (full wrapper, NOT a vtable patch — Steam overlay swaps
                        vtable pointers, which killed the old VTableHook design)
  -> DeviceHooks       detour logic per interesting call (Present, Clear,
                       SetRenderTarget, DrawPrimitive*, SetTransform,
                       SetVertexShaderConstantF, SetViewport, SetScissorRect…)
  -> StereoRenderer    the stereo engine: per-eye render targets, eye matrix
                       injection, mono/UI handling, frame assembly, SBS output
                       or OpenXR handoff, head-pose application, menu lock
  -> OpenXRBridge      (M3/M4; real code only in WBVR_WITH_OPENXR builds)
                       session, swapchains, frame loop, pose sampling
  -> MotionInput       (M5) actions -> SendInput emulation + swing classifier
```

Threading: everything runs on the **game's render thread**. `xrWaitFrame`
paces the game to HMD refresh. Nothing heavy happens in DllMain.

## 3. Rendering pipeline invariants (where most bugs live)

1. **Per-eye surfaces.** `StereoRenderer` owns `m_eyeSurf[2]` +
   `m_eyeDS[2]`, game-sized. While `m_active` (stereo mode on), eligible draw
   calls execute **twice**, once per eye: `DrawBothEyes` binds that eye's
   target through `ApplyEye`, invokes the original draw, then restores the
   game's target and state. There is no persistent double-width RT0.
2. **D3D9 viewport auto-reset.** `SetRenderTarget` resets the viewport to the
   full target. Any code that caches/uses a viewport must capture it
   *before* RT substitution (`m_gameVp` in `OnBeginScene`). This caused the
   original half-screen bug — read the git log before touching viewports.
3. **Duplication gate.** `DecideDuplication()` in `src/DupGate.h` is the pure
   policy point: `Skip`, `MonoPassThrough`, `FlatBothHalves`, or
   `StereoBothEyes`. When the game renders into *its own* non-main-sized
   offscreen surface (shadow maps and similar), calls normally pass through
   mono. The opt-in shadow-pass path redirects sampler bindings through
   `SurfaceShadow`.
4. **SBSMonoUI heuristic.** Batches with ≤ `UIMaxTriangles` (default 64) are
   UI and drawn into both eyes WITHOUT the eye shift (`DrawMonoBothHalves`).
   Warband's final frame is a 2-triangle fullscreen composite quad — if the
   right half of the screen shows stale sky, this heuristic is the prime
   suspect.
5. **Eye math.** `EyeView = Translation(±halfSep,0,0) * view_base`;
   asymmetric projection `out._31 = -sign*halfSep*_11/nearPlane`,
   `nearPlane = -_43/_33`. View delivery is shader-driven
   (`ViewSource=1`, `VSViewRegister`) — Warband made **zero** SetTransform
   calls in menu/loading; do not rely on fixed-function transforms.
6. **Depth-stencil legality.** D3D9 requires the DS to be ≥ RT size. Eye DS
   surfaces are created per-eye at eye size; never share the game's DS.

## 4. Subsystem contracts

### OpenXRBridge (`src/OpenXRBridge.{h,cpp}`)
- `Init(device)` — lazily called from `SubmitFrame` on first frame. Failures
  clean partial state, set `g_dead`, and are retried every 5 seconds so a
  runtime that starts after the game can still come up; SBS remains the
  fallback.
- `SubmitFrame(device, leftEye, rightEye)` — pumps session events, waits/
  begins/ends an XR frame, copies both eye surfaces into per-eye swapchains
  (fast GPU path if `SharedSurface=1` and device is Ex; else slow
  GetRenderTargetData → staging → swapchain), submits one projection layer.
- `GetHeadPoseDelta(ypr[3], xyz[3])` — smoothed pose relative to recenter
  reference; sampled once per XR frame at predicted display time.
- Sign conventions are **config-tunable** (`[Tracking] Invert*`) — do not
  hardcode flips.
- Stub build (`#else`): all functions exist and are safe no-ops. Both build
  flavors must compile; never reference XR symbols outside `#ifdef`.

### MotionInput (`src/MotionInput.{h,cpp}`)
- Actions use **both-hand subactions**; reads pass the subaction path. This
  is spec-required; per-hand actions without subactions + subaction spaces
  is illegal (XR_ERROR_PATH_INVALID).
- Injection model: attack = mouse-delta + LMB **held**; while held,
  `MotionSteer` injects per-frame deltas from hand velocity (Warband steers
  held attacks from mouse movement natively). Release on hand-slow or
  `SteerMaxHoldMs`. `Shutdown` must release any held key/button (no stuck
  inputs).
- All injection via `SendInput` — no engine memory writes.

### Config (`src/Config.{h,cpp}`)
- INI keys grouped `[Stereo] [Output] [Tracking] [Input] [InputMap]
  [Camera] [Debug]`. Defaults must keep stock behavior with
  `EnableStereo=0`. Key names parse via `VkKeyScanA` + name table.

## 5. Debugging playbook

| Symptom | First suspects |
|---|---|
| Right half stale/sky | SBSMonoUI composite-quad heuristic; DecideDuplication gate / RT routing |
| Image only in one eye | swapchain/eye mixup (eye 0 = left); SubmitEyes acquire failure |
| Crash on overlay open | DeviceProxy pass-through (vtable design is dead; stay wrapper) |
| SetRenderTarget fails | DS smaller than RT — eye DS must be eye-sized |
| Black in HMD, fine on monitor | OutputMode=1 path: check `OpenXRBridge` log lines, blend mode, `shouldRender` |
| HMD world moves wrong direction | `[Tracking] Invert*` keys — fix from ini, not code |
| Swing attacks wrong direction | classifier axis mapping (app space: +x right, +y up, -z fwd); check `[MELEE]` log |
| Keys stuck after exit | MotionInput::Shutdown release path |
| Game won't start with mod | `EnableStereo=0` must be stock; check Logger output |

Logging: `Logger.h` (`LOG_INFO/WARN/ERROR`); file next to the DLL. Frame
dumps: `[Debug] DumpFrames=N`. Perf rollup every 600 frames.

## 6. Hard rules (from AGENTS.md, expanded)

- Stock boot with `EnableStereo=0`; Win32/x86 only; nothing heavy in
  DllMain; all hooks check `DeviceHooks::Get().m_internal`; single-player
  only; static CRT (/MT); singletons only; no engine memory writes.
- `openxr_loader` must be **x86** to link; the DLL must sit next to the proxy.
- Both build configurations must compile after every change.

## 7. Known-deferred / known-risky

The full register with testing paths lives in `docs/09` — read it before
debugging anything on real hardware.

- The M3–M5 code was written without headset access. It is compile-verified
  locally via clang/zig cross-compilation (both flavors) and covered by the
  fake-OpenXR motion-input tests, and GitHub Actions is configured for real
  MSVC Win32 builds. It has still NOT been run on hardware, and the remote CI
  workflow has not been executed from this sandbox.
- Game-thread-coupled submission locks game fps to HMD refresh; async
  submission thread is a documented post-M3 optimization.
- Eye swapchains are game-resolution sized, not runtime-recommended size;
  if the HMD image looks soft, investigate scaling.
- M0-C (engine camera struct) is unfinished; the camera-write path (cleaner
  than matrix injection) is blocked on it.
- True 1:1 weapon-pose control is not possible without engine internals;
  see docs/01 §6 and the MotionSteer design comment in MotionInput.cpp.

## 8. Test infrastructure (headless)

Pure logic lives in host-compilable headers with **no Windows/D3D9/XR
includes** — include them directly in tests:

| Unit | Header | Covers |
| --- | --- | --- |
| Matrix core | `src/Mat4.h` (`namespace MatPure`) | EyeView/EyeProjection/RotationYPR; `MathUtil.h` is the D3DMATRIX boundary wrapper |
| Duplication gate | `src/DupGate.h` | `DecideDuplication()` -> Skip / MonoPassThrough / FlatBothHalves / StereoBothEyes |
| Swing math | `src/MeleeCore.h` (`namespace Melee`) | VelocityToView, ClassifySwing, FindPeak, SteerRelease, SteerDelta, BeginDelta |
| Grab zones | `src/GestureZones.h` | `ClassifyZone(hand, hp, head)` head-relative boxes |
| INI parsing | `src/ParseUtil.h` (`namespace Parse`) | KeyFromString, ULong, Vec3 (Config.cpp keeps Win32 file I/O + VkKeyScanA fallback) |
| Input injection | `src/InputSink.h` | `IInputSink`; production = SendInputSink, tests = recording fake |
| Pose smoothing | `src/OneEuroFilter.h` | deterministic, already pure |
| Comfort policy | `src/Comfort.h` | world-scale presets, vignette clamp/pixel math |

Run the full gate (host compiler, no game/GPU/HMD):

    sh tests/run_all.sh

It builds and runs pure-unit, fake-device integration, fault-injection fuzz,
and fake-OpenXR motion-input tests; compiles the proxy/entry translation units
with warnings fatal; cross-compiles both stub and OpenXR flavors for x86
Windows with zig; and runs companion, license, workflow, and parity checks.
For package comparison only:

    python3 tools/check_parity.py <pcvrDir> <nativeDir>

Integration harness (tests/test_integration.cpp): the REAL DeviceHooks,
StereoRenderer, Config, Capture and Logger compile against tests/winshim/
(a minimal Windows/D3D9 header + implementation layer) and run against
FakeDevice (tests/fake_device.*), which implements the full 119-method
IDirect3DDevice9 vtable and records every call. Scenarios assert the
duplication gate end-to-end: per-eye duplication, flat 2-triangle composite
quads (no eye-shifted transform), offscreen-RT passthrough, diverted-surface
skips, the m_internal guard, and the EnableStereo=0 stock path.
Fake* trivial methods live in tests/*_generated.inc (regenerate from mingw
d3d9.h if the interface set changes).

MotionInput's XR-gated layers are also headless-tested now:
`tests/test_motion_input.cpp` links the real `MotionInput.cpp` against
`tests/xr_fake.*`, installs a recording `IInputSink`, and drives emulation,
swing steering, gestures, bow-draw ownership, failure paths, and shutdown
cleanup.

Behavior changes to any unit above REQUIRE updated tests in the same commit.
What still needs a human: `docs/manual_test_checklist.md`.
