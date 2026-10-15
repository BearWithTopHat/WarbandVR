// SPDX-License-Identifier: GPL-3.0-only
#pragma once

// GestureZones: pure grab-zone classification (head-relative boxes in app
// space), extracted from MotionInput.cpp so it is host-compilable and
// unit-tested headless. No Windows/D3D9/XR headers.
//
// App-space axes: +X right, +Y up, -Z forward (so +dz means BEHIND the
// head plane). Distances tuned for an average body; adjustable by gesture
// tuning rather than rebuild.

#include <cmath>

enum GrabZone { ZONE_NONE, ZONE_HIP_L, ZONE_HIP_R, ZONE_SHOULDER, ZONE_QUIVER };

// hand: 0 = left, 1 = right. hp = hand position, head = head position (xyz).
inline GrabZone ClassifyZone(int hand, const float* hp, const float* head)
{
    const int LEFT = 0, RIGHT = 1;
    float dx = hp[0] - head[0];
    float dy = hp[1] - head[1];
    float dz = hp[2] - head[2];
    // hip: below chest, slightly out to the hand's side, in front-ish
    if (dy > -0.50f && dy < -0.18f && fabsf(dz) < 0.30f)
    {
        if (hand == LEFT  && dx < -0.08f && dx > -0.40f) return ZONE_HIP_L;
        if (hand == RIGHT && dx >  0.08f && dx <  0.40f) return ZONE_HIP_R;
    }
    // over-shoulder: above shoulder line and behind the head plane
    if (dy > -0.15f && dy < 0.25f && dz > 0.10f && dz < 0.45f)
        return ZONE_SHOULDER;
    // quiver: right-back, chest height (arrow grab for bows)
    if (hand == RIGHT && dy > -0.35f && dy < -0.05f && dz > 0.08f && dz < 0.40f && dx > 0.05f)
        return ZONE_QUIVER;
    return ZONE_NONE;
}
