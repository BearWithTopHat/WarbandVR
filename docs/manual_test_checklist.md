# Manual in-game test checklist

Everything on this page requires a human with the game (+ HMD for the VR
sections). Automated gates cover compile and headless logic only — **no
entry here may be claimed verified by an agent**. Reference the risk
register in `docs/09_Risks_SteamFrame_Phase2.md` for known-risky areas.

Run order matters: always clear section 0 before anything else.

## 0. Stock boot (release-blocking)

- [ ] `EnableStereo=0` → game boots to main menu, rendering pixel-identical
      to no-proxy, `WarbandVR_log.txt` shows no WARN/ERROR.
- [ ] `EnableStereo=0` → enter a battle, play 5 min, no crash, no perf
      difference.
- [ ] Quit game → no lingering key presses in Windows (stuck-keys).

## 1. SBS output (OutputMode=0)

- [ ] `EnableStereo=1`, SBS mode: both halves render the world.
- [ ] UI (HUD, menus) flat and identical in both halves (SBSMonoUI).
- [ ] The fullscreen composite quad does NOT get eye-shifted (no stale sky
      in the right half — historical regression).
- [ ] Eye separation looks correct; `IPD`/`WorldScale` respond to ini edits.
- [ ] `AsymmetricProjection=1`: no toe-in distortion at screen edges.
- [ ] `WorldScalePreset` compact→giant visibly changes world scale; the
      expert `[Stereo] WorldScale` key overrides the preset.
- [ ] `Vignette=1`: radial comfort darkening at the periphery in both eyes;
      `VignetteStrength` responds and clamps at 0..1.

## 2. HMD output (OutputMode=1, PCVR streaming)

- [ ] Session starts, per-eye image in the headset, correct left/right
      orientation (no swapped eyes).
- [ ] Framerate paces to the HMD (no double vsync; `ForceNoVsync=1`).
- [ ] Steam overlay opens/closes without corrupting the eye targets.
- [ ] `MirrorOnDesktop=1`: SBS mirror on monitor while HMD runs.
- [ ] `CinemaMode=1`: fixed quad, menus readable, no head coupling.

## 3. Tracking

- [ ] Head yaw/pitch/roll match real movement; if mirrored, flip the
      matching `[Tracking] Invert*` key — never accept a code flip.
- [ ] Positional tracking direction correct (lean forward = forward).
- [ ] `Smoothing=1`: no jitter, no swimmy lag (OneEuro tuning:
      `SmoothingMinCutoff`, `SmoothingBeta`).
- [ ] Recenter key (F8) recenters yaw and position.
- [ ] In-headset recenter: hold left stick click ~3 s, right stick centered
      → view recenters; moving the right stick during the hold cancels it.
- [ ] `LockInMenus=1`: tracking freezes in menus, resumes in field.

## 4. Motion input (M5)

- [ ] Left stick = WASD; right stick = mouse look (or snap turn when
      `SnapTurnDegrees>0`, with return-to-centre re-arm).
- [ ] Face buttons = Use/Jump keys.
- [ ] Swing directions match hand motion: horizontal R/L swings, downward
      chop = overhead, forward = thrust. Cross-check `[MELEE]` log lines.
- [ ] Held attack steers with the hand arc (`MotionSteer=1`); releases when
      the hand stops or after `SteerMaxHoldMs`.
- [ ] Grip hold = block (RMB) after `BlockHoldTime`; taps don't flick it.
- [ ] Bow draw: trigger hold = LMB hold; release = loose. A swing started
      while drawing is suppressed, and vice versa (LMB single-owner rule).
- [ ] Grab zones: hip L/R, over-shoulder, quiver produce F1–F4 key events;
      grip release in a zone produces F6 (holster); both grips 0.5 s = F5.
      (Verify with a key-display overlay or the companion mod.)
- [ ] Reaching fast into a grab zone does NOT fire an attack
      (`GrabRefractoryMs` window); a deliberate swing right after still works.
- [ ] Held-attack steering never jumps more than `SteerMaxDeltaPx` per frame,
      even on a deliberate fast flick.
- [ ] Look straight up/down (pitch > 60°): world stays stereo, does not go
      flat (UIRowNormMax guard).
- [ ] Quit game → no stuck LMB/RMB/keys (shutdown release path).

## 5. Streaming / Steam Frame specifics (Phase 1)

- [ ] Stream from PC to the Frame: frame pacing stable, no double
      compression artifacts beyond baseline streaming. If periodic stutter
      appears, retry with `[Output] FramePacing=1`.
- [ ] Steam Link / overlay interactions don't steal the d3d9.dll hook.
- [ ] Wi-Fi hiccup recovery: session resumes without DLL reload.
- [ ] SteamVR restart mid-game: bridge logs session loss and reinitializes
      within ~5 s; picture resumes without restarting the game.
- [ ] `SharedSurface=1`: both eyes current, no tearing. If one eye lags,
      retry with `SharedSyncMode=1`, else ship `SharedSurface=0`.
- [ ] Crossbow ritual in either order: quiver-grab then both-grips AND
      both-grips then quiver-grab must each seat one bolt; switching weapons
      mid-ritual resets it within ~1 s.
- [ ] Runtime matrix: validate on SteamVR OpenXR first. Meta's own runtime
      is a known-bad path in adjacent projects (docs/15); answer Meta-runtime
      issues with "use SteamVR or VDXR", not code changes.
- [ ] `ControllerPreset=quest` (then `psvr2`) loads its section overrides;
      tune the shipped values from real measurements and record them here.

## 6. Phase 2 (native on Frame) — see docs/13

- [ ] Proton prefix boots Warband with `WINEDLLOVERRIDES="d3d9=n,b"`.
- [ ] `SharedSurface=0` path works under Wine/DXVK (risk D2).
- [ ] wineopenxr → Monado session; D3D11 binding accepted (risk D3).

## Perf rollup

- [ ] Duplication cost per frame logged (`AddDuplicationTime` rollup) stays
      within the budget table in docs/01; note the number here: ______ ms.
