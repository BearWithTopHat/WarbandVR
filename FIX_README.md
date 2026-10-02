# Half-screen fix (2026-10-02)

## The symptom

Full game frame compressed into the **left** half of the screen; **right**
half shows only sky/clouds (menu backdrop sky on menu/map screens, skybox
in battle).

## Root cause

Warband's render pipeline ends with a **fullscreen composite quad** — a
2-triangle draw that blits the frame to the screen target. The `SBSMonoUI`
heuristic treated every draw with ≤64 primitives as "UI" and drew it **once**
into the current viewport. After `RestoreGameState`, that viewport is the
game's intended full-frame viewport — which on our double-width stereo target
is the **left half only**. So the complete final frame never reached the
right half; the right half kept whatever had been drawn directly to the
stereo target earlier (the sky backdrop layers).

## The fix (3 parts)

1. **Mono path now covers both eyes** — `ApplyEyeMono()`: UI/tiny-batch draws
   are executed once per eye with the per-eye viewport but **without** the
   view/projection shift. Both halves get the frame; UI stays flat.
2. **`DuplicationActive()` gate** — when the game has moved RT0 to its own
   offscreen surface (post-processing pass, `OffscreenPasses=0`), draws pass
   through once instead of being duplicated with a right-eye viewport that
   would clip against the game-sized target. RT0 ownership is now tracked
   explicitly (`m_rt0IsStereo`).
3. **Viewport capture ordering** — the game's viewport is captured in
   `OnBeginScene` **before** the stereo-target substitution. (D3D9 resets the
   viewport to the full size of a newly bound render target, so a
   post-substitution `GetViewport` returns the double-width viewport — a
   second, latent half-screen-class bug.)

## How to apply

Either apply the patch in your repo root:

```
git apply half-screen-fix.patch
```

…or copy the full replacement files over your tree (same relative paths):

```
src/DeviceHooks.cpp      src/StereoRenderer.cpp      src/StereoRenderer.h
WarbandVR.ini            docs/00_Runbook.md           docs/06_Risks_Decisions.md
```

Then rebuild **Release | Win32**, redeploy (`tools\deploy.ps1`), launch.

## What to expect after the fix

- Both halves show the full frame side-by-side.
- Menu text appears once per eye, at the same position (flat UI) — no more
  offset ghosting on long strings.
- The two halves will look **identical** for now. That's expected and
  correct: per-eye camera separation only engages once the view-matrix
  source is wired (`ViewSource`/`VSViewRegister` from the M0 capture run —
  the repo README already notes zero `SetTransform` calls, i.e. shader-driven,
  so M0's `CaptureMode=2` register ranking is the next real step).

## If anything is still off

Run once with `[Debug] DumpFrames=5` and grab `WarbandVR_frameNNN.bmp` from
the game folder plus `WarbandVR_log.txt` — the dump shows exactly what each
eye half contains and makes the next diagnosis one-shot.
