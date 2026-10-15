# 15 — Lessons from BannerlordVR

Source: <https://github.com/NotLooky/BannerlordVR> (v0.2.0-alpha.1, reviewed
2026-10). Native PCVR for Mount & Blade II: Bannerlord with full motion
controls, built inside Bannerlord's managed mod API (Harmony), MIT-licensed,
single-player only.

## Why the architectures don't share code

Bannerlord is 64-bit with an official C# module API: BannerlordVR patches the
engine from inside and the engine itself renders two real eye views into an
OpenXR swapchain. Warband is 32-bit D3D9 with no rendering hooks, which is why
WarbandVR is a d3d9 proxy duplicating draws. **No code transfers.** The value
is interaction design, calibration practice, and honest prior art on what
players accept.

## What it validates in our design

- **Body holster map.** Left hip = one-hand weapon, behind shoulder =
  bow/two-hander, behind either shoulder with bow held = arrow/bolt. Nearly
  the same zone layout we ship for F1–F5; independent convergence is a good
  sign.
- **Grip semantics.** Hold grip to keep holding; release at the holster to
  stow; release away to drop. Same model as our grab/holster gestures.
- **Crossbow as a multi-condition ritual.** Theirs: seat bolt + span string,
  in either order, both required, early release returns the string. Our
  companion ritual is now either-order too (see below).
- **Single-player only**, with multiplayer explicitly disabled.
- **Alpha honesty.** "Expect glitches, unfinished UI, and crashes" — the same
  compile-vs-hardware labeling standard we enforce.

## What we stole (implemented)

1. **Stick-click-hold recenter.** No keyboard exists in a headset; our
   recenter was F8-only. Now: hold the left stick press for
   `[Input] RecenterHoldSeconds` (default 3.0, 0 = off) with the right stick
   centered; right-stick motion cancels that press. Implemented in
   MotionInput as a system-level layer (independent of emulation/melee/
   gesture toggles), with a new `stickclick` action bound per interaction
   profile (unbound — inert — on profiles without a stick click, e.g. Vive
   wand). Headless-tested.
2. **Controller presets.** They ship Generic/PSVR2/Quest hand presets,
   manually selected because runtimes report different controllers under the
   same profile. We implement the mechanism as `[Input] ControllerPreset` +
   `[Preset.<name>]` ini sections overriding SwingThreshold / SteerScale /
   BlockHoldTime only. **Our preset values intentionally ship identical to
   the base defaults** — inventing per-controller numbers without hardware
   would be fabrication; the sections exist so calibration on real hardware
   is an ini edit, not a rebuild. Config-load integration-tested.
3. **Either-order crossbow ritual.** The companion state machine was a strict
   F4-then-F5 sequence; a dropped or reordered step could strand it. It now
   tracks `bolt seated` and `string drawn` as independent flags — either
   order, both required — with a completion trigger and the existing
   weapon-change watchdog. Statically checked.

## What we stole (documented, not implemented)

4. **Depth reprojection as a perf fallback.** Their cheapest stereo mode
   renders one eye and rebuilds the other from depth. Warband's Z-prepass
   gives us depth for free; a composite-time reproject of one eye to the
   other could halve draw cost in large battles if draw duplication misses
   the frame budget. Large feature; tracked as risk row R9 in docs/06.
5. **AFR.** Alternate-frame eye rendering leans on runtime reprojection
   support that does not map to our duplication model. Not applicable.
6. **Physical reins / ladder rungs / siege cranks / grip-sliding along a
   shaft / weapon-weight lag.** These need engine-side physics or animation
   hooks that Warband's closed 2010 engine does not expose to a d3d9 proxy.
   Not feasible via OS-level input injection; listed here so nobody re-asks.
7. **Throw-vs-drop release-velocity threshold.** Bannerlord distinguishes a
   damaging throw from a drop by hand speed at release. Warband's throwing
   is a native LMB-release mechanic, so our existing swing → attack path
   already covers it; nothing to add.

## Adopted on the architecture pass

After reading their source layout and BUILDING.md (not just the README),
three operational designs were judged strictly better than ours and adopted:

1. **Crash logging as `module+RVA`.** Their native DLL carries a vectored
   exception handler that logs access violations with module offsets before
   the engine's handler runs — the only way to get useful data out of a
   no-PDB engine. `src/Logger.cpp` now installs the same: hard faults (AV,
   illegal/privileged instruction, div-by-zero, stack overflow) are logged as
   `CRASH code=0x… at module+0x…` and the search ALWAYS continues (Steam/game
   handlers still fire). File I/O from crash context is technically unsound
   and deliberately accepted, same tradeoff they made. Headless-tested via a
   fabricated exception record.
2. **Previous-log rotation.** Their bug-report flow relies on `*.prev.log`
   because logs rewrite each launch. `Log::Init` now rotates
   `WarbandVR_log.txt` to `WarbandVR_log.prev.txt` before truncating, so a
   crash-then-relaunch no longer destroys the evidence.
3. **In-package provenance.** Their release ZIP carries `BUILD-INFO.json`
   (source commit + hashes). `tools/package_release.py` now writes one with
   package identity, UTC build time, a sha256 manifest of every shipped file,
   and the baseline-patch hash. It deliberately does NOT record test results
   — the packer did not run them; the gate output stays in the completion
   report.
4. **Runtime-timestamped hold timing.** OpenXR action states carry
   `lastChangeTime` — the runtime's own timestamp of the physical edge.
   Block hold, the both-grips stance, and the recenter chord now measure
   holds from that timestamp (via a new boolean `squeezeclick` action bound
   where the profile has a click path) instead of from the first poll that
   observed the edge, so a frame-rate dip can no longer delay engagement.
   Profiles with only an analog squeeze path (e.g. Oculus Touch) keep the
   previous threshold + tick behavior as an automatic fallback. All three
   paths — runtime-timed, between-polls press, and analog fallback — are
   headless-tested at exact millisecond boundaries.

Explicitly rejected after comparison:

- **Menu stand-down** (suppress combat gestures while UI owns input) —
  architecturally clean, but they learn menu state from engine callbacks.
  Warband's closed engine exposes no reliable menu signal to a proxy, and the
  static-view heuristic is far too coarse to gate combat on (a siege standoff
  would eat attacks). Not implementable honestly here.
- **In-headset settings panel** — better UX than an ini, but it is a full
  D3D9 UI with controller pointing. Future idea, not blind-built.
- **D-pad chord, multithread-protected D3D11 context, SceneView lifecycle
  rules, BGRA swapchain matching** — all specific to Bannerlord's managed
  engine or D3D11 threading; our game-thread-coupled D3D9 design does not
  share those failure modes.
- What we already matched: fail-safe-to-flat rule (= our stock-boot
  invariant), single VR-active gate (= `DecideDuplication` routing), one
  coordinate-conversion home (= MatPure/MathUtil), dead-ends documentation
  (= docs/06 log), deploy allow-list (= packaging exclusions), math test
  console (= the full headless suite, which is larger).

## Runtime matrix (community data)

Their tested combinations: Quest 3 + Virtual Desktop/VDXR, Quest 3 + SteamVR,
PSVR2 + SteamVR — with **Meta's own OpenXR runtime explicitly unsupported**.
Independent evidence that SteamVR (or VDXR) is the safe bring-up path; the
manual checklist's "SteamVR selected as the active OpenXR runtime" step stays
normative, and Meta-runtime issues should be answered with "use SteamVR or
VDXR", not code changes.
