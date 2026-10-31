// SPDX-License-Identifier: GPL-3.0-only
#pragma once

// MeleeCore: pure swing-classification and steering math, extracted from
// MotionInput.cpp so it is host-compilable and unit-tested headless
// (tests/test_main.cpp). No Windows/D3D9/XR headers.
//
// Axis convention: OpenXR application (LOCAL) space - +X right, +Y up,
// yaw=0 faces -Z. Screen/mouse space: +x right, +y DOWN.

#include <cmath>

namespace Melee
{
    enum AttackDir { DIR_NONE = -1, DIR_LEFT, DIR_RIGHT, DIR_OVERHEAD, DIR_THRUST };

    struct Sample { float vx, vy, vz; unsigned long long tMs; };

    inline float Speed(float vx, float vy, float vz)
    {
        return sqrtf(vx * vx + vy * vy + vz * vz);
    }

    // Rotate app-space velocity into head/view space (yaw only is enough for
    // screen-plane steering; pitch/roll barely matter for mouse deltas).
    // OpenXR: yaw=0 faces -Z, right = +X.
    inline void VelocityToView(const float* v, float headYaw, float* outRight, float* outUp)
    {
        float cs = cosf(headYaw), sn = sinf(headYaw);
        // head right vector = (cos yaw, 0, -sin yaw); up = (0,1,0)
        *outRight = v[0] * cs - v[2] * sn;
        *outUp    = v[1];
    }

    // Classify the dominant axis of a peak-velocity sample into one of
    // Warband's 4 attack directions (app space: +x right, +y up, -z fwd).
    // Upward-dominant motion has no matching attack -> DIR_NONE.
    inline AttackDir ClassifySwing(float vx, float vy, float vz)
    {
        float ax = fabsf(vx), ay = fabsf(vy), az = fabsf(vz);
        if (ay >= ax && ay >= az)
            return (vy < 0) ? DIR_OVERHEAD : DIR_NONE; // downward = overhead chop
        if (ax >= az)
            return (vx < 0) ? DIR_LEFT : DIR_RIGHT;
        return DIR_THRUST; // forward motion = stab
    }

    // Peak-velocity sample within a time window over a ring of recent
    // samples. Returns false if no sample in the window exceeds minSpeed.
    inline bool FindPeak(const Sample* ring, int ringSize,
                         unsigned long long now, unsigned long long windowMs,
                         float minSpeed, Sample* out)
    {
        float best = 0.0f;
        Sample peak{};
        for (int i = 0; i < ringSize; ++i)
        {
            const Sample& sm = ring[i];
            if (sm.tMs == 0 || now - sm.tMs > windowMs) continue;
            float sp = Speed(sm.vx, sm.vy, sm.vz);
            if (sp > best) { best = sp; peak = sm; }
        }
        if (best < minSpeed) return false;
        if (out) *out = peak;
        return true;
    }

    // Why a held attack should release, or RELEASE_NONE to keep steering.
    enum ReleaseReason
    {
        RELEASE_NONE,
        RELEASE_STEER_DISABLED_OR_TIMEOUT, // MotionSteer=0 or SteerMaxHoldMs
        RELEASE_LOST_TRACKING,             // hand velocity unavailable
        RELEASE_HAND_STOPPED,              // speed collapsed after min hold
    };

    inline ReleaseReason SteerRelease(bool motionSteer,
                                      unsigned long long heldMs,
                                      int steerMaxHoldMs,
                                      bool velocityValid,
                                      float speed,
                                      float swingThreshold)
    {
        if (!motionSteer || heldMs > (unsigned long long)steerMaxHoldMs)
            return RELEASE_STEER_DISABLED_OR_TIMEOUT;
        if (!velocityValid)
            return RELEASE_LOST_TRACKING;
        if (speed < swingThreshold * 0.5f && heldMs > 120)
            return RELEASE_HAND_STOPPED;
        return RELEASE_NONE;
    }

    // View-space hand velocity -> mouse deltas. Screen Y is down. Deltas are
    // clamped to +/- maxDelta px per poll: a tracking spike or an unusually
    // fast gesture must not whip the held attack across the screen in a
    // single frame (Warband's held-attack steering was tuned for mouse-rate
    // deltas, not VR head/hand rates).
    inline void SteerDelta(float vRight, float vUp, float steerScale, int maxDelta,
                           int* dx, int* dy)
    {
        int cx = (int)(vRight * steerScale);
        int cy = (int)(-vUp * steerScale);
        if (cx >  maxDelta) cx =  maxDelta; else if (cx < -maxDelta) cx = -maxDelta;
        if (cy >  maxDelta) cy =  maxDelta; else if (cy < -maxDelta) cy = -maxDelta;
        *dx = cx; *dy = cy;
    }

    // Distance between the two controllers in app space (metres). Used by
    // the physical two-hand bow draw.
    inline float HandDistance(const float* left, const float* right)
    {
        const float dx = left[0] - right[0];
        const float dy = left[1] - right[1];
        const float dz = left[2] - right[2];
        return sqrtf(dx * dx + dy * dy + dz * dz);
    }

    // Initial direction delta injected with the attack press (px). This is
    // Warband's direction-COMMIT distance, deliberately NOT covered by the
    // SteerDelta clamp: the clamp bounds continuous steering per poll, while
    // this fixed 80 px nudge is what makes the engine pick the intended
    // attack direction at press time.
    inline void BeginDelta(AttackDir dir, int* dx, int* dy)
    {
        *dx = 0; *dy = 0;
        switch (dir)
        {
        case DIR_LEFT:     *dx = -80; break;
        case DIR_RIGHT:    *dx =  80; break;
        case DIR_OVERHEAD: *dy = -80; break;
        case DIR_THRUST:   *dy =  80; break;
        default: break;
        }
    }
}
