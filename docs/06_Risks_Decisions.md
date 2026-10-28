# 06 — Risks, decisions, and session log

Append-only. Decisions are dated; if a decision is reversed, add a new entry
— don't edit history. The **decision log and session log are historical**:
entries describing the old double-width target, vtable hooks, or unimplemented
skeletons are evidence of what changed, not current architecture. The living
risk register below is kept current.

## Risk register (living)

| # | Risk | Likelihood | Impact | Mitigation / fallback | Status |
|---|---|---|---|---|---|
| R1 | Warband delivers view matrices only via shader constants, in a layout that varies per shader | Medium | M1 blocked on path 0 | ViewSource=1 path built into scaffold; M0 capture tooling ranks candidate registers | Open |
| R2 | Engine re-points RT0 mid-frame (shadows/reflections) breaking per-eye duplication | Medium | Visual glitches | M2-1: shadow-surface chain per eye | Open |
| R3 | CPU-bound battles + doubled draw calls miss 72 fps on host PC | Medium | Comfort | Battle-size guidance; overhead budget <4 ms enforced in M2 | Open |
| R4 | Engine camera struct never found reliably (M0-C fails) | Low-Med | M4 degraded | Stay on matrix-delta tracking (VorpX-style) permanently | Open |
| R5 | D3D9→OpenXR copy path too slow | Low | Latency | D3D9Ex shared-surface fast path implemented (`[OpenXR] SharedSurface=1`, Ex steering); CPU-copy slow path and SBS→Steam Link as fallbacks. Throughput unmeasured on hardware | Open |
| R6 | Game update breaks offsets/hook assumptions | Low | Sudden crash | Version-gate writes (`[Camera] GameBuild`); EnableStereo=0 always safe | Open |
| R7 | Motion melee injection misread by engine (attack direction stochastic) | Medium | M5 quality | M0-C input-state RE; extensive tunables; keep flat scheme as config option | Open |
| R8 | Scope creep into multiplayer | — | Project death | Explicit non-goals in design doc; AGENTS.md invariant 6. Native Steam Frame is now a Phase-2 package sharing this source tree (not scope creep), but its runtime is unverified | Controlled |
| R9 | Draw duplication misses frame budget in large battles (R3 realized) | Medium | Comfort | Depth-reprojection fallback (BannerlordVR lesson, docs/15): render one eye, rebuild the other from the Z-prepass depth at composite time. Design only; implement only if R3 measures bad on hardware | Open |
| R10 | Frame-rate-dependent input feel at non-vanilla cadences | Medium | Comfort/gameplay | All rate-based injection (look, steering) is dt-scaled to 60 fps. WSE2's fix history confirms the bug class | Mitigated |
| R11 | WSE2-x64 or threaded-renderer coexistence | Medium | Incompatible | Documented as incompatible/untested in FAQ; no x64 build planned (stock Warband is 32-bit) | Controlled |

## Decision log

### 2026-10-01 — Proxy + vtable hooks, not API hooking
Chosen: d3d9.dll proxy + per-instance COM vtable swapping. Rejected:
Detours/MinHook on d3d9 exports (system-wide blast radius, loader timing),
full 119-method device wrapper (mechanical bulk, error-prone; can be added
later if vtable approach hits an edge case).

### 2026-10-01 — Intercept-time draw duplication, not frame recording
Draw calls are duplicated at interception time while game state is live.
Rejected: full command-buffer recording/replay (needs the complete device
wrapper, doubles memory traffic). Revisit only if profiling shows
duplication bugs with state blocks.

### 2026-10-01 — PC VR + Steam Link streaming is the only supported runtime
The user already streams the flat game to a Steam Frame successfully with a
controller scheme; the mod inherits that pipeline. Native Arm64 Frame
execution is a non-goal (emulated x86 process vs native OpenXR runtime is an
unowned compatibility boundary; mobile-chip stereo perf for CPU-bound
battles is dubious).

