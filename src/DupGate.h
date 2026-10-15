// SPDX-License-Identifier: GPL-3.0-only
#pragma once

// DupGate: pure duplication-gate decision, extracted from DeviceHooks'
// DrawBothEyes so the policy is host-compilable and unit-tested headless.
// No Windows/D3D9/XR headers.
//
// Callers guarantee stereo is on (Config.EnableStereo && renderer active)
// before consulting this; the decision below is purely "given this draw,
// what do we do with it".

enum class DupDecision
{
    Skip,            // samples a diverted surface - drawing it would paint an
                     // empty surface over the frame
    MonoPassThrough, // game's own offscreen RT (shadow map etc.) - stock path
    FlatBothHalves,  // tiny UI/composite batch: draw into both eyes with no
                     // eye shift (SBSMonoUI)
    StereoBothEyes,  // world geometry: full per-eye duplication with shift
};

inline DupDecision DecideDuplication(bool divertedTextureBound,
                                     bool currentRTIsMain,
                                     bool shadowPassActive,
                                     bool sbsMonoUI,
                                     int primCount,
                                     int uiMaxTriangles)
{
    if (divertedTextureBound)
        return DupDecision::Skip;
    // An offscreen shadow pass is not main-sized, but when explicitly enabled
    // we deliberately duplicate it into per-eye shadow targets.
    if (!currentRTIsMain && !shadowPassActive)
        return DupDecision::MonoPassThrough;
    if (shadowPassActive)
        return DupDecision::StereoBothEyes;
    if (sbsMonoUI && primCount > 0 && primCount <= uiMaxTriangles)
        return DupDecision::FlatBothHalves;
    return DupDecision::StereoBothEyes;
}
