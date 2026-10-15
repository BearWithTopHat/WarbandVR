# 11 — Lessons from h2-mod-vr (MW2 Remastered VR)

Reviewed: https://github.com/Aeka0/h2-mod-vr (GPLv3, D3D11, SteamVR/OpenVR).

## Why their fidelity is higher — the structural fact

h2-mod-vr is built on **H2-Mod, a reconstructed/reverse-engineered client**
(IW6x/S1x lineage). They own the engine's internals, so they can do what
WarbandVR's docs/10 calls tier 5: physical weapon grips, per-weapon reload
procedures (mag buttons vs. pull-out magazines, pump actions, revolvers),
holster collision, scripted-scene adaptation. **None of that transfers
directly** — Warband is closed source, and our tier-4 equivalent
(companion-mod rituals) is the ceiling without engine RE.

## What DID transfer (implemented this round)

| Their feature | Our adoption | Where |
|---|---|---|
| Snap turning (comfort) | `SnapTurnDegrees` — right-stick X edge → rotate the pose *reference*, pivoting around current head position. Zero engine writes. | OpenXRBridge::SnapTurn, MotionInput |
| Body holsters (release grip at a zone to stow) | Grip **release** inside a grab zone emits `Holster` key (F6). Grab = press, holster = release — one continuous grammar. | MotionInput Gestures |
| Desktop spectator view | `MirrorOnDesktop=1` composites SBS to the monitor even while the session runs. | StereoRenderer::OnPresent |
| "Preserve responsibility boundaries of native game logic" | Already our tier design (docs/10); reinforced. | — |

## Adopted as documented guidance (not code)

- **First-time setup order**: controller alignment and turning FIRST, then
  everything else — now the recommended order in docs/08 bring-up.
- **Per-weapon procedures**: their mag/slide/charging-handle variety maps to
  our tier-4 ritual design — the companion mod can make each weapon type
  demand its own gesture sequence (docs/10 §4). Different ritual per weapon
  class is script-level work.
- **Controller alignment calibration**: grip angles differ per controller
  family; they ship a calibration step. Our equivalent would offset the
  swing-vector transform; deferred until M5 playtesting shows a need
  (docs/09 B9b tuning path covers it).
- **Only the developer's hardware was ever tested** — they ship untested
  bindings for Index/Vive. We ship five untested profiles too; same honesty
  applies. First hardware session should expect binding fixes.

## Explicitly rejected

- OpenVR/SteamVR-input manifest approach — we standardised on OpenXR
  suggested bindings (runtime-agnostic; SteamVR is one backend).
- Launcher/setup GUI — out of scope for the DLL; our "launcher" is the ini +
  docs/08. A GUI is polish for M6 if desired.
