#pragma once

// Capture: M0 reconnaissance tooling (task M0-1), built in from the start.
// Answers "how does Warband deliver its per-frame view matrix?" from the
// proxy's own logs - no external tool required (RenderDoc still recommended
// for confirmation; docs/04 has the full protocol).
//
// Driven by [Debug] CaptureMode:
//   0 = off (zero overhead beyond one branch)
//   1 = aggregate SetTransform VIEW/PROJECTION calls per frame
//   2 = aggregate SetVertexShaderConstantF writes per frame, with a
//       plausibility ranking of which registers hold view/proj matrices

#include <d3d9.h>

namespace Capture
{
    void BeginFrame();
    void OnSetTransform(D3DTRANSFORMSTATETYPE state, const D3DMATRIX* m);
    void OnSetVSConstantF(UINT startRegister, const float* data, UINT count4);
    void EndFrame();
}