### 2026-10-01 — Single-player only, enforced as a repo invariant
Injection mods and multiplayer don't mix (server trust, fairness, bans).
AGENTS.md invariant 6; no networking code will ever be added.

### 2026-10-01 — Config-gated everything; stock boot is the prime invariant
Every feature behind `WarbandVR.ini`, master switch `EnableStereo=0` ships
as the default. Uninstall = delete two files. This is what makes
autonomous-agent iteration safe: worst case is always one file delete.

### 2026-10-01 (iteration 2) — Everything agent-doable was done upfront
Policy decision: any work that doesn't require the physical game/headset was
implemented immediately rather than left in task briefs. Concretely: M0
tooling (`Capture`, `CameraScan`), M1's UI mono-draw heuristic, M2's
asymmetric projection + per-eye clears + viewport tracking + perf
instrumentation, M4's entire pose pipeline (composition, OneEuro smoothing,
menu lock, recenter plumbing), and the IDirect3D9Ex wrapper for M3's
zero-copy path. The agent prompts were rewritten so remaining tasks are
bring-up, triage, and verification rather than greenfield code.

### 2026-10-01 (iteration 2) — IDirect3D9Ex wrapped from day one
Originally the Ex path was an M3 gap ("passed through unwrapped"). Wrapped
now so the M3 zero-copy shared-surface path is a local change in
`CreateResources` + device creation steering, not a new subsystem. Note:
`IDirect3DDevice9Ex`'s vtable shares the base layout, so the 15 existing
hooks apply unchanged.

### 2026-10-01 (iteration 3) — Scaffold bit-rot fixed; first real-game boot
The scaffold had never actually compiled: (a) `__declspec(dllexport)` on
`dllmain.cpp`'s export definitions clashes with d3d9.h's own declarations
(C2375 on MSVC v143 + SDK 10.0.26100) — removed; exports come from
`WarbandVR.def` alone. (b) The proxies were written against a misremembered
`IDirect3D9`: real interface has `GetAdapterModeCount` (missing) and
`EnumAdapterModes` returns `HRESULT` (was `UINT`) — fixed, which also fixed
the abstract-class errors. (c) `IID_IDirect3D9(Ex)` live in `dxguid.lib`,
not `d3d9.lib` — both now linked. First real-game smoke test passed:
launcher bypassed (below), CreateDevice OK, 18/18 hooks, clean log at 4K.

