// SPDX-License-Identifier: GPL-3.0-only
#pragma once

// Capture: M0 reconnaissance tooling (task M0-1), built in from the start.
// Answers "how does Warband deliver its per-frame view matrix?" from the
// proxy's own logs - no external tool required (RenderDoc still recommended
// for confirmation; docs/04 has the full protocol).
//
// Driven by [Debug] CaptureMode:
//   0 = off (zero overhead beyond one branch)
//   1 = aggregate SetTransform VIEW/PROJECTION calls per frame
//   2 = per-frame ranking of VS register windows holding matrix-like data
//       (shadows all 256 registers, so chunked writes are caught too)
//   3 = like 2, plus a full dump of candidate matrices every 60 frames

#include <d3d9.h>

namespace Capture
{
    void BeginFrame();
    void OnSetTransform(D3DTRANSFORMSTATETYPE state, const D3DMATRIX* m);
    void OnSetVSConstantF(UINT startRegister, const float* data, UINT count4);
    void EndFrame();

    // CaptureMode=4: full event trace for one frame every 1200 frames.
    // No-op otherwise; call sites are the device hooks.
    void Trace(const char* fmt, ...);
    bool TraceActive();
}
