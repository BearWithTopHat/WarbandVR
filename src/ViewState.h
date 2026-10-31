// SPDX-License-Identifier: GPL-3.0-only
#pragma once

// ViewState: the current captured view/projection and the last-view-change
// timestamp, shared between StereoRenderer (writer) and MotionInput (reader
// for laser-pointer math). Inline globals keep this header-only with no
// additional link dependencies.

#include <d3d9.h>

namespace ViewState
{
    inline D3DMATRIX g_view = {};
    inline D3DMATRIX g_proj = {};
    inline bool      g_hasView = false;
    inline bool      g_hasProj = false;

    // Last time the captured view actually changed (menu/map heuristic).
    inline unsigned long long g_lastViewChangeTick = 0;

    // Current per-eye target size (set by StereoRenderer on creation/reset).
    inline UINT g_eyeW = 0;
    inline UINT g_eyeH = 0;
}
