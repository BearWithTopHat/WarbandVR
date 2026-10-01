# 01 — Design

## 1. Goal and constraints

Turn Mount & Blade: Warband into a PC VR game, streamed to a Steam Frame,
single-player only, without access to the game's source code.

Fixed facts we design around:

- Warband is a 32-bit DirectX 9, closed-source, custom-engine game (2010).
- Official modding (module system) exposes game *logic*, never the renderer,
  camera, or input pipeline. VR therefore requires engine-level injection.
- The game already runs flawlessly on the user's PC → Steam Frame streaming
  setup, with a controller scheme. So: **streaming and flat-input are solved
  problems**; the mod only has to produce stereo frames and (later) accept
  tracked input. The Frame itself never runs our code.
- Single-player only, so no anti-cheat, no server compatibility, no fairness
  concerns. We can reshape input as aggressively as we like.

## 2. Architecture overview

```
mb_warband.exe
   │  loads (app-dir DLL search order)
   ▼
d3d9.dll  ← WarbandVR proxy
   │  Direct3DCreate9 ──► real SysWOW64\d3d9.dll
   │                         │ returns real IDirect3D9
   │                         ▼
   │              D3D9Proxy (delegating wrapper)
   │                         │ CreateDevice
   │                         ▼
   │              DeviceHooks — vtable hooks on the real IDirect3DDevice9
   │                         │ BeginScene → substitute double-width RT
   │                         │ SetTransform / VS-constant → capture view matrix
   │                         │ DrawPrimitive* → execute twice, once per eye
   │                         │ Present → composite SBS (M1) or OpenXR (M3)
   │                         ▼
   │              StereoRenderer — stereo target, eye matrices, composite
   │                         │
   │                         ▼
   │              OpenXRBridge (M3) → OpenXR runtime → SteamVR → Steam Link
   │                                                              │
   ▼                                                              ▼
  monitor (SBS, debugging)                              Steam Frame headset
```

Key choice: **vtable hooking**, not Detours/MinHook. COM objects are called
through a per-class function table; we patch the table of the one device
instance the game creates. Precise, no trampolines, no instruction patching.
(`src/VTableHook.h`, install list in `DeviceHooks.cpp`.)

## 3. Stereo rendering strategy

Warband's renderer treats the camera as one view matrix per frame. We cannot
make the *engine* render twice — but we can make every **draw call** execute
twice, because at interception time all render state (streams, textures,
shaders, transforms) is already set. The plan, per intercepted draw:

1. Set viewport to the left half of a double-width render target.
2. Overwrite the view matrix with `EyeView(base, +IPD/2)` (see below).
3. Call the original draw.
4. Repeat for the right half with `-IPD/2`.
5. Restore the game's viewport and view matrix.

`EyeView = Translation(+/- IPD/2, 0, 0) * view_base` (view-space eye offset —
the standard "translate the camera in head space" formulation; see
`MathUtil.h`). Projection stays the game's symmetric one at first (slight
toe-in distortion, acceptable for M1). **M2 upgrade**: off-center (asymmetric)
projection for geometrically correct stereo.

### 3.1 Where does the view matrix come from? (the M0 question)

DX9 games deliver view transforms two ways, and we support both
(`WarbandVR.ini → ViewSource`):

- **ViewSource=0 — fixed function**: `SetTransform(D3DTS_VIEW, …)`. We capture
  it in `hkSetTransform` and overwrite per eye. Try this first — Warband is
  old enough that much of its pipeline may be fixed-function.
- **ViewSource=1 — shader constant**: the engine packs view (or view·proj)
  into vertex shader registers via `SetVertexShaderConstantF`. We capture at
  the configured register and rewrite it per eye. `docs/04` describes how to
  find the right register with PIX/RenderDoc.

M0's job is to determine which Warband uses (likely *both*: fixed-function for
some passes, shaders for skinned characters — in that case the agent must
handle both simultaneously).

### 3.2 Render-target redirection

At `BeginScene` we swap RT0 for our `2W × H` target and swap back at `Present`.
Known hole (M2): if the engine re-points RT0 mid-frame (shadow maps, water
reflections, scope views), those passes currently render to the game's own
surfaces and then composite onto *our* target — mostly harmless, sometimes
glitchy. The M2 fix is to track `SetRenderTarget` calls, keep a per-eye
shadow copy chain, and duplicate offscreen passes per eye too. TODOs mark the
hook points.

### 3.3 Clear/scissor/viewport duplication

Full-target `Clear` covers both eyes in one call. Region clears and scissor
rects are game-coordinate-space and need per-eye offsets (partially
implemented, marked TODO(M2)). Viewports are set per eye in
`StereoRenderer::ApplyEye`.

