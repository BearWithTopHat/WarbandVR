# 08 — Controls & setup guide

## Setup (quick path)

1. **Build.** `Release|Win32` for monitor SBS; `Release-OpenXR|Win32` for
   headset output (OpenXR SDK setup: docs/05 — remember the loader must be
   the **x86** build).
2. **Deploy.** `tools/deploy.ps1` copies the built `d3d9.dll` + `WarbandVR.ini`
   next to `mb_warband.exe`. For the OpenXR build also copy the x86
   `openxr_loader.dll` there once.
3. **Runtime.** SteamVR as the active OpenXR runtime on the PC; stream to the
   Steam Frame with your existing Steam Link setup.
4. **First run.** Leave `EnableStereo=0` to verify stock boot, then flip on
   features one at a time (see "Recommended bring-up order" below).

## INI reference (WarbandVR.ini)

### [Stereo]
| Key | Default | Meaning |
|---|---|---|
| EnableStereo | 0 | Master switch. 0 = byte-identical stock behavior. |
| IPD | 0.064 | Interpupillary distance, meters. |
| WorldScale | 1.0 | Stereo depth intensity multiplier. |
| ViewSource | 0 | 0=fixed-function SetTransform, 1=vertex-shader constants. |
| VSViewRegister | 0 | Shader constant register holding the view matrix. |
| AsymmetricProjection | 0 | Off-center projection per eye. |
| SBSMonoUI | 1 | UI batches drawn flat into both eyes. |
| UIMaxTriangles | 64 | Batch triangle count treated as UI. |
| UIZScaleMax | 0.5 | Matrix \|_33\| below this counts as ortho UI. |
| UIWScaleEps | 0.01 | Matrix \|_44 − 1\| within this counts as affine UI. |
| UIRowNormMax | 0.5 | 3x3 row-norm cutoff; keeps steep-pitch views stereo. |
| OffscreenPasses | 0 | Shadow/offscreen pass interception; experimental. |

### [Output]
| Key | Default | Meaning |
|---|---|---|
| OutputMode | 0 | 0 = SBS on monitor, 1 = OpenXR headset. |
| SharedSurface | 0 | Fast GPU handoff (D3D9Ex). Try slow path first. |
| MirrorOnDesktop | 0 | Spectator SBS on the monitor while in VR. |
| ForceNoVsync | 1 | Force present-interval immediate under OutputMode=1. |
| CinemaMode | 0 | Fixed quad "diorama" display instead of immersive projection. Pair with HeadTracking=0. |
| FramePacing | 0 | 0 = xrWaitFrame paces the game thread; 1 = free-run, XR gets one frame per display period. Try 1 for periodic stutter. |
| SharedSyncMode | 0 | SharedSurface=1 only: 0 = D3D11 flush; 1 = D3D9 event-query wait before the copy. Try 1 for one-eye tearing/lag. |

### [Comfort]
| Key | Default | Meaning |
|---|---|---|
| WorldScalePreset | realistic | `compact`, `realistic`, `large`, or `giant`. Applied before the expert `WorldScale` key. |
| Vignette | 0 | Radial comfort vignette rendered into both eye targets. |
| VignetteStrength | 0.55 | Vignette intensity, clamped to 0–1. |

The friendly preset sets `WorldScale` first. Uncommenting `[Stereo]
WorldScale` is still supported as an expert override and wins over the preset.

### [Tracking]
| Key | Default | Meaning |
|---|---|---|
| HeadTracking | 0 | Apply head pose to the view (needs OutputMode=1). |
| Smoothing | 1 | OneEuro smoothing on pose. |
| SmoothingMinCutoff / SmoothingBeta | 1.5 / 0.02 | Lag↔judder tradeoff. |
| RecenterKey | 0x77 (F8) | Recenter the pose reference. |
| LockInMenus | 1 | Freeze tracking when the view is static. |
| InvertYaw/Pitch/Roll/PosX/PosZ | 0/0/0/0/1 | Sign-convention fixes, no rebuild needed. |