### 2026-10-01 (iteration 3) — Depth-stencil substitution added (M1 blocker)
D3D9 requires the bound depth-stencil to be at least as large as the render
target, so pointing RT0 at the double-width stereo target while the game's
backbuffer-sized depth stayed bound would fail `SetRenderTarget` outright.
`StereoRenderer` now creates a matching 2W×H depth buffer and substitutes it
alongside the RT at `BeginScene` (restored at `Present`). A
`SetDepthStencilSurface` hook (index 39) keeps the pairing legal when the
game re-points depth mid-frame. Limitation: stereo RT/DS are
`D3DMULTISAMPLE_NONE`; if Warband runs with MSAA enabled and substitution
fails on-game, revisit (either create MSAA-matching surfaces or force the
game's AA off).

### 2026-10-01 (iteration 3) — Scissor hook index is 57, not 174
The M2-1 task brief said "SetRenderState hook, index 174". 174 is the value
of `D3DRS_SCISSORTESTENABLE`; the vtable index of `SetRenderState` is 57.
Implemented: `hkSetRenderState` tracks scissor-test enable,
`hkSetScissorRect` stores the game's rect, `ApplyEye` re-applies it per eye
with the eye offset.

### 2026-10-01 (iteration 3) — Offscreen passes: per-eye targets, not copy-back
The M2-1 design said "double-width shadow + copy the left half back to the
game surface". That is impossible mid-frame: every copy mechanism
(`StretchRect`, `UpdateSurface`, `UpdateTexture`, `GetRenderTargetData`) is
illegal inside `BeginScene`/`EndScene`, and deferring the copy to frame end
would leave the game's texture stale for same-frame sampling — worse than
today. Implemented instead: per-eye game-sized render-target textures
substituted in `ApplyEye`, plus a `SetTexture` hook (index 65) that learns
surface→texture bindings so samplers get the matching eye copy. The game's
own surface/texture is never touched. Behind `OffscreenPasses=1` (default
0) until verified on-game.

### 2026-10-01 (iteration 3) — Warband boots via a launcher; tests must click Play
`mb_warband.exe` opens a launcher dialog (Win32 class `#32770`, title
"Mount&Blade Warband") which itself calls `Direct3DCreate9` for adapter
enumeration — a log that stops after `Direct3DCreate9` with no
`CreateDevice` is the launcher, not a proxy fault. The game only creates
its device after Play is pressed. `tools/launch_game.ps1` clicks the Play
button (dialog control ID 1029, same technique as the WarbandX launcher) so
automated smoke tests reach the game.

### 2026-10-01 (iteration 4) — Full device wrapper replaces vtable hooks
The M1 crash hunt (`EnableStereo=1` died ~15s after CreateDevice with an
empty log) revealed something worse than a crash: **the vtable hooks never
fired at all**, even in stock runs. A vtable watchdog showed our patches
landing, then the entries returning to the original d3d9 addresses within
~200 ms, with the device object's vtable pointer redirected to a
heap-allocated shadow vtable. Culprit: **Steam's gameoverlayrenderer.dll**
(loaded in every Steam session) — its known technique of swapping the
object's vtable pointer to its own copy and restoring the original memory.
Every in-place vtable patch is silently bypassed; the stock-boot "success"
was vacuous (the mod was inert), and the stereo crash came from whatever
undefined state the partially-patched window produced.
Decision: implement the full delegating device wrapper (`DeviceProxy`, 134
methods implementing `IDirect3DDevice9Ex`, generated from the SDK header) —
the option design doc §2 had reserved for exactly this case. The game holds
our object; no overlay can swap or restore our vtable. The 18 intercepted
methods still route to the same `hkXxx` detours; those now call
`dev->Xxx(...)` on the real device so the overlay remains in the call
chain and keeps working. `VTableHook.h` deleted.
Verified on the real game: stock boot clean; `EnableStereo=1` no longer
crashes — stereo target + depth substitution active, per-eye duplication
running, `Reset` survived, zero ERR/WARN in 60s.
Bonus finding: Warband never calls `SetTransform` (menu/loading) — it is
almost certainly fully shader-driven, so expect `ViewSource=1`; the M0
capture must run in a 3D scene to rank VS registers.

### 2026-10-01 (iteration 5) — The half-screen saga: Z-prepass + EQUAL
Weeks of on-game iteration produced a consistent symptom: **left half =
perfect full frame, right half = skybox only** (plus 3D-tracked markers).
Eliminated in turn, each with instrumentation or a discriminating
experiment: SBS composite (StretchRect verified OK), RT routing (main
scenes verified substituted), the SBSMonoUI mono-composite-quad bug
(user-supplied patch — real bug, but insufficient alone), the default
scissor rect (game enables scissor test but never sets a rect; widened),
the `DuplicationActive` gate (only shadow scenes were mono), RHW
pre-transformed geometry (traced: all battle draws are rhw=0), stale
right-half depth (clears verified full-target), per-eye matrices
(verified correct ±0.049 view-space shift), and a viewport-swap test
(terrain follows the left half regardless of eye).
The two experiments that cracked it: **BothEyesLeft** (both eyes into the
left half → double vision proves eye-1 draws render fine) and **NoZTest**
(depth testing disabled → the world appears in BOTH halves). Final root
cause: **Warband uses a Z-prepass that renders mono** (its scene is never
stereo-substituted), writing depth only into the left half of the
double-width depth buffer; the color pass tests **ZFUNC_EQUAL** — the left
half matches, the right half (depth still at the 1.0 clear value) passes
only far-plane sky. Every right-eye draw failed EQUAL, forever.
This invalidated the entire single-double-width-target architecture.

### 2026-10-01 (iteration 6) — Per-eye game-sized surfaces (M1 done)
Replaced the single 2W×H stereo target + double-width depth with **two
game-sized render targets + two game-sized depth buffers** (one pair per
eye). Every intercepted main-frame draw reroutes into the current eye's
target (RT0/DS saved and restored around the pair). Consequences, all
verified on-game:
- The Z-prepass now runs per eye with per-eye depth: EQUAL passes in both
  halves; battle scenes render with correct occlusion in both eyes.
- The game's own coordinate space (viewports, scissor, RHW vertices,
  offscreen passes) works per eye exactly as mono — no clipping class of
  bugs remains.
