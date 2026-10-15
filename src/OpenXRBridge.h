// SPDX-License-Identifier: GPL-3.0-only
#pragma once

// OpenXRBridge: submits the per-eye frames to an OpenXR runtime (M3) and
// provides the head-pose stream (M4) and action/space plumbing that
// MotionInput (M5) builds on.
//
// Two build modes:
//   - default (WBVR_WITH_OPENXR undefined): inert stubs; zero dependencies.
//   - WBVR_WITH_OPENXR defined (Release-OpenXR|Win32 configuration): the real
//     implementation. Requires the OpenXR SDK (Win32 loader + headers) and
//     links openxr_loader.lib, d3d11.lib, dxgi.lib.
//
// Frame submission is game-thread-coupled: StereoRenderer::OnPresent calls
// SubmitFrame once per game frame, which drives xrWaitFrame/BeginFrame/
// EndFrame synchronously. This implicitly paces the game to the HMD refresh
// when OutputMode=1. (An async submission thread is a documented post-M3
// optimization; see docs/01_Design.md.)
//
// Frame paths into the swapchains:
//   - slow (always available): GetRenderTargetData to system memory -> D3D11
//     staging texture -> CopySubresourceRegion into the swapchain image.
//   - fast ([Output] SharedSurface=1): eye targets created as D3D9Ex shared
//     textures (requires the Ex-steered device, see dllmain.cpp), opened in
//     D3D11 via OpenSharedResource and copied GPU-to-GPU.

#include <d3d9.h>

namespace OpenXRBridge
{
    // Lazily initialised on first SubmitFrame when OutputMode=1; also safe to
    // call explicitly. Returns false if no runtime/session is available
    // (caller should keep using SBS output).
    bool Init(IDirect3DDevice9* device);

    // Called once per game frame from StereoRenderer::OnPresent.
    // left/right are the per-eye game-sized render targets.
    void SubmitFrame(IDirect3DDevice9* device, IDirect3DSurface9* left, IDirect3DSurface9* right);

    void Shutdown();

    // M4: latest head pose as a delta from the recenter reference.
    // outYawPitchRoll / outXYZ are in the conventions consumed by
    // StereoRenderer::ApplyHeadDelta (radians, metres, sign conventions
    // tunable via [Tracking] Invert* ini keys). Returns false if no valid
    // pose has ever been sampled.
    bool GetHeadPoseDelta(float* outYawPitchRoll, float* outXYZ);

    // M4: capture the current pose as the neutral reference.
    void Recenter();

    // Comfort snap turn: rotate the pose reference so the view turns by the
    // given degrees without any engine-side state. Called by MotionInput
    // when the right stick crosses the snap threshold (SnapTurnDegrees > 0).
    void SnapTurn(float degrees);

    // M5 gestures: absolute head position in application space (metres).
    // MotionInput uses it to locate grab zones (hips/shoulders/quiver)
    // relative to the body. Returns false if no pose sampled yet.
    bool GetHeadPoseAbsolute(float* outXYZ);

    // M5 plumbing (MotionInput): opaque handles to the live session. All
    // return false/nullptr when the bridge isn't running.
    bool SessionRunning();
    void* XrInstanceHandle();  // XrInstance
    void* XrSessionHandle();   // XrSession
    void* XrAppSpaceHandle();  // XrSpace (LOCAL reference space)
    long long PredictedDisplayTime(); // XrTime of the current frame
}
