# 02 — Milestones & goals

North-star goal: **play Warband's single-player campaign in VR on the Steam
Frame, streamed from the user's PC, with tracked head and motion-controlled
melee** — with every intermediate milestone independently playable.

Milestones are sequential; each has a deliverable and acceptance criteria.
`docs/03_Agent_Prompts.md` is retained as the historical task-brief record;
the checkboxes below are the current status source of truth.

Status legend: `[ ]` todo · `[~]` in progress · `[x]` done

---

## M0 — Reconnaissance: how Warband sees the camera
**Effort:** one evening (tooling is built; only the game runs remain) · **Risk: gates everything downstream**

Tooling already implemented in the scaffold: `Capture` (view-source detector
+ shader-register ranker, `CaptureMode=1/2`) and `CameraScan` (memory
candidate finder). Remaining work is human-side: run the capture protocol and
record answers. Deliverable: a written answer (in `docs/04` §Results) to
three questions:
1. Does Warband set its view matrix via `SetTransform(D3DTS_VIEW)`,
   vertex-shader constants, or both? (→ sets `ViewSource` in the ini)
2. Which VS register holds view / view·proj, if shaders are used?
3. Where in memory does the engine store its camera struct (position,
   orientation, FOV)? (needed for M4's true camera coupling)

- [ ] Capture protocol run (mode 1, then 2 if needed) — logs interpreted
- [ ] `ViewSource` + register values recorded in `WarbandVR.ini`
- [ ] (Optional now, needed by M4) CameraScan converged; Cheat Engine confirm
- [ ] Camera struct offset table (per game build) documented in docs/04
- [ ] RenderDoc/PIX capture cross-check of one battle frame

## M1 — Stereo on a monitor (no headset)
**Effort:** 1–2 weeks · core duplication path is implemented; this is bring-up + bugfix

Deliverable: with `EnableStereo=1, OutputMode=0`, the game renders
side-by-side stereo to the normal monitor.

- [x] Proxy loads, game boots stock with `EnableStereo=0` (invariant) — verified on the real game: CreateDevice OK, DeviceProxy wraps the device, clean log; `EnableStereo=1` also boots clean
- [x] SBS image in first-person on foot: two views, correct parallax (per-eye game-sized targets; verified with correct occlusion)
- [x] SBS image in a field battle with 100+ agents
- [x] Menu/UI text aligns with its buttons (per-draw register readback replaces captured matrices)
- [x] Encounter dialog option layout matches stock (sub-viewport panels re-apply the game viewport after the per-eye RT swap; tooltip text fixed: depth clears now reach the per-eye depth buffers, and axis-aligned UI cameras are never eye-shifted)
- [ ] IPD/WorldScale feel right (cross-eye check or record + review)
- [x] No crash on window resize / alt-tab / `Reset`

## M2 — Correctness & performance hardening
**Effort:** 1–3 weeks · asymmetric projection, UI mono-draw, per-eye clears,
and perf instrumentation are already implemented; remaining work is offscreen
passes, scissor, and verification

- [x] ~~Offscreen render passes (shadow maps, reflections) handled per eye~~ implemented behind `OffscreenPasses=1` (per-eye shadow targets + sampler swap; needs on-game verification)
- [x] ~~Scissor rects offset per eye~~ implemented (`hkSetRenderState` + `hkSetScissorRect`; needs on-game verification)
- [x] ~~Asymmetric (off-center) projection~~ implemented (`AsymmetricProjection=1`; needs on-game verification)
- [x] ~~2D UI passes detected and drawn *mono*~~ implemented (`SBSMonoUI` heuristic; tune `UIMaxTriangles` on-game)
- [ ] Overhead verified: `[PERF]` log shows <4 ms/frame added at 1080p
- [ ] 150-agent battle holds 72 fps on target PC

## M3 — OpenXR output to the Steam Frame
**Status: code complete and headless/compile-gated; pending headset bring-up**

Implemented: full OpenXRBridge (instance/session/LOCAL space/per-eye
swapchains/game-thread-coupled frame loop), slow CPU-copy submit path,
D3D9Ex shared-surface fast path (`[Output] SharedSurface=1`, incl. Ex device
steering in dllmain), `Release-OpenXR|Win32` build configuration. What
remains is on-hardware bring-up.

- [~] `Release-OpenXR|Win32` builds against the OpenXR SDK: GitHub Actions job configured with a pinned x86 SDK loader build; remote CI execution remains pending
- [ ] `OutputMode=1` shows the game in the headset via SteamVR → Steam Link
- [ ] Correct eye assignment; stable 30-minute session
- [ ] Fast path (`SharedSurface=1`) after slow path works
- [ ] (Post-M3) async submission thread — the synchronous loop locks the game
      to HMD refresh; fine for bring-up, revisit if frame pacing stutters

## M4 — Head tracking
**Status: code complete 2026-10-02, pending on-head verification**

Implemented: `xrLocateViews` pose sampling at predicted display time,
recenter reference + hotkey (wired to the real bridge now), OneEuro
smoothing, menu lock, pose-delta composition onto the captured view, and
`[Tracking] Invert*` sign-convention tunables so handedness mismatches are
fixable from the ini without a rebuild. Remaining:
- [ ] `HeadTracking=1` verified: 1:1 look-around, no judder
- [ ] Sign conventions confirmed/flipped via the Invert* keys
- [ ] (Stretch, needs M0-C camera struct) engine-camera write path
- [ ] Comfort pass: horseback, sieges, menus

## M5 — Motion-controlled melee
**Status: code complete and fake-OpenXR regression-tested; pending playtesting & tuning**

Implemented (`src/MotionInput.cpp`): OpenXR action set + suggested bindings
for Touch/Index/Vive/WMR/simple profiles; controller→kb/mouse emulation with
`[InputMap]` remappable keys; swing classifier (ring buffer, peak-velocity
dominant-axis classification into left/right/overhead/thrust) with
mouse-direction + held-attack injection; grip-hold block; MotionSteer held
swing steering with per-poll delta clamp; snap turn; grab-zone virtual-slot
gestures with post-grab swing refractory; bow-trigger LMB ownership;
stick-click-hold recenter chord; runtime-timestamped hold timing for block,
both-grips, and recenter (OpenXR `lastChangeTime`, with analog fallback);
controller presets (`[Preset.*]` overrides);
shutdown release of held inputs; swing→inject latency instrumentation. The
companion module implements gesture equipment switching, optional melee
direction assist, and the either-order crossbow reload ritual.

Remaining:
- [ ] Emulation mode regression-tested on hardware (menus, map, movement)
- [ ] Classifier accuracy on the 50-swing test set (<10% misclassification)
- [ ] Directional block from controller orientation (currently grip hold only)
- [ ] MotionSteer tuning on real swings (SteerScale, SteerMaxHoldMs)
- [ ] Shield, archery (two-hand aim), couched-lance handling
- [ ] Companion module verified in-game on a single-player module
- [ ] 20-swing trial vs training dummies: >=18 intended-direction hits;
      swing→attack latency <50 ms

## M6 — Polish & release
- [x] Publish-oriented README, FAQ, controls guide, and Native package notes
- [x] Repeatable package script and PCVR/Native source-parity gate
- [x] GitHub Actions workflow for headless regression plus MSVC Win32 stub/OpenXR builds (configured; remote run pending)
- [x] Comfort options: world-scale presets, vignette, snap turn, cinema mode
- [ ] UI projected on a curved plane at comfortable distance
- [ ] Per-scene profiles (menu/battle/map/tournament) with auto-switching
- [ ] Installer + uninstaller, README for users, Nexus/Workshop release
- [ ] Performance tuning guide (battle size vs fps)
- [ ] Full hardware checklist completed (`docs/manual_test_checklist.md`)

---

## Effort rollup

| Milestone | Agent-tractable? | Human needed for |
|---|---|---|
| M0 | Partially (agent writes tools; human runs PIX/CE on the real game) | captures, verifying matrix identity |
| M1 | Yes, mostly | cross-eye/VR judgement of scale |
| M2 | Yes | playtesting |
| M3 | Yes (hello_xr is a known pattern) | headset testing |
| M4 | Yes | comfort judgement |
| M5 | Partially — classification code yes, feel-tuning no | extensive playtesting |
| M6 | Partially | release QA |

**Suggested division of labor:** the autonomous agent owns code, builds, and
the test checklist; the human owns runtime verification on the real game and
all "does it *feel* right" calls. Every task brief in `03` states which side
of that line its acceptance criteria fall on.