- Scenes whose RT0 is not backbuffer-sized (the 256×256 shadow map) pass
  through untouched, so the game's own textures keep their content.
- Draws sampling a "diverted" main-sized surface (its scene was rerouted
  to the eye targets, e.g. the world texture feeding the final composite
  blit) are skipped — the blit would paint an empty surface over the frame.
- Gate fix: scenes where the game never calls SetRenderTarget (the menu,
  default backbuffer) must count as main-frame; RT0 is now queried at
  every BeginScene.
- UI passes (orthographic projection, tiny z-scale) are drawn flat in both
  eyes so text aligns with flat-drawn boxes.
Verified: stock boot clean; splash/menu SBS; battle with terrain, trees,
characters, HUD in both halves with correct depth; `Reset` survived; no
ERR/WARN. Known deferred items: cursor offset (game hit-tests in
3840-wide coordinates while halves are squished — UI remap later),
IPD/WorldScale tuning, menu-only cursor.

## Session log

- **2026-10-01** — Scaffold created: proxy, IDirect3D9 wrapper, 15 device
  vtable hooks, stereo duplication path, SBS composite, OpenXR stub, config,
  logging, deploy script, docs 01–06. Next action: task M0-1 (capture modes).
- **2026-10-01 (iteration 2)** — Implemented all game-independent work:
  Capture (M0-1), CameraScan (M0-2), SBSMonoUI heuristic, viewport tracking,
  asymmetric projection, per-eye Clear offsets, perf instrumentation,
  D3D9ExProxy, head-tracking pipeline (ApplyHeadDelta, OneEuro, menu lock,
  recenter key), exe version tagging. New doc 00_Runbook.md (human's evening
  plan). Prompts 03 rewritten. Next action: human runs Phase 1–3 of the
  runbook; first agent coding task is M1-1 bring-up.
- **2026-10-01 (iteration 3)** — Fixed scaffold bit-rot (dllexport linkage,
  proxy interface, dxguid) so the project builds on stock VS2022. Fixed a
  latent M1 blocker: double-width stereo depth-stencil substitution
  (+ `SetDepthStencilSurface` hook). M2-1 code landed: per-eye scissor
  (new hooks `SetRenderState` 57, `SetScissorRect` tracking) and offscreen
  shadow passes (new hooks `SetRenderTarget` routing, `SetTexture` 65;
  behind `OffscreenPasses=0`). 15 → 18 hooks. New `tools/launch_game.ps1`
  clicks through the Warband launcher for automated runs. First real-game
  smoke test: stock boot with proxy, CreateDevice OK, 18/18 hooks, clean
  log — invariant 1 verified on the real game. Next action: human M0
  capture protocol (runbook Phase 2), then M1 stereo bring-up.
- **2026-10-01 (iteration 4)** — Root-caused the `EnableStereo=1` startup
  crash: vtable hooks never fired because Steam's gameoverlayrenderer swaps
  the device object's vtable pointer to a heap shadow (watchdog + module
  dump proved it). Replaced vtable hooking with a full `DeviceProxy`
  wrapper (134 methods; the 18 intercepted ones route to the same hkXxx
  detours, which now chain through the real device's live vtable so the
  overlay keeps working). Verified on the real game: stock boot clean;
  stereo boot clean — substitution active, Reset survived, zero ERR/WARN.
  Also learned: Warband never calls `SetTransform` (menu/loading) → likely
  fully shader-driven (`ViewSource=1`); M0 capture needs a 3D scene. Next
  action: human runs CaptureMode=2 in a battle to identify the VS view
  register, then visual SBS check.
