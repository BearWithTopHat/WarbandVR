# 02 — Milestones & goals

North-star goal: **play Warband's single-player campaign in VR on the Steam
Frame, streamed from the user's PC, with tracked head and motion-controlled
melee** — with every intermediate milestone independently playable.

Milestones are sequential; each has a deliverable, acceptance criteria, and
the task briefs that implement it (in `docs/03_Agent_Prompts.md`).

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

- [x] Proxy loads, game boots stock with `EnableStereo=0` (invariant) — verified on the real game: CreateDevice OK, 18/18 hooks, clean log
- [ ] SBS image in first-person on foot: two views, correct parallax
- [ ] SBS image in a field battle with 100+ agents
- [ ] IPD/WorldScale feel right (cross-eye check or record + review)
- [ ] No crash on window resize / alt-tab / `Reset`

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
**Effort:** 2–4 weeks · needs OpenXR SDK

Deliverable: `OutputMode=1` renders into an OpenXR session; game is visible
in the headset via SteamVR → Steam Link, still with mouse/keyboard or the
existing controller scheme.

- [ ] `WBVR_WITH_OPENXR` build configuration works
- [ ] OpenXRBridge implemented (instance/session/swapchains/frame loop)
- [ ] Stereo surface reaches swapchain images (slow copy path first)
- [ ] Streamed to Steam Frame; stable 30-minute session
- [ ] (Stretch) D3D9Ex shared-surface zero-copy path — **the Ex wrapper is
      already implemented**, so this is "use it in CreateResources", not a
      new subsystem

## M4 — Head tracking
**Effort:** 1–2 weeks · pipeline is implemented; needs the M3 pose source
plus on-head tuning

Already implemented: pose-delta composition onto the captured view
(`ApplyHeadDelta`), menu lock, recenter hotkey plumbing, OneEuro filter.
Remaining:
- [ ] `OpenXRBridge::GetHeadPoseDelta` implemented (xrLocateViews at
      predicted display time, relative to recenter reference)
- [ ] Pose sampled once per frame (in OnBeginScene), filters wired
- [ ] `HeadTracking=1` verified: 1:1 look-around, no judder at 72 fps
- [ ] (Stretch, needs M0-C camera struct) engine-camera write path
- [ ] Comfort pass: horseback, sieges, menus

## M5 — Motion-controlled melee
**Effort:** 1–3 months · the design-heavy milestone; see design doc §6

- [ ] Controllers emulate the existing working controller scheme (menus, map,
      movement) — regression-tested
- [ ] Swing capture: velocity/direction classification into Warband's 4
      attack directions
- [ ] Directional block from controller orientation
- [ ] Shield, archery (two-hand aim), and couched-lance handling
- [ ] Input latency measured: swing → attack start <50 ms

## M6 — Polish & release
- [ ] UI projected on a curved plane at comfortable distance
- [ ] Per-scene profiles (menu/battle/map/tournament) with auto-switching
- [ ] Installer + uninstaller, README for users, Nexus/Workshop release
- [ ] Performance tuning guide (battle size vs fps)

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
