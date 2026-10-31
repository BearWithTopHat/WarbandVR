// SPDX-License-Identifier: GPL-3.0-only
#pragma once

// PointerMath: pure controller-ray -> logical UI coordinate mapping for
// laser-pointer menus. No Windows/D3D9/XR headers; host-testable.
//
// App space is OpenXR LOCAL space (+X right, +Y up, -Z forward); the view
// matrix maps it into the game's D3D view space (+Z forward). The ray-plane
// intersection is computed in view space at z = 1.0 m, then projected with
// the game's own projection matrix into full-window logical coordinates,
// which is exactly the space the game hit-tests in.

#include "Mat4.h"

namespace PointerMath
{
    // Rotate a vector by a quaternion (x,y,z,w), no normalization needed.
    inline void QuatRotate(float qx, float qy, float qz, float qw,
                           float vx, float vy, float vz, float* out)
    {
        // t = 2 * cross(q.xyz, v)
        const float tx = 2.0f * (qy * vz - qz * vy);
        const float ty = 2.0f * (qz * vx - qx * vz);
        const float tz = 2.0f * (qx * vy - qy * vx);
        // out = v + qw*t + cross(q.xyz, t)
        out[0] = vx + qw * tx + (qy * tz - qz * ty);
        out[1] = vy + qw * ty + (qz * tx - qx * tz);
        out[2] = vz + qw * tz + (qx * ty - qy * tx);
    }

    inline void TransformPoint(const Mat4& m, float x, float y, float z, float* out)
    {
        out[0] = m._11 * x + m._12 * y + m._13 * z + m._14;
        out[1] = m._21 * x + m._22 * y + m._23 * z + m._24;
        out[2] = m._31 * x + m._32 * y + m._33 * z + m._34;
    }

    inline void TransformDir(const Mat4& m, float x, float y, float z, float* out)
    {
        out[0] = m._11 * x + m._12 * y + m._13 * z;
        out[1] = m._21 * x + m._22 * y + m._23 * z;
        out[2] = m._31 * x + m._32 * y + m._33 * z;
    }

    // Controller pose in app space -> logical full-window coordinates.
    // Returns false when the ray does not point into the view plane
    // (dir.z <= 0, i.e. away from the scene).
    inline bool ComputePointerLogical(const float* handPos, const float* handQuat,
                                      const Mat4& view, const Mat4& proj,
                                      int eyeW, int eyeH, int* outX, int* outY)
    {
        float origin[3];
        TransformPoint(view, handPos[0], handPos[1], handPos[2], origin);
        // Grip forward is -Z in OpenXR grip space; rotate it by the hand's
        // orientation, then by the view matrix.
        float fwd[3];
        QuatRotate(handQuat[0], handQuat[1], handQuat[2], handQuat[3],
                   0.0f, 0.0f, -1.0f, fwd);
        float dir[3];
        TransformDir(view, fwd[0], fwd[1], fwd[2], dir);
        if (dir[2] <= 1e-6f) return false;

        const float t = 1.0f / dir[2];
        const float px = origin[0] + t * dir[0];
        const float py = origin[1] + t * dir[1];
        // z = 1.0 m in view space, so ndc.x = px*_11 + _31, ndc.y = py*_22.
        const float ndcX = px * proj._11 + proj._31;
        const float ndcY = py * proj._22;
        float x = (ndcX + 1.0f) * 0.5f * eyeW;
        float y = (1.0f - ndcY) * 0.5f * eyeH;
        if (x < 0.0f) x = 0.0f; else if (x > eyeW) x = static_cast<float>(eyeW);
        if (y < 0.0f) y = 0.0f; else if (y > eyeH) y = static_cast<float>(eyeH);
        *outX = static_cast<int>(x);
        *outY = static_cast<int>(y);
        return true;
    }
}