- **2026-10-01 (iterations 5–6)** — The half-screen saga. Weeks of
  experiments (BMP surface dumps, CaptureMode=4 frame traces, scissor
  probes, viewport swaps) chased the "world left, sky right" symptom
  through a dozen eliminated theories until **NoZTest** proved the world
  renders in both halves with depth off: the game's mono **Z-prepass**
  only fed the left half's depth, and the **ZFUNC_EQUAL** color pass
  killed every right-eye draw. Replaced the double-width stereo target
  with **per-eye game-sized render targets + depth buffers**; draws
  reroute per eye, offscreen scenes pass through, diverted-texture blits
  are skipped, ortho-UI passes draw flat. **M1 stereo works**: battle and
  menu render side-by-side with correct occlusion in both eyes. Also
  integrated a user-supplied patch (mono composite-quad coverage,
  pre-substitution viewport capture) along the way. Next: IPD/WorldScale
  tuning, cursor/UI remap, then M2 polish.

### 2026-10-02 — Fix: mono-UI heuristic must still cover both eyes
First on-game test showed the classic half-screen symptom: full frame in the
left half, sky-only in the right. Root cause: `SBSMonoUI` drew <=64-triangle
batches ONCE into the current viewport (the left half), and Warband's final
fullscreen composite quad is exactly such a draw - so the complete frame only
ever reached the left half of the stereo target. Fixed: the UI path now draws
per eye WITHOUT the view shift (`ApplyEyeMono`), and a new
`DuplicationActive()` gate stops per-eye duplication entirely while the game
renders to its own offscreen surfaces (where the right eye's viewport would
be clipped against the game-sized target). Also fixed: the game viewport is
now captured BEFORE the RT substitution (D3D9 resets the viewport to the full
new target on SetRenderTarget, which poisoned the post-substitution
GetViewport fallback).

## Session log

- **2026-10-02** — First on-game run: stock boot OK, stereo active, but
  half-screen output. Diagnosed from photos + code review; shipped
  half-screen fix (mono-UI per-eye coverage, DuplicationActive gate,
  pre-substitution viewport capture). Next: rerun with the fix; keep
  `[Debug] DumpFrames=5` ready if anything is still off.

### 2026-10-02 — Readback-based register handling; open encounter-dialog bug
Captured view matrices turned out to be unusable for UI: Warband uploads
per-element transforms as 3x4 `SetVertexShaderConstantF(count=3)` writes and
many UI passes never re-upload before drawing, so any captured matrix stamps
stale state on the wrong element (misaligned/reordered text). Replaced with a
**per-draw-pair readback**: `ApplyEye` reads the game's actual c0 via
`GetVertexShaderConstantF` once per eye pair, skips the upload entirely for
ortho-UI matrices (`|_33|<0.5, _44==1`), otherwise uploads
`EyeView(readback)`; `RestoreGameState` puts the game's own value back. Main
menu, most windows, battles, and the world map now render correctly in both
eyes.

**Open bug:** the encounter ("You have encountered...") dialog lays its
options out wrong under stereo (spread out / wrong order vs stock's tight
bottom cluster; highlight mismatch follows from the known cursor offset).
Confirmed NOT present with `EnableStereo=0`, so passthrough and the capture
hooks are clean. Reasoning so far: `EyeView` only injects a world-space X
translation, so a *vertical* layout change cannot come from the c0 upload at
all - suspects are the diverted-surface skip, `m_gameVp` tracking, or the
game baking dialog layout from device state that differs mid-frame. Diag
build in the tree logs each pair's actual register + live viewport vs
`m_gameVp` (`RB cap=...` lines); next step is key-triggered logging/dump so
the capture lands exactly on the dialog.