## 4. Output paths

- **M1 — SBS monitor** (`OutputMode=0`): two `StretchRect` calls at `Present`
  copy each eye half onto the real backbuffer. Lets us validate geometry
  stereo with zero VR hardware; cross-eye or record a video to check.
- **M3 — OpenXR** (`OutputMode=1`, `WBVR_WITH_OPENXR` build): `OpenXRBridge`
  owns an XrInstance/XrSession (D3D11 binding), two swapchains, and a frame
  loop. The stereo surface reaches swapchain images via:
  - *Short-term*: `GetRenderTargetData` → upload to a D3D11 texture →
    `CopySubresourceRegion` into the swapchain image. Slow (~1–2 ms) but
    correct; fine for first headset pixels.
  - *Long-term*: D3D9Ex shared render target → `OpenSharedResource` in D3D11,
    zero-copy. The proxy already wraps both creation entry points
    (`D3D9Proxy` / `D3D9ExProxy`), so M3 can steer device creation through
    Ex and share surfaces — a local change, not a new subsystem.

Once the mod speaks OpenXR on PC, **SteamVR + Steam Link streaming to the
Frame is infrastructure the user already has** — no Frame-specific code.

## 5. Head tracking (M4)

Two layers, in order of implementation:

1. **Pose delta on the captured view matrix** (no engine RE needed):
   `view = T(eye) * R(headDelta) * view_captured`, where `headDelta` comes
   from `OpenXRBridge::GetHeadPoseDelta` (xrLocateViews at predicted display
   time, relative to a recentered reference). Rotational tracking first;
   positional scaled by `WorldScale`. This is VorpX-style "head-as-mouse"
   and gets us 80% of presence for 20% of the work.
2. **True camera coupling** (needs M0 RE results): write head pose directly
   into the engine's camera struct before it builds the view matrix, so game
   logic (collision, aim, mounted camera smoothing) stays consistent. The
   M0 playbook (`docs/04`) produces the offsets; the write happens in a new
   `EngineCamera` module called from `hkBeginScene`.

Roll from head tracking should be applied; game-driven camera roll is rare in
Warband, so no special handling. All tracking goes through a one-Euro or
simple smoothing filter — raw poses fed into a DX9-era frame loop will jitter.

## 6. Input (M5) — design before code

The user already plays flat with a controller, so **menus and map navigation
are solved** — keep controllers emulating that scheme (Steam Input or
SendInput injection) for everything except combat.

Combat is the interesting part. Warband melee = 4 directional attacks +
directional blocks, chosen by mouse movement at button-press. Design:

1. Track controller angular velocity + linear swing vector each frame.
2. On trigger threshold (configurable m/s), classify the swing's dominant
   plane into {left, right, overhead, thrust}.
3. Inject the corresponding mouse-movement-then-attack sequence the engine
   expects (determined during M0/M4 RE; likely via `SendInput` mouse deltas
   or by writing the engine's input state directly — RE will tell).
4. Blocks: hold-to-block with direction from controller orientation.
5. Archery: two-handed aim — bow hand = left controller position, draw hand
   trigger; release arrow on draw-hand release past threshold.

Everything ships behind `InputMode` ini flags, defaulting to today's working
controller scheme.

## 7. Performance budget

Target: stable 72 fps (Frame's minimum refresh) at 1080p-ish per eye on the
host PC. Stereo doubles draw calls; Warband is CPU-bound in large battles
(single-threaded agent AI), so battle size is the lever, not resolution. M2
includes measuring added overhead (must be <4 ms/frame on the user's machine)
and documenting recommended battle-sizer settings.

## 8. Failure modes & fallbacks

| Risk | Fallback |
|---|---|
| Warband uses shader-constant view matrices everywhere | ViewSource=1 path (built) |
| View matrix can't be reliably captured | Z-buffer stereo (SuperDepth3D-style) as a degraded mode — documented, not implemented |
| Engine camera write (M4) proves unstable | Stay on matrix-delta tracking forever (VorpX-style) |
| OpenXR D3D9 path too slow | D3D9Ex shared-surface path; worst case, SBS → Steam Link's built-in SBS-to-VR |
| Doubled draw calls tank big battles | Battle-size cap recommendation; dynamic IPD=0 fallback to mono for menus |

## 9. Explicit non-goals

- Multiplayer. Anti-cheat and server trust make it a different project.
- Native Steam Frame (Arm64) execution — re-evaluate in a year; streaming is
  the supported path.
- Replacing the game's UI — we project it, we don't rebuild it.
