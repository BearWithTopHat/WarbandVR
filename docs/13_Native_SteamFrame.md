# 13 — Native Steam Frame (Phase 2)

Phase 2 runs the whole stack **on the Steam Frame itself**: Warband under
Proton (x86 Windows game on ARM64 via FEX), our proxy as a native DLL
override, OpenXR via wineopenxr → Monado. Nothing in this document has been
executed on hardware — every step lists its failure mode and fallback.

The PCVR streaming setup (Phase 1) is the default and the reference. Phase 2
exists to remove the PC. If D2 below fails, the answer is "stay on
streaming", not heroics.

## Deploy

1. Install Warband on the Frame via Steam (it downloads the Windows build;
   SteamOS runs it under Proton automatically).
2. Build `Release|Win32` (SBS first, **not** the OpenXR config) and copy
   `d3d9.dll` + the Native package's annotated `WarbandVR.ini` next to
   `mb_warband.exe` in the compatdata prefix. The native package also carries
   `README_NATIVE.md` with the expected SteamOS paths; there is currently no
   automated native deploy script.
3. Set the DLL override so our proxy wins over Wine's builtin d3d9:

   ```
   WINEDLLOVERRIDES="d3d9=n,b" %command%
   ```

   (Steam → game Properties → Launch Options.)
4. Keep the Native package ini profile: `SharedSurface=0` (D3D9Ex shared
   surfaces are untested under Wine/DXVK — risk D2/D5 in docs/09). The profile
   is shipped as the package's `WarbandVR.ini`, not as a separate
   `profiles/` file.

## Bring-up order (matches the risk register in docs/09)

1. **D3 — flat perf.** Stock game under Proton, no proxy. If Warband isn't
   playable flat under FEX, stop here.
2. **D1 — proxy + SBS.** Our DLL with `EnableStereo=1`, `OutputMode=0`.
   Validates proxy load order, the full DeviceProxy wrapper, and the stereo
   renderer on Wine's d3d9 (DXVK). Check `WarbandVR_log.txt`.
3. **D2 — XR.** Build `Release-OpenXR|Win32`, `OutputMode=1`. The known
   gate is `xrGetD3D11GraphicsRequirementsKHR` through wineopenxr → Monado;
   failure is explicit in the log. If the D3D11 binding is unsupported:
   (a) track Wine/Proton releases, (b) a Vulkan interop path (large work),
   (c) stay on streaming.
4. **D4 — controllers.** SteamOS/Monado pairing should be native and
   *better* than streamed (streaming risks C2/C3 disappear). Validate the
   five binding profiles in MotionInput.

## Honest unknowns

- **FEX overhead on a CPU-bound 2010 title** — unmeasured; step 1 answers it.
- **wineopenxr D3D11 interop status** — version-dependent; treat as the
  go/no-go gate.
- **DXVK shared-surface behavior** — untested; the native profile keeps the
  slow path. If the slow path's `GetRenderTargetData` stalls prove too
  costly under DXVK, try `SharedSurface=1` as an experiment, not a default.
- **Wine's loader and our `TargetName=d3d9`** — the override mechanism is
  standard, but verify the load order in the log at step 2 (the log prints
  which d3d9 we forwarded to).

## Community feasibility signals

Anecdotal evidence that the Phase 2 route is viable (a signal, not data):

- **2026-10 — r/SteamFrame:** a developer copied a work-in-progress PC VR
  motion-controls mod for Mafia onto a Steam Frame as a plain x86 Windows
  build — no ARM rebuild, no optimization — and had it running standalone in
  under two hours (AI-assisted compatibility-layer fixes).
  <https://www.reddit.com/r/SteamFrame/comments/1x3vyl5/>
  This matches our architecture's assumptions: proxy-DLL mods survive the
  Proton/FEX translation layer, and the OpenXR boundary is the only runtime
  surface that matters. Caveats: "works" there means launches-and-renders,
  with no published frame times or latency; Mafia-era rendering differs from
  Warband's; and none of our bring-up gates (D1–D4 above) are settled by it.
  The manual checklist remains the release gate.

## What does NOT change from PCVR

All of src/ is byte-identical between the PCVR and Native packages (enforced
by `tools/check_parity.py`). The intentional Native deltas are the annotated
ini profile, `README_NATIVE.md`, and packaging metadata; there is no separate
native source fork.