### 2026-10-02 (iteration M3–M5) — Game-thread-coupled OpenXR submission
The frame loop is driven synchronously from `SubmitFrame` (called by
`StereoRenderer::OnPresent`): xrWaitFrame paces the game to the HMD refresh.
Simplest correct ordering, no cross-thread surface ownership issues. Revisit
with an async submission thread only if frame pacing stutters on hardware.

### 2026-10-02 — Per-eye swapchains sized to eye surfaces (not the runtime's
recommended view size). The eye surfaces are the game's backbuffer size;
the projection layer's per-eye FOV/pose come from xrLocateViews. If the
runtime upscales noticeably, add a scale factor config later.

### 2026-10-02 — D3D9Ex steering is opt-in (`SharedSurface=1`)
Steering `Direct3DCreate9` → Ex changes device semantics slightly (the game
gets an Ex device). Verified-safe pattern used by other wrappers, but the
slow CPU-copy path is the default precisely so bring-up starts from the most
compatible configuration.

### 2026-10-02 — Motion injection via SendInput, not engine memory writes
M5 uses mouse-direction + click injection (Warband's documented attack-input
behavior). No engine memory patching required; works regardless of build
offsets. Engine-input-struct injection remains a possible M5+ refinement if
SendInput proves lossy (direction quantization).

## Session log

- **2026-10-02 (M3–M5 autonomous pass)** — Implemented the complete OpenXR
  bridge (session, swapchains, frame loop, slow + fast submit paths, pose
  sampling with OneEuro + recenter), the M4 wiring end of it, and the full
  MotionInput module (action bindings for 5 controller profiles, kb/mouse
  emulation with remappable keys, swing classifier + attack/block injection,
  latency instrumentation). Added `Release-OpenXR|Win32` build config,
  `[Output] SharedSurface`, `[Tracking] Invert*`, `[Input]`/`[InputMap]`
  config sections. Everything off by default; stock boot invariant intact.
  Remaining work is headset bring-up and feel tuning — needs the hardware.

### 2026-10-02 (review pass) — Held-attack steering (MotionSteer)
Warband natively steers a HELD attack from mouse movement. So instead of
injecting an instant click per swing, attacks now inject direction + LMB
down, keep the button held, and stream per-frame mouse deltas from live
hand velocity (rotated into head-yaw space) until the hand slows or
SteerMaxHoldMs elapses. This is the closest approximation to "the weapon
follows the hand" available without engine animation internals (closed
source; M0-C unfinished). True per-frame weapon-pose control would require
reverse engineering Warband's animation/agent state - out of scope; the
held-steer path rides a documented, robust engine behavior.

