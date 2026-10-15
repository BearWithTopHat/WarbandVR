# 01 — Design

## 1. Goal and constraints

Turn Mount & Blade: Warband into a PC VR game, streamed to a Steam Frame,
single-player only, without access to the game's source code.

Fixed facts we design around:

- Warband is a 32-bit DirectX 9, closed-source, custom-engine game (2010).
- Official modding (the Module System) exposes game *logic*, never the
  renderer, camera, or low-level input pipeline. VR therefore requires
  interception at the D3D9 boundary, plus ordinary module scripts for
  equipment behavior.
- The game already runs on the user's PC → Steam Frame streaming setup, with
  a controller scheme. Streaming and flat input are therefore existing
  infrastructure; the mod supplies stereo frames, head pose, and VR controller
  input on the PC side.
- Single-player only. No multiplayer support, networking code, anti-cheat
  workaround, or engine memory writes.
- Stock behavior is the escape hatch: with `EnableStereo=0`, the proxy is a
  pass-through and all VR features stay inert.

## 2. Architecture overview

```text
mb_warband.exe
   │  loads (app-dir DLL search order)
   ▼
d3d9.dll  ← WarbandVR proxy
   │  Direct3DCreate9[/Ex] ──► real system/Wine d3d9.dll
   │                              │ returns real IDirect3D9[Ex]
   │                              ▼
   │              D3D9Proxy / D3D9ExProxy (delegating wrappers)
   │                              │ CreateDevice[/Ex]
   │                              ▼
   │              DeviceProxy — FULL device wrapper; game holds OUR object
   │                              │ Begin/EndScene, Present, Clear,
   │                              │ SetRenderTarget, SetTexture,
   │                              │ SetTransform / VS constants,
   │                              │ viewport/scissor, DrawPrimitive*
   │                              ▼
   │              StereoRenderer — per-eye targets, duplication gate,
   │                              eye matrices, flat-UI handling, vignette,
   │                              SBS composite or OpenXR handoff
   │                              │
   │                              ▼
   │              OpenXRBridge — session/swapchains/frame loop/pose
   │                              │
   │                              ▼
   │              MotionInput — OpenXR actions → keyboard/mouse input
   │
   ▼
monitor SBS / OpenXR runtime → SteamVR → Steam Link → Steam Frame
```

Key choice: **a full delegating COM wrapper** (`DeviceProxy`), not vtable
patching or Detours/MinHook. Steam's overlay can replace a real device
object's vtable pointer with a shadow table and restore the original memory,
silently bypassing in-place patches. With a wrapper, the game holds our object
and our vtable. Intercepted methods route to `hkXxx` functions in
`DeviceHooks.cpp`; all other device methods forward verbatim. Internal calls
run under `DeviceHooks::m_internal` so our own D3D work never re-enters the
interception path.

## 3. Stereo rendering strategy

Warband treats the camera as one view transform per frame. We cannot make the
engine render twice, but at draw-call interception time all streams, textures,
shaders, and transforms are already live. The renderer therefore executes
eligible draw calls twice, once per eye.

### 3.1 Per-eye game-sized surfaces

The current architecture uses **two game-sized color targets and two
game-sized depth buffers**, one pair per eye. It deliberately does not use a
single double-width target.

Per-eye targets matter because Warband's own coordinate-space assumptions stay
intact per eye:

- viewports and scissor rects remain game-sized;
- RHW/pre-transformed UI geometry lands where the game expects;
- the Z-prepass and later `ZFUNC_EQUAL` color pass share the same eye's depth;
- offscreen passes keep their own dimensions and textures.

The earlier double-width architecture failed exactly here: the mono Z-prepass
only fed the left half's depth, so the right eye's equal-depth color pass
collapsed to sky. That history is preserved in `docs/06`; it is not the
current design.

### 3.2 Draw routing

`DeviceHooks::DrawBothEyes` asks the pure, unit-tested gate in
`src/DupGate.h` what to do:

