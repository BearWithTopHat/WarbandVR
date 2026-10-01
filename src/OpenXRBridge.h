#pragma once

// OpenXRBridge: submits the stereo frame to an OpenXR runtime (Milestone M3).
//
// This module is a STUB in the default build. It compiles in two modes:
//   - default (WBVR_WITH_OPENXR undefined): SubmitFrame does nothing, so the
//     project builds with zero external dependencies. Use OutputMode=0 (SBS
//     on monitor) for milestones M1/M2.
//   - WBVR_WITH_OPENXR defined + OpenXR SDK linked: the real implementation.
//
// Implementation guide lives in docs/01_Design.md section "OpenXR output" and
// docs/03_Agent_Prompts.md task M3 - they spell out exactly what to write
// here, based on the Khronos hello_xr sample (D3D11 binding).

#include <d3d9.h>

namespace OpenXRBridge
{
    // One-time init after the D3D9 device exists. Returns false if the
    // runtime is unavailable (fall back to SBS output).
    bool Init(IDirect3DDevice9* device);

    // Called from StereoRenderer::OnPresent with the 2W x H stereo surface
    // (left eye in the left half, right eye in the right half).
    void SubmitFrame(IDirect3DDevice9* device, IDirect3DSurface9* stereoSurface);

    void Shutdown();

    // Latest head pose as a view-space delta, for the M4 head-coupled camera.
    // Returns false until a real pose stream exists.
    bool GetHeadPoseDelta(float* outYawPitchRoll, float* outXYZ);
}