### 2026-10-02 (review pass) — MotionInput subaction model fix
The first draft created per-hand actions WITHOUT subaction paths but then
created action spaces and suggested bindings WITH /user/hand/* paths -
illegal per the OpenXR spec (XR_ERROR_PATH_INVALID). Reworked to the
standard model: one action per control with both hand subactions; reads and
spaces specify the subaction path.

### 2026-10-02 (review pass) — Monitor mirror fallback
With OutputMode=1 and no running XR session (no runtime, headset off,
session not begun), the backbuffer composite still runs, so the desktop
window stays live and the game remains playable flat. While the session
runs, the desktop window is intentionally not composited (perf).

- **2026-10-02 (review/quality pass)** — Full self-review + cross-compile
  verification (clang via zig, x86-windows-gnu, both plain and
  WBVR_WITH_OPENXR flavors; OpenXR headers via a spec-faithful harness
  subset). One real code bug found by the compiler
  (`xrGetActionStateVector2F` capitalization) plus harness-only noise.
  Fixed: hr scoping compile error in CreateResources shared
  path; eye-DS leak on shared->plain fallback; stub SubmitFrame log spam;
  unchecked swapchain acquire/wait; per-frame GetDesc in the slow copy;
  hardcoded blend mode (now enumerated, OPAQUE preferred); upward-dominant
  swings no longer inject overhead; attacks are now held + steered; instant-
  click injection replaced by deferred release. Added docs/07 (AI
  onboarding) and docs/08 (controls & setup).

- **2026-10-02 (final hardening pass)** — Preemptive fixes: bridge Shutdown
  now runs on DLL_PROCESS_DETACH (previously: leaked XR session and possible
  stuck emulated keys on exit); init failure retries every 5 s instead of
  permanently giving up (runtime may start after the game); eye-surface size
  changes (resolution switch / Reset) rebuild swapchains instead of risking
  OOB copies; projection layer suppressed when pose locate fails; MotionInput
  retries Init while the session runs; per-eye Flush on the cross-API fast
  path. Everything not code-fixable is catalogued with testing paths in
  docs/09 (game, mod, Steam/streaming, Steam Frame Phase 2 feasibility).
  Re-cross-compiled clean in both flavors after all changes.

- **2026-10-02 (VR combat gestures)** — Tier-3 weapon handling: grab-zone
  gestures (hip L/R, over-shoulder, quiver) and two-handed stance emit
  synthetic keys (F1–F5 defaults) that a companion module-system mod turns
  into real equipment changes - the one clean channel into the closed
  engine (DLL senses, game acts). Bow draw = trigger hold/release. Zone
  grabs suppress grip-block for that press. Design + companion script
  skeleton in docs/10. Bridge now also exposes absolute head position for
  body-relative zones. Compiled clean both flavors.

- **2026-10-03 (h2-mod-vr pattern adoption)** — Reviewed Aeka0/h2-mod-vr
  (docs/11). Their tier-5 fidelity comes from a reconstructed engine client,
  which Warband lacks - not transferable. Adopted what IS transferable:
  snap turning (reference-pose rotation, pivot re-anchored to head XZ),
  holster-on-grip-release in a zone (F6; press=grab / release=stow grammar),
  spectator mirror (MirrorOnDesktop). Their setup-order and per-weapon-
  procedure guidance folded into docs/08 and docs/10. Compiled clean both
  flavors.

- **2026-10-06 (diablo2-vr-port review)** — Analysis in docs/12. Their
  "VSync off - important" lesson adopted as code: ForceNoVsync=1 (default)
  makes the proxy force D3DPRESENT_INTERVAL_IMMEDIATE in CreateDevice and
  Reset when OutputMode=1, so driver vsync cannot fight xrWaitFrame pacing.
  Their version-resilient scanning validates CameraScan's direction; their
  per-weapon calibration reality is folded into docs/10's tuning path;
  their BodyWalk/FlatVR commercial dependency chain is a cautionary
  contrast for Phase 2 (ours has no such dependency).

- **2026-10-07 (streaming-scope reassessment + cinema mode)** — If the
  project stays pure-PCVR-streaming, the D2R-VR commercial chain becomes a
  documented plan-B and ViGEmBus analog input a documented upgrade path
  (docs/12 addendum). Implemented their diorama idea natively instead:
  [Output] CinemaMode=1 submits a fixed quad layer 2.2m ahead of the
  recenter reference. Full tree recompiled clean, both flavors.

- **2026-10-07 (sanity audit)** — Full recompile with -Wall -Wextra (clean,
  both flavors) plus a scripted consistency battery: config field/parse/usage
  cross-check, ini<->parser key parity, bridge decl/def parity in both build
  branches. Fixes: dead config fields removed (swapEyeViewports, bothEyesLeft,
  vsMatrixIsViewProj - silently-dead knobs are traps; VSMatrixIsViewProj may
  return WITH the shader bring-up if the register proves to hold view*proj);
  dead m_loggedEyes diag field removed; DumpFrames/NoZTest added to the ini
  for discoverability; REAL LOGIC BUG fixed - LMB had two owners (bow-draw
  trigger hold and the swing classifier), so a swing during a held draw
  released the bow early: swings are now suppressed while the trigger holds
  LMB and vice versa. Reviewed the draw-duplication gates
  (DivertedTextureBound / CurrentRTIsMain / SBSMonoUI) - coherent with the
  per-eye surface architecture, no leftover half-screen-fix scaffolding.
