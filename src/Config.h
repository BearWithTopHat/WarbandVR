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
    bool  vsMatrixIsViewProj    = false;
    bool  asymmetricProjection  = false;  // off-center frustum (M2 correctness)
    bool  sbsMonoUI             = true;   // draw tiny primitives once (UI)
    int   uiMaxTriangles        = 64;     // "tiny" threshold
    bool  offscreenPasses       = false;  // M2: per-eye shadows for the game's
                                          // own render passes (UNTESTED - keep
                                          // off until validated on-game)

    // [Output]
    int   outputMode            = 0;      // 0 = SBS monitor, 1 = OpenXR

    // [Tracking] (M4; pose source arrives with OpenXR in M3)
    bool  headTracking          = false;
    bool  trackingSmoothing     = true;
    float smoothingMinCutoff    = 1.5f;   // OneEuro min cutoff (Hz)
    float smoothingBeta         = 0.02f;  // OneEuro speed coefficient
    int   recenterKey           = 0x77;   // VK_F8 default
    bool  lockInMenus           = true;   // freeze tracking when view is static

    // [Camera] (M4 true camera coupling; filled by M0-C)
    char  gameBuild[64]         = "";     // refuse camera writes if mismatch

    // [Debug]
    int   logLevel              = 2;
    int   logViewMatrices       = 25;
    int   captureMode           = 0;      // 0 off, 1 SetTransform, 2 VS consts
    int   dumpFrames            = 500;      // dump stereo surface of first N presents to BMP
    bool  swapEyeViewports      = false;  // diagnostic: draw each eye into the opposite half
    bool  bothEyesLeft          = false;  // diagnostic: draw both eyes into the left half
    bool  noZTest               = false;  // diagnostic: disable depth testing on eye draws
    bool  cameraScan            = false;
    float expectedPos[3]        = { 0, 0, 0 };
    bool  hasExpectedPos        = false;
    unsigned long scanBase      = 0;      // 0 = auto (exe .data section)
    unsigned long scanSize      = 0;

    static Config& Get();
    void Load();   // call once at proxy init
};
