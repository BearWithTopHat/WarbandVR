# 06 — Risks, decisions, and session log

Append-only. Decisions are dated; if a decision is reversed, add a new entry
— don't edit history.

## Risk register (living)

| # | Risk | Likelihood | Impact | Mitigation / fallback | Status |
|---|---|---|---|---|---|
| R1 | Warband delivers view matrices only via shader constants, in a layout that varies per shader | Medium | M1 blocked on path 0 | ViewSource=1 path built into scaffold; M0 capture tooling ranks candidate registers | Open |
| R2 | Engine re-points RT0 mid-frame (shadows/reflections) breaking per-eye duplication | Medium | Visual glitches | M2-1: shadow-surface chain per eye | Open |
| R3 | CPU-bound battles + doubled draw calls miss 72 fps on host PC | Medium | Comfort | Battle-size guidance; overhead budget <4 ms enforced in M2 | Open |
| R4 | Engine camera struct never found reliably (M0-C fails) | Low-Med | M4 degraded | Stay on matrix-delta tracking (VorpX-style) permanently | Open |
| R5 | D3D9→OpenXR copy path too slow | Low | Latency | D3D9Ex shared-surface path (planned in M3-1); SBS→Steam Link fallback | Open |
| R6 | Game update breaks offsets/hook assumptions | Low | Sudden crash | Version-gate writes (`[Camera] GameBuild`); EnableStereo=0 always safe | Open |
| R7 | Motion melee injection misread by engine (attack direction stochastic) | Medium | M5 quality | M0-C input-state RE; extensive tunables; keep flat scheme as config option | Open |
| R8 | Scope creep into multiplayer/native-Frame | — | Project death | Explicit non-goals in design doc §9; AGENTS.md invariant 6 | Controlled |

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