### [Input] / [InputMap] (Release-OpenXR build, headset session running)
| Key | Default | Meaning |
|---|---|---|
| EmulateControllers | 1 | Controllers emulate kb/mouse. |
| MotionMelee | 0 | Swing-to-attack. Enable after emulation is comfortable. |
| SwingThreshold | 1.8 | Hand speed (m/s) that starts a swing. |
| BlockHoldTime | 0.15 | Grip-hold seconds before block engages. |
| AttackCooldownMs | 400 | Min ms between injected attacks. |
| MotionSteer | 1 | Held swing follows live hand motion. |
| SteerScale | 40.0 | Mouse px per m/s of hand speed while steering. |
| SteerMaxDeltaPx | 60 | Per-poll clamp on steering deltas; 0 disables steering. |
| SteerMaxHoldMs | 600 | Safety force-release of a held attack. |
| GrabRefractoryMs | 400 | Swing suppression after a zone grab/holster. |
| SnapTurnDegrees | 0 | >0: snap turn by N degrees on stick X (comfort). |
| RecenterHoldSeconds | 3.0 | Hold left stick click this long (right stick centered) to recenter in-headset; 0 = off. |
| ControllerPreset | (empty) | `generic`/`psvr2`/`quest`: loads `[Preset.<name>]` overrides for SwingThreshold/SteerScale/BlockHoldTime. Empty = use [Input] values. |
| GestureGrab | 1 | Grab-zone gestures emit the F1–F6 gesture keys (companion module turns them into equipment switches). |
| AttackHoldTrigger | 1 | Right trigger hold = LMB hold (physical bow draw); owns LMB while held. |
| MouseSpeed | 6.0 | Right-stick look sensitivity. |
| StickDeadzone | 0.2 | Stick deadzone. |
| [InputMap] Forward/Back/Left/Right/Use/Jump | W/S/A/D/F/SPACE | Emulated keys (names or single chars). |
| [InputMap] GrabHipLeft/GrabHipRight/GrabShoulder/GrabQuiver/TwoHand/Holster | F1/F2/F3/F4/F5/F6 | Gesture-channel keys the companion module listens for. |

### [Camera]
| Key | Default | Meaning |
|---|---|---|
| GameBuild | (build tag) | Refuse engine-camera writes if the host exe doesn't match (M0-C gate). |

### [Debug]
| Key | Default | Meaning |
|---|---|---|
| LogLevel | 2 | 0=errors … 3=verbose. |
| LogViewMatrices | 25 | Dump the first N captured view matrices to the log. |
| CaptureMode | 0 | M0 recon (docs/04): 1=SetTransform capture, 2=VS constants. One mode at a time. |
| DumpFrames | 0 | N>0: dump N stereo frames as BMPs. |
| CameraScan | 0 | M0-C experiment; memory scan for camera struct. |
| NoZTest | 0 | Diagnostic: disable depth testing on per-eye draws. |
| ExpectedPos / ScanBase / ScanSize | — | Camera scan parameters. |

## Controls in VR (Release-OpenXR build)

With `EmulateControllers=1` (and a bound profile — Touch/Index/Vive/WMR have
suggested bindings):

| Controller | Game action |
|---|---|
| Left stick | Move (WASD) |
| Right stick | Camera (mouse look) |
| Right trigger | Attack (LMB) — pure emulation mode |
| Right grip | Block (RMB) |
| A (right) | Use/interact (`[InputMap] Use`) |
| B (right) | Jump (`[InputMap] Jump`) |
| F8 (keyboard) | Recenter view |
| Left stick click, hold ~3 s | Recenter view in-headset (right stick centered; moving it cancels) |

With `MotionMelee=1` additionally:

| Motion | Game action |
|---|---|
| Swing down fast | Overhead chop |
| Swing sideways | Left/right slash |
| Thrust forward | Stab |
| Keep moving after contact | Swing follows your hand (MotionSteer) |
| Hold right grip | Block |
| Grip-press at hip / shoulder / quiver | Grab gesture -> F1–F4 keys (docs/10) |
| Grip-release at a body zone | Holster/stow -> F6 |
| Right-stick X (if SnapTurnDegrees>0) | Comfort snap turn |
| Both grips held 0.5 s | Two-handed stance -> F5 |
| Hold right trigger | Bow draw; release to loose |

Physical keyboard/mouse still work throughout — emulation is additive.

## Companion module

Gesture keys are only half of the equipment system. The proxy emits F1–F6;
Warband itself decides what those keys mean through the companion module in
`module_system/warbandvr_companion.py`.

Minimum integration:

1. Copy `module_system/warbandvr_companion.py` beside
   `module_mission_templates.py` in your single-player module source.
2. Import `warbandvr_mission_triggers`.
3. Concatenate the triggers into the mission templates that should support VR
   equipment gestures.
4. For the manual crossbow ritual, attach the crossbow item trigger described
   in `module_system/README.md` to crossbow items.
5. Rebuild the module and install the compiled output as usual.

The companion is single-player-only and deliberately conservative: the DLL
senses a gesture and emits a key; the module validates equipment state and
performs the equipment or reload action. Keep the proxy's F1–F6 bindings and
the module's slot map in sync.

## Recommended bring-up order

1. `EnableStereo=0`, OutputMode=0 → stock game must be untouched.
2. `EnableStereo=1`, OutputMode=0 → SBS on the monitor.
3. OutputMode=1 (Release-OpenXR build, slow path) → image in the HMD.
4. `SharedSurface=1` → fast path.
5. `HeadTracking=1` → fix axis signs with `Invert*` if needed.
6. `EmulateControllers=1` → play flat-style with controllers.
7. `MotionMelee=1` → swing combat; tune `SwingThreshold`/`SteerScale`.
