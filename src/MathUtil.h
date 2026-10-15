// SPDX-License-Identifier: GPL-3.0-only
#pragma once

// MathUtil: D3DMATRIX <-> Mat4 boundary conversions plus D3DMATRIX-typed
// convenience wrappers around the pure matrix core (Mat4.h, namespace
// MatPure - host-compilable, no Windows headers, unit-tested headless).
// This header is Windows/D3D9-bound by design; pure logic and tests use
// Mat4.h directly.

#include "Mat4.h"

#include <d3d9.h>
#include <cstring>

inline Mat4 MatFromD3D(const D3DMATRIX& m)
{
    Mat4 r;
    static_assert(sizeof(Mat4) == sizeof(D3DMATRIX), "layout must match");
    std::memcpy(&r, &m, sizeof(Mat4));
    return r;
}

inline D3DMATRIX MatToD3D(const Mat4& m)
{
    D3DMATRIX r;
    std::memcpy(&r, &m, sizeof(D3DMATRIX));
    return r;
}

namespace Mat
{
    inline D3DMATRIX Identity()    { return MatToD3D(MatPure::Identity()); }
    inline D3DMATRIX Translation(float x, float y, float z) { return MatToD3D(MatPure::Translation(x, y, z)); }
    inline D3DMATRIX Multiply(const D3DMATRIX& a, const D3DMATRIX& b)
    {
        return MatToD3D(MatPure::Multiply(MatFromD3D(a), MatFromD3D(b)));
    }
    inline D3DMATRIX EyeView(const D3DMATRIX& headView, float halfSep, int eye)
    {
        return MatToD3D(MatPure::EyeView(MatFromD3D(headView), halfSep, eye));
    }
    inline D3DMATRIX EyeProjection(const D3DMATRIX& proj, float halfSep, int eye)
    {
        return MatToD3D(MatPure::EyeProjection(MatFromD3D(proj), halfSep, eye));
    }
    inline D3DMATRIX RotationYPR(float yaw, float pitch, float roll)
    {
        return MatToD3D(MatPure::RotationYPR(yaw, pitch, roll));
    }
    inline D3DMATRIX Transpose3x3(const D3DMATRIX& m)
    {
        return MatToD3D(MatPure::Transpose3x3(MatFromD3D(m)));
    }
    inline bool IsFlatOrtho(const D3DMATRIX& m, float zScaleMax, float wEps, float rowNormMax)
    {
        return MatPure::IsFlatOrtho(MatFromD3D(m), zScaleMax, wEps, rowNormMax);
    }
    inline bool IsPlausibleTransform(const D3DMATRIX& m)
    {
        return MatPure::IsPlausibleTransform(MatFromD3D(m));
    }
    inline void Format(const D3DMATRIX& m, char* out, size_t outSize)
    {
        MatPure::Format(MatFromD3D(m), out, outSize);
    }
}
