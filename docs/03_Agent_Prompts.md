# 03 — Agent prompts

Copy-paste task briefs for an autonomous coding agent. Each is self-contained:
context, exact scope, constraints, and acceptance criteria. Use them in order;
each assumes the repo invariants in `AGENTS.md`.

> **Update 2026-10-01 (second iteration):** the original M0-1, M0-2, and the
> code halves of M1-1/M2-1/M4-1 are **already implemented in the scaffold**
> (`Capture`, `CameraScan`, `SBSMonoUI`, asymmetric projection, per-eye
> clears, viewport tracking, head-pose pipeline, perf instrumentation). The
> prompts below have been rewritten accordingly: M0 tasks are now supervised
> run-and-record jobs, and coding tasks start at M1 bring-up.

**Master prompt preamble** — prepend to every task:

```
You are working in the WarbandVR repository. First read AGENTS.md and
docs/01_Design.md in full. Obey the prime invariants: the game must boot
stock with EnableStereo=0; Win32/x86 only; nothing heavy in DllMain; all D3D
hooks check DeviceHooks::Get().m_internal; single-player only. Build with:
  msbuild WarbandVR.sln /p:Configuration=Release /p:Platform=Win32
A task is done only when its acceptance criteria pass; "it compiles" is not
done. When you finish, update README.md's status table, the checkboxes in
docs/02_Milestones.md, and append decisions to docs/06_Risks_Decisions.md.
```

---

## M0-1 — View-matrix source: run the capture and record the answer
```
Task: Supervised run of the already-built capture tooling. The human executes
steps on the real game; you interpret logs and set config.

Context: src/Capture.cpp implements [Debug] CaptureMode 1 (SetTransform
aggregation) and 2 (VS register matrix-likelihood ranking). docs/04 is the
full protocol.

Do:
1. Instruct the human: CaptureMode=1, LogLevel=2, play a battle 30s, quit,
   paste WarbandVR_log.txt.
2. Interpret: "SetTransform: VIEW xNNN" per frame -> ViewSource=0. VIEW x0 ->
   instruct the human to rerun with CaptureMode=2; the log's candidate table
   gives the register -> set ViewSource=1, VSViewRegister=<top candidate>,
   VSMatrixIsViewProj per whether the matrix looked proj-like.
3. Cross-check request: one RenderDoc frame capture confirming the register
   (or the fixed-function path).
4. Record the findings in docs/04 §Results and set WarbandVR.ini defaults.

Acceptance criteria: docs/04 §Results row 1-2 filled with log evidence;
ini defaults updated; decision logged in docs/06.
```

## M0-2 — Camera struct: run CameraScan, confirm in Cheat Engine
```
Task: Supervised camera-struct hunt using the built-in scanner.

Context: src/CameraScan.cpp scans the exe's .data (or [Debug] ScanBase/
ScanSize) for ExpectedPos float triples, then rescores candidates as the
player moves. docs/04 Part C is the protocol.

Do:
1. Have the human stand somewhere known, run one capture to read the camera
   position off a logged view matrix (translation row), set ExpectedPos.
2. CameraScan=1, walk 60s, paste the converged candidate list.
3. Guide the Cheat Engine confirmation (freeze-value test, "find what
   writes") per docs/04 Part C.
4. Record the offset table keyed to the [Camera] GameBuild tag from the
   "Host exe" log line.

Acceptance criteria: <=20 candidates converged; CE-confirmed camera base +
offsets in docs/04 §Results with build tag; decision logged.
```

