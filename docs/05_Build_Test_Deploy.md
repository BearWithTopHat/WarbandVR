# 05 — Build, test, deploy

## Toolchain

- Visual Studio 2022 (Community is fine) with "Desktop development with C++",
  MSVC v143, Windows 10/11 SDK.
- Platform: **Win32 (x86)** — mandatory; Warband is 32-bit.
- Default build: zero external dependencies (static CRT).
- M3 only: OpenXR SDK (Khronos loader + headers). Install anywhere; point the
  `Release-OpenXR` configuration's include/lib dirs at it.

## Build

```
msbuild WarbandVR.sln /p:Configuration=Release /p:Platform=Win32
```

or build `Release|Win32` in the IDE. Output: `Release/d3d9.dll`.

Sanity check that the output is 32-bit:
`dumpbin /headers Release\d3d9.dll | findstr machine` → `14C (x86)`.

## Deploy

```
powershell -ExecutionPolicy Bypass -File tools\deploy.ps1 -GameDir "C:\Program Files (x86)\Steam\steamapps\common\MountBlade Warband"
```

Copies `d3d9.dll` + `WarbandVR.ini` next to `mb_warband.exe`. The ini is only
copied if absent, so tuned configs survive redeploys. Uninstall:
`deploy.ps1 -GameDir ... -Uninstall` (or just delete the two files).

Note: the game folder may be under `Program Files (x86)` — run the deploy
shell as Administrator if copies fail. (Or move the game via Steam to a
library outside Program Files — recommended; saves a lot of friction.)

## The test ritual (run after EVERY change)

1. **Stock boot.** `EnableStereo=0`. Launch, reach the main menu, load a
   save, walk 30 seconds. Rendering must be indistinguishable from vanilla.
   `WarbandVR_log.txt` must show: proxy init → `CreateDevice` →
   `15/15 vtable hooks installed` → no `ERR` lines.
2. **Feature on.** Flip only the flag under test. Verify the feature.
3. **Stability probes:** alt-tab out and back; toggle fullscreen/windowed;
   trigger a battle load screen. (These all exercise `Reset` — historically
   the #1 crash site for D3D9 proxies.)
4. **Log review** at `LogLevel=2`. Any `ERR`/`WRN` you don't recognize = stop
   and explain it before proceeding.

## Debug logging

`WarbandVR_log.txt` is written next to the exe, flushed every line (a crash
never eats the last message). `LogLevel=3` is verbose — expect large files in
battles; use only for targeted repro sessions.

For live debugging, Visual Studio can attach to `mb_warband.exe`
(Debug → Attach to Process) with the pdb in place — breakpoints in detours
work normally. Expect the game to be frozen while broken in.

## Testing in the headset (from M3)

1. Start SteamVR on the PC, confirm the Steam Frame streams flat games fine
   (the user's existing setup).
2. Launch the game; with `OutputMode=1` the mod creates the OpenXR session.
3. First checks: correct eye assignment (close each eye), IPD sanity
   (doorway test — does a doorway look doorway-sized?), 10-minute stability.
4. Stream quality is a Steam Link concern, not ours — if flat streaming was
   clean, VR streaming of the same machine will be too, bandwidth permitting.

## Performance measurement

- Built in: at `LogLevel=2`, a `[PERF]` line is logged every 600 frames with
  average draw-duplication cost (µs/draw and ms/frame). This is the number
  the M2 budget (<4 ms/frame added) is checked against.
- External: CapFrameX or PresentMon on the PC for end-to-end frame times;
  Steam Link's built-in performance overlay for the streamed side.
- Targets: <4 ms added CPU time per frame (M2); 72 fps minimum in a
  150-agent battle on the user's PC (M2/M6).

## Rollback plan

Any bad state: delete `d3d9.dll` from the game folder → game is stock.
Everything else (ini, logs) is inert without the dll. Git tags at each
milestone; never deploy from an uncommitted tree.