- `Skip`: the draw samples a diverted main-sized surface whose scene was
  already rerouted into the eye targets.
- `MonoPassThrough`: the game is rendering to its own non-main-sized
  offscreen target (for example a shadow map) and `OffscreenPasses` is not
  actively substituting per-eye copies.
- `FlatBothHalves`: a tiny UI/composite batch draws once into each eye with no
  stereo shift, so text, boxes, and the final composite quad stay aligned.
- `StereoBothEyes`: world geometry draws into left and right targets with
  eye-adjusted transforms.

At present, `Present` either composites the two eye targets side-by-side onto
the real backbuffer (`OutputMode=0`) or hands them to OpenXR
(`OutputMode=1`). `MirrorOnDesktop=1` keeps the SBS monitor image alive while
an XR session runs.

### 3.3 Where the view matrix comes from

Two delivery paths are supported:

- `ViewSource=0`: fixed-function `SetTransform(D3DTS_VIEW, …)` matrices are
  captured and replaced per eye.
- `ViewSource=1`: the shader path reads the game's actual vertex-shader
  register back with `GetVertexShaderConstantF` once per eye pair. This is
  intentional: UI elements often reuse stale transforms, so trusting a cached
  capture misaligns or reorders interface elements. The original register
  value is restored after the pair.

Warband's menu/loading observations strongly indicate the shader path is the
important one, but both paths remain supported. Orthographic-looking matrices
are treated as flat UI and are not eye-shifted.

`AsymmetricProjection=1` optionally derives an off-center projection from the
game's own perspective projection. It is available but remains opt-in until
hardware A/B verification.

### 3.4 Offscreen passes

With `OffscreenPasses=0` (default), non-main-sized render-target scenes pass
through untouched. That preserves the game's own shadow/reflection textures.
With `OffscreenPasses=1`, main offscreen scenes can be represented by per-eye
shadow render-target textures, and sampler bindings are swapped per eye. This
is implemented but hardware-unverified and should stay off unless a specific
scene requires it.

### 3.5 Clear, viewport, and scissor state

Eye targets are cleared once per frame. The game's own clears pass through
unless an opt-in shadow pass is active. The game viewport and scissor state
are tracked outside internal substitutions, applied per eye, and restored
before control returns to the game.

## 4. Output paths

- **SBS monitor** (`OutputMode=0`): two `StretchRect` calls composite the eye
  targets onto the backbuffer. This remains the fastest bring-up and fallback
  path.
- **OpenXR** (`OutputMode=1`, `Release-OpenXR|Win32`): `OpenXRBridge` owns the
  instance, D3D11 binding, session, reference space, two per-eye swapchains,
  and the game-thread-coupled frame loop. `xrWaitFrame` paces the game.
- **Slow submit path**: D3D9 eye surface → system-memory surface → D3D11
  staging texture → swapchain image. Compatible first.
- **Fast submit path** (`SharedSurface=1`): D3D9Ex shared eye textures are
  opened as D3D11 resources and copied GPU-side. Device creation is steered to
  Ex only when this option is enabled.
- **Cinema mode** (`CinemaMode=1`): submits a fixed quad layer instead of the
  immersive projection layer. Pair with `HeadTracking=0`.

OpenXR initialization failure is not fatal: the bridge cleans partial state
and retries periodically, while the desktop/SBS path remains available.

## 5. Head tracking

The implemented path samples `xrLocateViews` once per XR frame at the
predicted display time, derives a pose delta from a recentered reference,
filters it with OneEuro smoothing, and composes it onto the captured view in
`StereoRenderer::ApplyHeadDelta`.

Config keys cover the expected sign-convention problems (`InvertYaw`,
`InvertPitch`, `InvertRoll`, `InvertPosX`, `InvertPosZ`) so runtime or engine
handedness differences are tunable without a rebuild. `LockInMenus=1` freezes
tracking when the captured view is static. A recenter hotkey and right-stick
snap turning are implemented.