## M1-1 — Stereo bring-up (fix what breaks on the real game)
```
Task: Bring up side-by-side stereo (EnableStereo=1, OutputMode=0).

Context: duplication path, SBS composite, UI mono-draw heuristic
(SBSMonoUI/UIMaxTriangles), viewport tracking, and per-eye clears are
implemented. ViewSource comes from M0-1. Your job is triage and fixes from
the human's logs/screens.

Do:
1. First ensure the invariant: EnableStereo=0 boots perfectly. Fix any proxy/
   hook plumbing regression before anything else.
2. Stereo on. Triage guide, in order of likelihood:
   - Black screen: CreateResources fallback path; check log for target size.
   - Mono image: wrong ViewSource (revisit M0-1) or m_hasView never set -
     check LogViewMatrices output.
   - Double-vision UI: raise/lower UIMaxTriangles; if UI draws with big
     batches, add a state-based UI detector (2D projection heuristic:
     orthographic projection or D3DTS_VIEW==identity during the draw).
   - Crash on alt-tab/resize: hkReset ordering; DEFAULT-pool resources must
     die before the real Reset.
3. Perf sanity: read the [PERF] rollup; note baseline ms/frame.

Acceptance criteria (human-verified): stable SBS on foot and in a 100-agent
battle; correct parallax; survives alt-tab; EnableStereo=0 unaffected; log
clean at LogLevel=2.
```

## M2-1 — Offscreen passes and remaining correctness
```
Task: Handle the engine's own render passes correctly in stereo.

> Update 2026-10-01 (third iteration): the code halves of this task are now
> IMPLEMENTED: per-eye scissor (hkSetRenderState index 57 - the brief below
> said 174; that is D3DRS_SCISSORTESTENABLE's value, not a vtable index -
> plus hkSetScissorRect tracking), depth-stencil substitution (new hook
> SetDepthStencilSurface index 39), and offscreen shadow passes (per-eye
> shadow targets + SetTexture sampler swap, index 65) behind
> [Stereo] OffscreenPasses=1 (default 0). What remains is on-game
> verification and triage, plus making AsymmetricProjection the default once
> A/B-tested.

Already implemented (verify on-game, fix if wrong): asymmetric projection
(AsymmetricProjection=1), per-eye region Clear offsets, UI mono heuristic,
game viewport tracking.

Do:
1. Track SetRenderTarget/GetRenderTarget in the hooks: maintain a stack of
   game RT switches; when the game renders to its OWN surface mid-frame,
   duplicate that pass per eye into per-eye shadow copies of the surface
   (create shadows on demand, keyed by surface pointer).
2. Per-eye scissor: hkSetScissorRect currently passes through; when scissor
   test is enabled (track D3DRS_SCISSORTESTENABLE via a SetRenderState hook,
   index 174... verify against d3d9.h), offset the rect per eye like hkClear.
3. Verify AsymmetricProjection=1 against toe-in (A/B screenshots), then make
   it the default.

Acceptance criteria: reflections/shadows correct in both eyes; no stereo
artifacts around UI; [PERF] added overhead <4 ms/frame; regression: stock
boot with EnableStereo=0.
```

## M3-1 — OpenXR output
```
Task: Implement OpenXRBridge for real (WBVR_WITH_OPENXR build).

Context: src/OpenXRBridge.cpp has a skeleton with TODOs. Follow the Khronos
hello_xr D3D11 sample structure. The stereo frame arrives as a D3D9 surface
(2W x H, left eye = left half). The IDirect3D9Ex wrapper (D3D9ExProxy) is
already implemented - use it for the shared-surface fast path.

Do:
1. Add a second project configuration "Release-OpenXR|Win32" defining
   WBVR_WITH_OPENXR and linking openxr_loader.lib + d3d11.lib. Document SDK
   setup in docs/05.
2. Implement: xrCreateInstance (with XR_KHR_D3D11_enable), D3D11 device
   creation, xrCreateSession (XrGraphicsBindingD3D11KHR), LOCAL reference
   space, two swapchains sized to half the stereo target, and a frame loop
   on a dedicated thread (xrWaitFrame/BeginFrame/EndFrame with two
   projection layer views).
3. SubmitFrame: GetRenderTargetData -> staging -> D3D11 texture ->
   CopySubresourceRegion into acquired swapchain images (slow path first).
   Then the fast path behind [Output] SharedSurface=1: create the stereo
   target as D3D9Ex shared (pSharedHandle), open in D3D11 via
   OpenSharedResource. Requires the device to be Ex - the proxy now wraps
   both creation paths, and you may steer plain CreateDevice games toward Ex
   by having Direct3DCreate9 internally route through Direct3DCreate9Ex when
   Available. Log which path engaged.
4. GetHeadPoseDelta: xrLocateViews at predicted display time relative to a
   recenter reference; return yaw/pitch/roll + position. M4 consumes this.

Acceptance criteria: game visible in headset via SteamVR; stable 30-minute
session; correct left/right eye assignment (human closes each eye); falls
back to SBS with a clear log line if no runtime is present.
```

