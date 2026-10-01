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
