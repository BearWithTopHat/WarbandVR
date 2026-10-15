# 04 — Reverse engineering playbook (M0)

Goal: produce three facts — **(1)** how Warband delivers the view matrix,
**(2)** which VS register holds it (if shaders), **(3)** where the engine's
camera struct lives in memory. Everything downstream keys off these.

Tools: the proxy itself (capture modes from task M0-1), RenderDoc or PIX for
frame capture, Cheat Engine + x64dbg (or Ghidra/IDA) for memory work.

---

## Part A — Fixed-function or shader?

1. Build and deploy the proxy with the M0-1 capture modes implemented.
2. Set `[Debug] CaptureMode=1, LogLevel=3`, `EnableStereo=0`. Boot, load a
   battle, look around for 10 seconds, quit.
3. Read `WarbandVR_log.txt`:
   - If `SetTransform(D3DTS_VIEW)` fires per frame with matrices whose
     translation row tracks your movement → **ViewSource=0** path is live.
   - If it's silent or the values look static/identity → the engine is
     shader-driven; go to Part B.
4. Cross-check with RenderDoc: capture one frame, look at the vertex shader
   input constants for any draw call — is there a world→view or
   view→projection matrix in the constant registers?

**Result A:** record in `WarbandVR.ini` as `ViewSource` (0 or 1) — and note
that *both* can be true at once (fixed-function terrain + shader skinning is
common in 2010-era engines). If both, the mod must apply per-eye matrices
through both paths simultaneously.

## Part B — Which register? (shader path only)

1. `[Debug] CaptureMode=2` logs every `SetVertexShaderConstantF` write with
   a plausibility ranking (last row ≈ [0,0,0,1] for pure view matrices).
2. Correlate with movement: stand still (values static) vs. move (which
   registers change?). Candidate registers change when the camera moves AND
   when it rotates.
3. Confirm in RenderDoc: select a skinned-character draw call, inspect
   vertex shader constants at the register your log flagged — the values
   should match. Note whether it's view-only or view·proj (multiply by the
   captured projection and compare).
4. Record `VSViewRegister` in the ini and note in the results table whether
   the register appears to hold view-only or view·proj. There is intentionally
   no separate `VSMatrixIsViewProj` runtime key; the current shader path reads
   back the register per draw pair and classifies flat UI from the actual
   matrix values.

## Part C — Camera struct (needed for M4, not M1)

The view matrix is derived each frame from an engine-side camera struct
(position, forward/up/right or yaw/pitch/roll, FOV). Finding it lets M4 write
head pose *into the engine* instead of layering on the matrix.

Procedure (Cheat Engine):
1. Stand somewhere known; note position from the captured view matrix's
   translation row (or `CameraScan`'s `ExpectedPos`).
2. Unknown-initial-value scan (float) → move forward → "increased/decreased"
   refinement loops on the axis you moved along → narrow to <50 addresses.
3. For candidates: freeze the address in CE. If the in-game camera stops
   moving on that axis → you found a live camera coordinate.
4. Find what writes it (right-click → "find out what writes to this
   address") → the instruction is inside the engine's camera update; the
   struct base is `register + offset` at that site. Record base + offsets
   for position, and repeat for orientation (rotate the camera, watch which
   nearby floats change).
5. Validate with `CameraScan` (task M0-2) keeping the proxy-side candidate
   list warm.

**Version sensitivity:** offsets differ per Warband build. Record the exact
Steam build ID and exe SHA-256 next to every offset table. The M4 module must
read the ini's `[Camera] GameBuild` and refuse to write on unknown builds.

## Results (fill in as M0 completes)

| Question | Answer | Evidence (log/capture) | Date | Game build |
|---|---|---|---|---|
| View delivery mechanism | _TBD_ | | | |
| VS register / layout | _TBD_ | | | |
| Camera struct base + offsets | _TBD_ | | | |

## Gotchas learned the hard way

- RenderDoc intercepts D3D9 too — load order fights with our proxy are
  possible. If RenderDoc can't see frames, temporarily rename our dll out of
  the game folder; capture stock, use the proxy's own logs for correlation.
- Warband's `rgl_config.txt` forces some render paths; keep a copy of the
  stock config used during M0 so experiments are reproducible.
- The engine may cache transforms and skip redundant `SetTransform` calls —
  a "silent" log in one scene doesn't disprove fixed-function. Test on foot,
  on horseback, and in a siege before concluding.
