# 00 — Human runbook: from this scaffold to first stereo image

This is the exact sequence for the human half of the team. Everything the
agent could do without the game is already done; what remains here is
"install, build, run, read logs, report numbers." Estimated total human time
to first stereo image: **one focused evening**.

## Phase 0 — machine setup (30 min, once)

1. Install **Visual Studio 2022 Community** (free) with the workload
   *"Desktop development with C++"*. That includes MSVC v143 and the
   Windows SDK — the only dependencies.
2. In Steam, move Warband to a library **outside** `Program Files (x86)`
   (Steam → Settings → Storage → add e.g. `D:\SteamLibrary`, move game).
   Saves permission friction on every deploy.
3. Note your game build: Steam → Warband → Properties → check for updates;
   the exact version matters for offset tables later.
4. `git init` the repo and commit the scaffold. Every agent session starts
   from a commit and ends with one.

## Phase 1 — build & smoke test (15 min)

1. Open `WarbandVR.sln`, select **Release | Win32**, build.
   - Expect 0 errors. If the scaffold bit-rotted, paste the build log into
     the agent with the master prompt from `docs/03` — this is precisely the
     kind of task it can fix unattended.
2. Deploy:
   `powershell -ExecutionPolicy Bypass -File tools\deploy.ps1 -GameDir "D:\SteamLibrary\steamapps\common\MountBlade Warband"`
3. Launch the game (stock ini: everything off). It must be **indistinguishable
   from vanilla**. Note the game first shows a **launcher dialog** — press
   Play. (For automated runs, `tools\launch_game.ps1` clicks it for you.)
   Confirm `WarbandVR_log.txt` exists next to the exe and
   contains `18/18 vtable hooks installed` and the `Host exe:` line.
   - Copy the `Host exe:` line into `WarbandVR.ini → [Camera] GameBuild`
     (paste the size+mtime, e.g. `12345678-9ABCDEF012345678`).
4. If the game won't boot: delete `d3d9.dll` from the game folder, hand the
   log to the agent. This is the one hard-stop checkpoint.

## Phase 2 — M0 capture runs (1–2 h, the only real homework)

Follow `docs/04`. Short version:

1. Set `[Debug] CaptureMode=1`, `LogLevel=2`. Play a battle 30 s, quit.
   - Log says `SetTransform: VIEW xNNN` per frame → set `ViewSource=0`.
   - Log says VIEW x0 → set `CaptureMode=2`, run again, and the log will
     rank shader registers holding matrix-like data; put the top candidate
     into `VSViewRegister` and set `ViewSource=1`.
2. (M4 prep, optional now) Camera struct hunt: `[Debug] CameraScan=1` +
   `ExpectedPos`. Stand somewhere, read coordinates off the log's captured
   view matrix, walk 60 s, collect the converged candidate list, then confirm
   in Cheat Engine per docs/04 Part C.

## Phase 3 — M1 first stereo (the payoff, ~1 h)

1. `EnableStereo=1`, `OutputMode=0`. Launch → first person on foot.
2. What you should see: everything doubled side-by-side with slightly
   different viewpoints. Cross your eyes (or record 10 s and review) to check
   parallax: near objects should differ more between halves than distant
   mountains.
3. Tune `IPD`/`WorldScale` until scale feels plausible in the SBS image
   (rough is fine; final tuning happens in the headset at M3).
4. Alt-tab, resize, load a 100-agent battle. Crashes/artifacts → log to agent.

Report back to the agent: which `ViewSource` worked, the log excerpt, and
what the image looks like ("double UI", "one eye black", "perfect SBS").
That phrase-level feedback maps directly onto the fix list in M1-1.

## Phase 4 — hand back to the agent

From here the loop is: agent implements next task from `docs/03` → you build,
deploy, run the acceptance checklist, report. M2+ needs only your eyes; M3+
needs the headset; M5 needs your sword arm.

## Cheat sheet: what each file in the log means

| Log line | Meaning |
|---|---|
| `CreateDevice ... hooks installed` | Proxy live, device hooked (18/18) |
| `[CAPTURE f=...] SetTransform: VIEW xN` | M0 mode 1: N per-frame view writes |
| `c17  writes=... viewLike=...` | M0 mode 2: register c17 looks matrix-like |
| `[CAMERASCAN] converged to N candidates` | M0-C: camera position candidates |
| `[PERF] ... ms/frame` | duplication overhead (M2 budget: <4 ms) |
| any `ERR` | stop; feed log to agent |