## M4-1 — Head tracking bring-up
```
Task: Switch on head tracking; the pipeline already exists.

Context: StereoRenderer::ApplyHeadDelta composes the pose delta onto the
captured view (R^T * T(-pos) * view), menu lock freezes tracking when the
view is static, the recenter key is plumbed, OneEuroFilter.h is ready. What
remains is the pose source (from M3-1 step 4) plus tuning.

Do:
1. Sample the pose ONCE per frame in StereoRenderer::OnBeginScene (cache it;
   never query per draw call). Run yaw/pitch/roll and position through
   OneEuro instances when [Tracking] Smoothing=1.
2. Wire the recenter key to actually reset the pose reference (currently it
   only logs).
3. Tune SmoothingMinCutoff/SmoothingBeta with the human's judder feedback.
4. (Stretch, needs M0-2 offsets) EngineCamera module: write head pose into
   the engine camera struct pre-frame, gated on [Camera] GameBuild matching
   the offset table's build. Refuse to write on mismatch.

Acceptance criteria (human): 1:1 look-around in battle; no judder at 72 fps;
menus rock-steady; recenter works; drift check after 10 minutes.
```

## M5-1 — Motion melee, stage 1 (emulation + capture)
```
Task: Controller support stage 1 - emulate the user's existing controller
scheme for everything except melee, and build the swing classifier (inactive).

Do:
1. New module src/MotionInput.{h,cpp}, [Input] MotionMelee=0 default.
2. Enumerate XrActions for controllers in the OpenXR session; map sticks/
   buttons to the keyboard/mouse events of the user's current flat controller
   scheme (read mapping from ini). Use SendInput; batch into one call/frame.
3. Build the swing classifier: ring buffer of controller linear velocity +
   angular velocity at 90 Hz; on dominant-axis swing > threshold m/s, classify
   into {left, right, overhead, thrust}. Log classifications (visual debug:
   overlay text into the stereo frame in Debug builds).
4. NO injection into gameplay yet - classification is observed only.

Acceptance criteria: full campaign playable with controllers using the
emulated scheme; classifier logs swings with <10% misclassification in the
human's 50-swing test set; zero impact when MotionMelee=0.
```

## M5-2 — Motion melee, stage 2 (injection)
```
Task: Wire classified swings to Warband's directional attack input.

Do:
1. Determine the engine's attack-direction mechanism (M0 RE notes, docs/04):
   mouse-delta-at-click, or an input-state struct. Implement injection via
   the least invasive path found.
2. Attack: on classified swing, inject direction + attack press. Block:
   hold-to-block with direction from controller pose.
3. Archery: two-hand aim (bow hand position, draw hand trigger); release on
   draw-hand motion.
4. Tunables in [Input]: SwingThreshold, BlockHoldTime, DrawThreshold.
   Latency instrumentation: log swing-detect -> injection timestamp delta.

Acceptance criteria (human): 20-swing melee trial vs training dummies with
>=18 intended-direction hits; swing->attack latency <50 ms; the flat
controller scheme still available by config.
```

---

### Prompt-writing rules for future tasks
- One task = one milestone item + its acceptance criteria. Never bundle.
- Always name the files the agent will touch and the ini flags it must add.
- Every prompt ends with the regression requirement: `EnableStereo=0` stock.
- If a task needs human judgment ("does it feel right"), say so explicitly and
  define exactly what the human must report back.
