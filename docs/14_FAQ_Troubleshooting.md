# 14 — FAQ and troubleshooting

This page collects the likely failure modes from the risk register and the
bring-up docs. Start here before changing code.

## First rule

If anything is wrong, set:

```ini
[Stereo]
EnableStereo=0
```

Then boot the game. If stock behavior returns, the problem is in a gated VR
feature. If it does not, remove `d3d9.dll` from the game folder and treat the
last code or deployment change as the suspect.

## FAQ

### Is WarbandVR safe to leave installed?

Yes when built and configured correctly. The shipped configuration is disabled
(`EnableStereo=0`) and the proxy should act as a pass-through. Uninstalling is
still just deleting the proxy DLL.

### Does it work in multiplayer?

No. The project is deliberately single-player-only. Injection-style rendering
mods and multiplayer trust/fairness rules do not mix.

### Why a full device wrapper instead of vtable hooks?

Steam's overlay can replace the D3D device object's vtable pointer with its
own shadow table, silently bypassing in-place patches. The full `DeviceProxy`
wrapper is the object the game holds, so the overlay cannot remove the mod's
interception layer that way.

### Why does input use keyboard and mouse emulation?

Warband is closed-source and 32-bit. SendInput-style OS input follows a
documented engine behavior without writing engine memory or depending on
fragile build offsets. It also keeps the mod compatible with ordinary key
bindings and the companion module-system channel.

### What is the companion module for?

The DLL can detect gestures, but only a Warband module can cleanly change
equipment or run module-side rituals. The DLL emits synthetic keys such as
F1–F6; the companion module validates agent/item state and performs the
resulting single-player action.

## Troubleshooting

### Game does not start after installing

1. Restore `EnableStereo=0`.
2. Check that the DLL is 32-bit (`dumpbin /headers` should report x86).
3. Remove `d3d9.dll` to confirm stock boot.
4. Inspect `WarbandVR_log.txt`; a crash never intentionally swallows the last
   line because logging is flushed per line. Hard faults are logged as
   `CRASH code=0x... at module+0x...` by the proxy's exception handler, and
   the previous run's log is kept as `WarbandVR_log.prev.txt` — include both
   files in a bug report.

### Log stops after `Direct3DCreate9`

Warband starts through a launcher. The launcher enumerates Direct3D adapters;
the real game device appears only after Play is pressed. Use
`tools/launch_game.ps1` for scripted runs.

### Menu or world appears only in one eye

This was historically caused by mono UI/composite passes and later by the
engine's Z-prepass interacting with a shared double-width depth buffer. The
current architecture uses per-eye game-sized color/depth targets and separate
flat UI duplication. If the symptom returns, do not tune IPD; collect a frame
dump and check the duplication gate and render-target routing.

### Steam overlay appears to break hooks

The full wrapper should be immune to vtable-pointer shadowing. Verify that the
game really loaded this proxy, that no other `d3d9.dll` wrapper is chained
first, and that the log reaches `Device created OK`.

### Headset shows no image

Check in order:

1. OpenXR build, not the stub build.
2. x86 `openxr_loader.dll` beside the game executable.
3. SteamVR selected as the active OpenXR runtime.
4. `EnableStereo=1` and `OutputMode=1`.
5. Slow path first: `SharedSurface=0`.
6. Steam Link streaming already works for the flat game.

### Menus or dialog text sit in the wrong place (misaligned with backgrounds)

That was the encounter-dialog layout bug: fixed-function UI with an identity
view matrix and an orthographic projection was being eye-shifted, and under
an ortho projection a view translation is a pure screen-space offset — text
moved while backgrounds drawn through the flat path stayed put. Current
builds gate the eye view shift on the PROJECTION type: only perspective
scenes get it; orthographic/2D UI stays flat and identical in both eyes. If
you still see it on a current build, capture a frame dump and the log.

### Head movement is mirrored or moves the wrong axis

Use `[Tracking] InvertYaw`, `InvertPitch`, `InvertRoll`, `InvertPosX`, or
`InvertPosZ`. Do not patch sign conventions in code for one runtime.

### Swing attacks fire while drawing a bow

That should not happen: the bow-draw trigger owns LMB while held, and the
swing classifier is suppressed. If it reproduces, capture the motion-input log
and test with `[Input] MotionMelee=0` to separate emulation from melee
classification.

### Headset picture freezes when SteamVR or the runtime restarts

Session and instance loss are recovered automatically: the bridge tears down
and retries initialization every 5 seconds (watch for `session loss pending`
/ `instance loss pending` in the log). If it does not recover, the runtime
never came back up — restart the game.

### Periodic stutter at a regular interval

The runtime's frame pacing may be fighting the game's own frame limiter. Set
`[Output] FramePacing=1` so the game free-runs and XR receives at most one
submitted frame per display period. Keep `ForceNoVsync=1`.

### One eye tears or lags a frame with SharedSurface=1

The cross-API shared-surface copy has no fence by default. Set
`[Output] SharedSyncMode=1` to wait on a D3D9 event query before the copy.
If it persists, fall back to `SharedSurface=0`.

### Attacks fire when reaching for a weapon or quiver

That should not happen: zone grabs start a `GrabRefractoryMs` swing
suppression window. If it reproduces, raise the value (default 400) or check
that the reach genuinely ends inside the zone (grab key logged).

### Gesture keys do nothing in game

Confirm which layer is failing:

1. Use a key-display overlay to confirm F1–F6 arrive.
2. Confirm the companion module source was rebuilt into the active module.
3. Confirm the agent actually has an item in the mapped equipment slot.
4. Check the companion README's slot map and key map against your ini.

### Performance drops in large battles

Warband is CPU-bound and stereo doubles intercepted draw work. Check the
periodic `[PERF]` log line, reduce battle size, disable the shared-surface
experiment if enabled, and keep `MotionMelee`/debug logging off while
measuring.

## Hardware verification

Automated tests prove compile health and headless logic only. Hardware status
must be recorded against `docs/manual_test_checklist.md`; never label a build
runtime-verified unless the relevant checklist entry was run on the real game
and headset.
