// SPDX-License-Identifier: GPL-3.0-only
#pragma once

// Mat4: pure row-major 4x4 matrix math, field-layout IDENTICAL to D3DMATRIX
// (row-major, D3D9 convention). No Windows/D3D9/XR headers - this unit is
// host-compilable and unit-tested headless (tests/test_main.cpp).
// Boundary conversions to/from D3DMATRIX live in MathUtil.h.

#include <cmath>
#include <cstdio>

struct Mat4
{
    float _11, _12, _13, _14;
    float _21, _22, _23, _24;
    float _31, _32, _33, _34;
    float _41, _42, _43, _44;
};

namespace MatPure
{
    inline Mat4 Identity()
    {
        Mat4 m = {};
        m._11 = m._22 = m._33 = m._44 = 1.0f;
        return m;
    }

    inline Mat4 Translation(float x, float y, float z)
    {
        Mat4 m = Identity();
        m._41 = x; m._42 = y; m._43 = z;
        return m;
    }

    // Row-major multiply: result = a * b (apply a, then b)
    inline Mat4 Multiply(const Mat4& a, const Mat4& b)
    {
        Mat4 r;
        const float* A = &a._11;
        const float* B = &b._11;
        float* R = &r._11;
        for (int i = 0; i < 4; ++i)
            for (int j = 0; j < 4; ++j)
            {
                float s = 0.0f;
                for (int k = 0; k < 4; ++k)
                    s += A[i * 4 + k] * B[k * 4 + j];
                R[i * 4 + j] = s;
            }
        return r;
    }

    // view_eye = T(+/- halfIPD, 0, 0) * view_head
    // (camera_eye sits at -/+ halfIPD in head space; the view matrix is its inverse)
    inline Mat4 EyeView(const Mat4& headView, float halfSep, int eye /*0=left,1=right*/)
    {
        float sign = (eye == 0) ? +1.0f : -1.0f;
        return Multiply(Translation(sign * halfSep, 0.0f, 0.0f), headView);
    }

    // Off-center (asymmetric) projection for one eye, derived from the game's
    // own symmetric perspective projection. Avoids the toe-in distortion of a
    // pure view shift. Falls back to the unmodified matrix for orthographic or
    // unrecognised projections (can't shift what we can't parse).
    inline Mat4 EyeProjection(const Mat4& proj, float halfSep, int eye)
    {
        // Row-major D3D perspective projection has _34 == 1 and _44 == 0.
        const float* f = &proj._11;
        bool isPerspective = std::fabs(f[11] - 1.0f) < 1e-3f && std::fabs(f[15]) < 1e-4f;
        if (!isPerspective)
            return proj; // orthographic or unknown: shift not applicable

        // Recover near plane: _33 = f/(f-n) and _43 = -n*_33  =>  n = -_43/_33
        if (std::fabs(proj._33) < 1e-6f)
            return proj;
        float nearPlane = -proj._43 / proj._33;
        if (nearPlane <= 0.0f)
            return proj;

        // Shift the frustum horizontally by the eye offset at the near plane:
        //   _31' = -/+ halfSep * _11 / n
        float sign = (eye == 0) ? +1.0f : -1.0f;
        Mat4 out = proj;
        out._31 = -sign * halfSep * proj._11 / nearPlane;
        return out;
    }

    // Yaw(Y)-pitch(X)-roll(Z) rotation, left-handed row-major (D3D convention).
    inline Mat4 RotationYPR(float yaw, float pitch, float roll)
    {
        float cy = std::cos(yaw),  sy = std::sin(yaw);
        float cx = std::cos(pitch), sx = std::sin(pitch);
        float cz = std::cos(roll),  sz = std::sin(roll);

        Mat4 ry = Identity(); ry._11 = cy;  ry._13 = sy;  ry._31 = -sy; ry._33 = cy;
        Mat4 rx = Identity(); rx._22 = cx;  rx._23 = -sx; rx._32 = sx;  rx._33 = cx;
        Mat4 rz = Identity(); rz._11 = cz;  rz._12 = -sz; rz._21 = sz;  rz._22 = cz;
        return Multiply(Multiply(ry, rx), rz); // yaw, then pitch, then roll
    }

    // Transpose of the 3x3 part (== inverse for pure rotations).
    inline Mat4 Transpose3x3(const Mat4& m)
    {
        Mat4 r = Identity();
        r._11 = m._11; r._12 = m._21; r._13 = m._31;
        r._21 = m._12; r._22 = m._22; r._23 = m._32;
        r._31 = m._13; r._32 = m._23; r._33 = m._33;
        return r;
    }

    // Orthographic-UI heuristic: flat UI passes run with an ortho-style
    // matrix - tiny z-scale, an affine bottom row (w == 1), and NON-unit
    // 3x3 rows. The row-norm condition keeps steep-pitch 3D views (where
    // cos(pitch) shrinks _33 below the z cutoff) from being misclassified as
    // UI: rotations stay orthonormal, pixel-space ortho rows are ~2/width.
    // Thresholds are parameters because the right cutoff is game- and
    // shader-dependent.
    inline bool IsFlatOrtho(const Mat4& m, float zScaleMax, float wEps, float rowNormMax)
    {
        if (std::fabs(m._33) >= zScaleMax || std::fabs(m._44 - 1.0f) >= wEps)
            return false;
        const float* f = &m._11;
        for (int r = 0; r < 3; ++r)
        {
            const double n2 = static_cast<double>(f[r * 4 + 0]) * f[r * 4 + 0]
                            + static_cast<double>(f[r * 4 + 1]) * f[r * 4 + 1]
                            + static_cast<double>(f[r * 4 + 2]) * f[r * 4 + 2];
            if (n2 >= static_cast<double>(rowNormMax) * rowNormMax)
                return false;
        }
        return true;
    }

    // Plausibility check for a matrix read back from a shader constant
    // register believed to hold a view (or viewproj) transform. Accepts both
    // 3D views and flat orthographic UI matrices; rejects the garbage a WRONG
    // register guess produces (NaN/Inf, all-zero, absurd magnitudes). A
    // shifted garbage matrix per eye is worse than mono, so callers fall back
    // to unshifted rendering when this fails.
    inline bool IsPlausibleTransform(const Mat4& m)
    {
        const float* f = &m._11;
        double sumSq = 0.0;
        for (int i = 0; i < 16; ++i)
        {
            if (!std::isfinite(f[i])) return false;
            if (std::fabs(f[i]) > 1.0e7f) return false; // beyond any sane world extent
            sumSq += static_cast<double>(f[i]) * f[i];
        }
        return sumSq > 1.0e-12; // reject the all-zero register
    }

    inline void Format(const Mat4& m, char* out, size_t outSize)
    {
        snprintf(out, outSize,
            "\n  | %8.4f %8.4f %8.4f %8.4f |\n"
            "  | %8.4f %8.4f %8.4f %8.4f |\n"
            "  | %8.4f %8.4f %8.4f %8.4f |\n"
            "  | %8.4f %8.4f %8.4f %8.4f |",
            m._11, m._12, m._13, m._14,
            m._21, m._22, m._23, m._24,
            m._31, m._32, m._33, m._34,
            m._41, m._42, m._43, m._44);
        out[outSize - 1] = '\0';
    }
} // namespace MatPure