The deeper alternative — writing directly into Warband's engine camera struct —
is intentionally not active. It needs M0-C offsets per game build, and the
project's safety rules prohibit unverified memory writes.

## 6. Motion and controller input

MotionInput runs in the OpenXR build and uses one action per control with
both `/user/hand/left` and `/user/hand/right` subaction paths. It converts
controller state into OS-level keyboard/mouse input through the `IInputSink`
seam; production uses `SendInput`, and tests install a recording sink.

Implemented layers:

1. **Controller emulation**: left stick → WASD, right stick → mouse look or
   snap turn, face buttons → configurable keys, triggers/grips → mouse
   buttons when not owned by motion gestures.
2. **Motion melee**: right-hand velocity is ring-buffered; a swing is
   classified by its dominant axis into left, right, overhead, or thrust;
   injection is mouse direction plus a held LMB. Continued hand motion steers
   the held attack until the hand slows or a timeout releases it.
3. **Gestures**: grip presses at head-relative body zones emit configurable
   virtual-slot keys (defaults F1–F4), both grips emit F5, and releasing a
   grip in a zone emits F6 for holster/stow behavior.
4. **Bow draw ownership**: holding the right trigger owns LMB for a drawn
   bow; swing injection is suppressed while that hold is active.

The DLL deliberately does not decide inventory state. Equipment switching and
reload rituals belong to the companion module (`module_system/`), which turns
the synthetic keys into script-side Warband actions.

## 7. Performance model

Stereo duplication doubles intercepted draw work, while Warband's large
battles are already CPU-bound. The built-in `[PERF]` rollup reports added
duplication time every 600 frames. The initial target remains under 4 ms of
added CPU time per frame at 1080p-class resolution and stable 72 fps on the
host. Battle size is the primary user-facing performance lever.

The synchronous OpenXR frame loop trades architectural simplicity for pacing
correctness. If hardware testing shows frame-pacing stalls, an asynchronous
submission thread is a documented post-bring-up optimization, not a current
assumption.

## 8. Failure modes and fallbacks

| Risk | Fallback |
|---|---|
| View source differs by scene or shader | `ViewSource=0/1` capture paths plus per-draw register readback |
| Wrong `VSViewRegister` guess | implausible readbacks (NaN/Inf/zero/absurd) render mono, one-shot warning logged |
| UI separates incorrectly | `SBSMonoUI` flat duplication; `UIMaxTriangles` and `UIZScaleMax`/`UIWScaleEps`/`UIRowNormMax` classifier tuning |
| Steep-pitch views flatten | 3x3 row-norm guard in the UI classifier (`UIRowNormMax`) keeps rotations stereo |
| Offscreen effects glitch | `OffscreenPasses=0` mono passthrough; opt-in per-eye shadows |
| OpenXR runtime absent or slow to start | Logged retry; monitor/SBS path remains usable |
| XR session/instance lost mid-game | full teardown plus 5 s reinit retry (loss events and error codes both handled) |
| Runtime pacing fights the game limiter | `FramePacing=1` free-run submission throttle |
| Shared-surface path misbehaves | `SharedSurface=0` slow copy path; `SharedSyncMode=1` event-query sync for one-eye tearing |
| Steering spike whips the camera | `SteerMaxDeltaPx` per-poll clamp |
| Grab reach misreads as a swing | `GrabRefractoryMs` post-grab swing suppression |
| Head-pose axis mismatch | `[Tracking] Invert*` config keys |
| Tracking discomfort | smoothing, snap turn, vignette, cinema mode |
| Stereo overhead too high | lower resolution/battle size; disable experimental options |
| Companion module absent | bind emitted keys to Warband's native controls or disable gestures |

## 9. Explicit non-goals

- Multiplayer or networking support.
- Engine memory writes or build-specific camera/animation patching.
- Replacing Warband's UI or mission scripting system.
- Treating compile-only OpenXR code as hardware-verified.
- Native Steam Frame execution as the primary path. A Phase 2 package exists
  for experimentation, but PCVR streaming is the supported baseline.
