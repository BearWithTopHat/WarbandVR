#pragma once

// Minimal D3DMATRIX helpers (D3DXMATRIX equivalents without the D3DX
// dependency). All matrices are row-major, matching D3D9 convention.

#include <d3d9.h>
#include <cmath>
#include <cstdio>

namespace Mat
{
    inline D3DMATRIX Identity()
    {
        D3DMATRIX m = {};
        m._11 = m._22 = m._33 = m._44 = 1.0f;
        return m;
    }

    inline D3DMATRIX Translation(float x, float y, float z)
    {
        D3DMATRIX m = Identity();
        m._41 = x; m._42 = y; m._43 = z;
        return m;
    }

    // Row-major multiply: result = a * b (apply a, then b)
    inline D3DMATRIX Multiply(const D3DMATRIX& a, const D3DMATRIX& b)
    {
        D3DMATRIX r;
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
    inline D3DMATRIX EyeView(const D3DMATRIX& headView, float halfSep, int eye /*0=left,1=right*/)
    {
        float sign = (eye == 0) ? +1.0f : -1.0f;
        return Multiply(Translation(sign * halfSep, 0.0f, 0.0f), headView);
    }

    // Off-center (asymmetric) projection for one eye, derived from the game's
    // own symmetric perspective projection. Avoids the toe-in distortion of a
    // pure view shift. Falls back to the unmodified matrix for orthographic or
    // unrecognised projections (can't shift what we can't parse).
    inline D3DMATRIX EyeProjection(const D3DMATRIX& proj, float halfSep, int eye)
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
        D3DMATRIX out = proj;
        out._31 = -sign * halfSep * proj._11 / nearPlane;
        return out;
    }

    // Yaw(Y)-pitch(X)-roll(Z) rotation, left-handed row-major (D3D convention).
    inline D3DMATRIX RotationYPR(float yaw, float pitch, float roll)
    {
        float cy = std::cos(yaw),  sy = std::sin(yaw);
        float cx = std::cos(pitch), sx = std::sin(pitch);
        float cz = std::cos(roll),  sz = std::sin(roll);

        D3DMATRIX ry = Identity(); ry._11 = cy;  ry._13 = sy;  ry._31 = -sy; ry._33 = cy;
        D3DMATRIX rx = Identity(); rx._22 = cx;  rx._23 = -sx; rx._32 = sx;  rx._33 = cx;
        D3DMATRIX rz = Identity(); rz._11 = cz;  rz._12 = -sz; rz._21 = sz;  rz._22 = cz;
        return Multiply(Multiply(ry, rx), rz); // yaw, then pitch, then roll
    }

    // Transpose of the 3x3 part (== inverse for pure rotations).
    inline D3DMATRIX Transpose3x3(const D3DMATRIX& m)
    {
        D3DMATRIX r = Identity();
        r._11 = m._11; r._12 = m._21; r._13 = m._31;
        r._21 = m._12; r._22 = m._22; r._23 = m._32;
        r._31 = m._13; r._32 = m._23; r._33 = m._33;
        return r;
    }

    inline void Format(const D3DMATRIX& m, char* out, size_t outSize)
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
}
