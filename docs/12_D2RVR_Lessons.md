# 12 — Lessons from diablo2-vr-port (D2R VR)

Reviewed: https://github.com/natpoh/diablo2-vr-port (MIT) — Diablo II:
Resurrected VR via a D2RLoader plugin (`d2rl-vrcam.dll`), a ReShade depth-fog
effect, and a BodyWalk/FlatVR bridge. Four camera views (isometric stereo,
third person, floor/MR diorama, first person with controller-driven arms).

## Architectural contrast (and why ours is the more durable design)

Their stack is a *chain of three external runtimes*: D2RLoader (plugin host),
ReShade (post-processing platform), and BodyWalk/FlatVR (**commercial, closed,
Windows-only Steam products**). Ours is a single self-contained GPL DLL. Two
concrete consequences:

1. **Phase 2 (Steam Frame native)**: their stack cannot survive Proton/ARM —
   BodyWalk and FlatVR have no Linux/ARM story. Our DLL has no such
   dependency; docs/09 §D stands.
2. **Fragility**: every chain link is a separate product that can break,
   change pricing, or desync versions. Our dependency surface is the OpenXR
   loader alone.

What they gain in exchange: FlatVR gives them headset display and
controller→gamepad emulation "for free" — we implemented that ourselves
(OpenXRBridge, MotionInput), which is exactly why M3/M5 were the big lifts.

## Lessons adopted in code

| Their practice | Our adoption |
|---|---|
| **"VSync off — important"**: they draw eyes in alternating frames, so vsync halves per-eye throughput. We draw both eyes per frame, but vsync still fights xrWaitFrame pacing. | `ForceNoVsync=1` (default): proxy forces `D3DPRESENT_INTERVAL_IMMEDIATE` in CreateDevice and Reset when OutputMode=1. **Implemented this round.** |
| **Version-resilient code location**: their mod "finds the game code it needs by itself", with a Status tab + Scan button, instead of hardcoded offsets that die on every patch. | Validates the CameraScan (M0-C) design direction. Our equivalent of their Status tab: the log diagnostics + CameraScan ini. Reinforced as the standard for any future engine-memory work (docs/04). |
| **Per-weapon calibration UI** ("Weapon Adjust") — their weapons sit wrong in hands until tuned one by one. | Expect the same for swing/steer feel per weapon class; per-weapon-class tunables are the docs/10 future path. No global "perfect" setting exists — plan for a tuning table, not a fix. |
| **Offline-only warning** (injection + online = ban risk). | Matches our single-player invariant; already in AGENTS.md/docs. |

## Lessons documented (not code)

- **Temporal-effect incompatibility** (their no-DLSS/no-TAA rule comes from
  alternating-eye frames). Warband (2010, DX9) has no TAA/DLSS, but any
  ReShade/SweetFX the user adds with temporal effects would break stereo the
  same way — noted in docs/09.
- **Square/per-eye aspect**: they ask users to run a square window for HMD
  aspect. Our swapchains are game-resolution 16:9; the cleaner fix is
  per-eye aspect-aware swapchain sizing (docs/09 B9) — same problem, better
  solution path.
- **Embodied UI** (wrist health/mana, belt inventory, map orb in hand) —
  Warband's UI is engine-drawn; the closest tier-4 approximation is
  companion-mod HUD elements near the view edge. Aspirational; documented in
  docs/10's future section.
- **Alternating-eye rendering** was *their* constraint (FlatVR's frame
  grabber); our per-frame per-eye duplication is strictly better where the
  API boundary allows interception — worth remembering if anyone proposes
  "simplifying" to frame alternation: don't.

## Addendum — if we stay pure-PCVR streaming (no Phase 2)

Dropping the native-Frame goal removes the "their stack can't run there"
objection; everything in the D2R chain becomes *available* on the Windows
host. Reassessed applicability:

1. **FlatVR/BodyWalk as a plan-B fallback.** If our OpenXR bring-up fails
   hard on the target hardware, BodyWalk+FlatVR can provide headset display
   (SBS frame-grab of our OutputMode=0 composite) and gesture mapping as a
   stopgap. Trade-offs: commercial, closed, version-fragile, higher latency
   (frame-grabber vs. our direct swapchain submission — their
   alternating-eye constraint is proof). Keep as documented contingency,
   not the primary path.
2. **ViGEmBus virtual gamepad (genuine upgrade).** Warband natively supports
   Xbox controllers with ANALOG movement; our SendInput WASD is digital
   (8-way). A ViGEm feed from MotionInput would give true analog locomotion
   and native gamepad aim assist curves. Cost: kernel driver install +
   ViGEm SDK dependency. Optional enhancement; eval after M5 playtests.
3. **ReShade becomes usable — but collides on the d3d9.dll filename.**
   Resolution: chain-loading (our proxy loads ReShade renamed, or ReShade
   loads us) — untested; temporal effects remain banned (B9c).
4. **Square-window per-eye aspect fix, zero code:** a square game window
   (or FlatVR-style virtual monitor) gives square eye buffers. Cheap
   workaround for docs/09 B9 until aspect-aware swapchains land.
5. **Cinema/diorama mode** — their "game on your floor" view. IMPLEMENTED
   this round as `[Output] CinemaMode=1`: single fixed quad layer ~2.2 m
   ahead of the recenter reference (pair with HeadTracking=0).
