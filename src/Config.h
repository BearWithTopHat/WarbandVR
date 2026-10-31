// SPDX-License-Identifier: GPL-3.0-only
#pragma once

// Runtime configuration, loaded from WarbandVR.ini next to the proxy DLL.
// Everything that affects whether the mod touches the frame flows through
// here so a broken build can always be neutralised with EnableStereo=0.

struct Config
{
    // [Stereo]
    bool  enableStereo          = false;  // master switch - default SAFE
    float ipd                   = 0.064f; // metres
    float worldScale            = 1.0f;   // metres -> warband units
    int   viewSource            = 0;      // 0 = SetTransform, 1 = VS constant
    int   vsViewRegister        = 0;      // c-register of view(/proj) matrix
    bool  asymmetricProjection  = false;  // off-center frustum (M2 correctness)
    bool  sbsMonoUI             = true;   // draw tiny primitives flat in both eyes
    int   uiMaxTriangles        = 64;     // "tiny" threshold
    float uiZScaleMax           = 0.5f;   // |_33| below this => ortho UI matrix
    float uiWScaleEps           = 0.01f;  // |_44 - 1| within this => affine UI
    float uiRowNormMax          = 0.5f;   // 3x3 row norm below this => not a
                                          // rotation (steep-pitch view guard)
    bool  offscreenPasses       = false;  // M2: per-eye shadows for the game's
                                          // own render passes (UNTESTED - keep
                                          // off until validated on-game)

    // [Comfort]
    bool  comfortVignette       = false;  // static radial peripheral darkening
    float vignetteStrength      = 0.55f;  // 0..1, capped by the pixel generator
    char  worldScalePreset[16]  = "realistic"; // compact/realistic/large/giant

    // [Output]
    int   outputMode            = 0;      // 0 = SBS monitor, 1 = OpenXR
    bool  sharedSurface         = false;  // M3 fast path: D3D9Ex shared eye
                                          // targets (steers device creation to
                                          // Ex; keep off until slow path works)
    bool  mirrorOnDesktop       = false;  // composite SBS to the monitor even
                                          // while the XR session runs (spectator)
    bool  sbsAspectCorrect      = true;   // letterbox per-eye images into the
                                          // split panels instead of stretching
                                          // (menus keep their aspect ratio)
    bool  sbsMouseRemap         = true;   // remap cursor coords to the game's
                                          // full-window logical space (fixes
                                          // hover/hit-test in split panels)
    bool  forceNoVsync          = true;   // force present-interval immediate when
                                          // OutputMode=1 (XR pacing owns the rate)
    bool  cinemaMode            = false;  // fixed quad layer instead of immersive
                                          // per-eye projection (comfort/menus;
                                          // pair with HeadTracking=0)
    int   framePacing           = 0;      // 0 = xrWaitFrame paces game thread;
                                          // 1 = free-run, submit at most one XR
                                          // frame per display period
    int   sharedSyncMode        = 0;      // 0 = D3D11 flush before shared copy;
                                          // 1 = D3D9 event-query wait (stronger,
                                          // costs a GPU sync stall)

    // [Input] (M5; OpenXR build only)
    bool  emulateControllers    = true;   // controllers emulate kb/mouse
    bool  motionMelee           = false;  // swing-to-attack injection
    float swingThreshold        = 1.8f;   // m/s to start a swing
    float blockHoldTime         = 0.15f;  // s of grip hold before block engages
    bool  blockForearm          = true;   // left forearm raised = physical block
    bool  haptics               = true;   // haptic pulses on swing/block/bow release
    int   attackCooldownMs      = 400;    // min ms between injected attacks
    float mouseSpeed            = 6.0f;   // right-stick -> mouse px/frame
    float stickDeadzone         = 0.2f;
    bool  motionSteer           = true;   // held-attack swing follows hand motion
    float steerScale            = 40.0f;  // mouse px per m/s of hand speed
    int   steerMaxDeltaPx       = 60;     // per-poll clamp on steering deltas
    int   steerMaxHoldMs        = 600;    // force-release a held attack after this
    bool  gestureGrab           = true;   // grab-zone gestures (docs/10)
    int   grabRefractoryMs      = 400;    // swing suppression after a zone grab
    bool  attackHoldTrigger     = true;   // trigger hold = bow draw / LMB hold
    bool  bowTwoHand            = true;   // physical two-hand draw: pull hands
                                          // apart to draw, together to loose
    float snapTurnDegrees       = 0.0f;   // >0: right-stick X snap turn (e.g. 45)
    float recenterHoldSeconds   = 3.0f;   // hold left stick click to recenter; 0 = off
    char  controllerPreset[16]  = "";     // empty, or generic/psvr2/quest:
                                          // loads [Preset.<name>] key overrides

    // [InputMap] (emulation keys; names or single characters)
    int   keyForward            = 'W';
    int   keyBack               = 'S';
    int   keyLeft               = 'A';
    int   keyRight              = 'D';
    int   keyUse                = 'F';
    int   keyJump               = 0x20;     // VK_SPACE
    // Grab-gesture keys (synthetic "virtual slot" channel; companion mod
    // listens for these - docs/10. F1-F5 defaults; remap to taste).
    int   keyGrabHipL           = 0x70;     // VK_F1
    int   keyGrabHipR           = 0x71;     // VK_F2
    int   keyGrabShoulder       = 0x72;     // VK_F3
    int   keyGrabQuiver         = 0x73;     // VK_F4
    int   keyTwoHand            = 0x74;     // VK_F5
    int   keyHolster            = 0x75;     // VK_F6 (grip release in a zone)

    // [Tracking] (M4; pose source arrives with OpenXR in M3)
    bool  headTracking          = false;
    bool  trackingSmoothing     = true;
    float smoothingMinCutoff    = 1.5f;   // OneEuro min cutoff (Hz)
    float smoothingBeta         = 0.02f;  // OneEuro speed coefficient
    int   recenterKey           = 0x77;   // VK_F8 default
    bool  lockInMenus           = true;   // freeze tracking when view is static
    // Sign conventions (OpenXR right-handed -> D3D left-handed); flip without
    // a rebuild if a given runtime/game disagrees with these defaults.
    bool  invertYaw             = false;
    bool  invertPitch           = false;
    bool  invertRoll            = false;
    bool  invertPosX            = false;
    bool  invertPosZ            = true;   // OpenXR -Z fwd vs D3D +Z fwd

    // [Camera] (M4 true camera coupling; filled by M0-C)
    char  gameBuild[64]         = "";     // refuse camera writes if mismatch

    // [Debug]
    int   logLevel              = 2;
    int   logViewMatrices       = 25;
    int   captureMode           = 0;      // 0 off, 1 SetTransform, 2 VS consts
    int   dumpFrames            = 0;        // dump stereo surface of first N presents to BMP
    bool  noZTest               = false;  // diagnostic: disable depth testing on eye draws
    bool  cameraScan            = false;
    float expectedPos[3]        = { 0, 0, 0 };
    bool  hasExpectedPos        = false;
    unsigned long scanBase      = 0;      // 0 = auto (exe .data section)
    unsigned long scanSize      = 0;

    static Config& Get();
    void Load();   // call once at proxy init
};
