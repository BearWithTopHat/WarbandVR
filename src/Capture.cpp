#include "Capture.h"
#include "Config.h"
#include "Logger.h"
#include "MathUtil.h"

#include <cstring>
#include <cmath>

namespace
{
    unsigned Fnv1a(const void* data, size_t size)
    {
        const unsigned char* p = static_cast<const unsigned char*>(data);
        unsigned h = 2166136261u;
        for (size_t i = 0; i < size; ++i) { h ^= p[i]; h *= 16777619u; }
        return h;
    }

    // Heuristic classification of 16 floats as a transform matrix, in both
    // row-major (CPU layout) and transposed (column-major HLSL upload)
    // arrangements - games upload matrices in both, and a transposed matrix
    // evades the row-major tests entirely.
    enum MatrixKind { MK_None, MK_ViewLike, MK_ProjLike, MK_ViewLikeT, MK_ProjLikeT, MK_Other };

    MatrixKind ClassifyMatrix(const float* f)
    {
        for (int i = 0; i < 16; ++i)
            if (!std::isfinite(f[i])) return MK_None;

        const float f3 = f[3], f7 = f[7], f11 = f[11], f15 = f[15];
        // Row-major affine (view/world): last COLUMN ~ [0,0,0,1]
        if (std::fabs(f3) < 1e-4f && std::fabs(f7) < 1e-4f &&
            std::fabs(f11) < 1e-4f && std::fabs(f15 - 1.0f) < 1e-3f)
            return MK_ViewLike;
        // Row-major D3D perspective projection: _34 = 1, _44 = 0
        if (std::fabs(f11 - 1.0f) < 1e-3f && std::fabs(f15) < 1e-4f)
            return MK_ProjLike;
        // Transposed affine: last ROW ~ [0,0,0,1] (translation sits in f3/f7/f11)
        if (std::fabs(f[12]) < 1e-4f && std::fabs(f[13]) < 1e-4f &&
            std::fabs(f[14]) < 1e-4f && std::fabs(f15 - 1.0f) < 1e-3f)
            return MK_ViewLikeT;
        // Transposed D3D perspective projection: _43 = 1, _44 = 0
        if (std::fabs(f[14] - 1.0f) < 1e-3f && std::fabs(f15) < 1e-4f)
            return MK_ProjLikeT;
        return MK_Other;
    }

    bool IsMatrixLike(MatrixKind k)
    {
        return k == MK_ViewLike || k == MK_ProjLike || k == MK_ViewLikeT || k == MK_ProjLikeT;
    }

    const char* KindName(MatrixKind k)
    {
        switch (k)
        {
        case MK_ViewLike:  return "view";
        case MK_ProjLike:  return "proj";
        case MK_ViewLikeT: return "viewT";
        case MK_ProjLikeT: return "projT";
        default:           return "?";
        }
    }

    struct FrameStats
    {
        int setTransformView = 0;
        int setTransformProj = 0;
        unsigned lastViewSum = 0;
        unsigned lastProjSum = 0;
        int vsWritesTotal = 0;

        void Reset()
        {
            setTransformView = setTransformProj = 0;
            vsWritesTotal = 0;
        }
    };

    FrameStats g_stats;
    unsigned long long g_frame = 0;

    // Shadow copy of all 256 VS float4 registers (4 KB), plus the frame each
    // register was last written. Lets us classify matrices regardless of how
    // the game chunks its SetVertexShaderConstantF calls (a 4x4 written as
    // four single-register writes is invisible to per-call classification).
    float g_vsShadow[256 * 4] = {};
    unsigned long long g_vsWritten[256] = {};
    bool g_vsSeeded = false;

    // Rank aligned 4-register windows by matrix-likeness and freshness.
    // Returns the count of candidates written into `out` (capacity 8).
    struct Candidate { int base; MatrixKind kind; unsigned sum; bool fresh; };
    int RankCandidates(Candidate* out, int capacity)
    {
        int n = 0;
        for (int base = 0; base <= 252; base += 4)
        {
            MatrixKind kind = ClassifyMatrix(g_vsShadow + base * 4);
            if (!IsMatrixLike(kind)) continue;
            bool fresh = false;
            for (int r = base; r < base + 4; ++r)
                if (g_vsWritten[r] == g_frame) { fresh = true; break; }
            if (n < capacity)
            {
                out[n].base = base;
                out[n].kind = kind;
                out[n].sum  = Fnv1a(g_vsShadow + base * 4, 16 * sizeof(float));
                out[n].fresh = fresh;
                ++n;
            }
        }
        // Fresh (written this frame) candidates first; stable otherwise.
        for (int i = 0; i < n; ++i)
            for (int j = i + 1; j < n; ++j)
                if (out[j].fresh && !out[i].fresh)
                { Candidate t = out[i]; out[i] = out[j]; out[j] = t; }
        return n;
    }
}

void Capture::BeginFrame()
{
    if (Config::Get().captureMode != 0)
        g_stats.Reset();
}

void Capture::OnSetTransform(D3DTRANSFORMSTATETYPE state, const D3DMATRIX* m)
{
    if (Config::Get().captureMode != 1 || !m) return;
    if (state == D3DTS_VIEW)
    {
        ++g_stats.setTransformView;
        g_stats.lastViewSum = Fnv1a(m, sizeof(D3DMATRIX));
    }
    else if (state == D3DTS_PROJECTION)
    {
        ++g_stats.setTransformProj;
        g_stats.lastProjSum = Fnv1a(m, sizeof(D3DMATRIX));
    }
}

void Capture::OnSetVSConstantF(UINT startRegister, const float* data, UINT count4)
{
    const int mode = Config::Get().captureMode;
    if (mode < 2 || !data || startRegister > 255) return;

    g_stats.vsWritesTotal += count4;

    const UINT end = (startRegister + count4 > 256) ? 256 : startRegister + count4;
    for (UINT r = startRegister; r < end; ++r)
    {
        memcpy(g_vsShadow + r * 4, data + (r - startRegister) * 4, 4 * sizeof(float));
        g_vsWritten[r] = g_frame + 1; // frame counter increments in EndFrame
    }
    g_vsSeeded = true;
}

void Capture::EndFrame()
{
    const int mode = Config::Get().captureMode;
    if (mode == 0) return;
    ++g_frame;

    if (mode == 1)
    {
        LOG_INFO("[CAPTURE f=%llu] SetTransform: VIEW x%d (sum %08X), PROJ x%d (sum %08X)",
                 g_frame, g_stats.setTransformView, g_stats.lastViewSum,
                 g_stats.setTransformProj, g_stats.lastProjSum);
        // Interpretation aid, once after ~2s of frames.
        if (g_frame == 120)
        {
            if (g_stats.setTransformView > 0)
                LOG_INFO("[CAPTURE] D3DTS_VIEW is being written per frame -> try ViewSource=0");
            else
                LOG_INFO("[CAPTURE] No D3DTS_VIEW writes seen -> engine is shader-driven, run CaptureMode=2");
        }
        return;
    }

    if (!g_vsSeeded)
    {
        if ((g_frame % 60) == 0)
            LOG_INFO("[CAPTURE f=%llu] no VS constant writes yet (in a 3D scene?)", g_frame);
        return;
    }

    Candidate cand[8];
    int n = RankCandidates(cand, 8);

    if (mode == 2)
    {
        // Compact per-frame ranking of matrix-like register windows.
        LOG_INFO("[CAPTURE f=%llu] VS writes=%d; matrix-like windows:", g_frame, g_stats.vsWritesTotal);
        for (int i = 0; i < n; ++i)
            LOG_INFO("  c%-3d %-5s sum=%08X%s", cand[i].base, KindName(cand[i].kind), cand[i].sum,
                     cand[i].fresh ? " (written this frame)" : "");
        if (n == 0)
            LOG_INFO("  (none)");
        return;
    }

    // mode 3: periodic full dump of candidate windows for manual analysis.
    if ((g_frame % 60) != 0) return;
    LOG_INFO("[CAPTURE f=%llu] VS writes=%d; dumping %d matrix-like window(s):",
             g_frame, g_stats.vsWritesTotal, n);
    for (int i = 0; i < n; ++i)
    {
        char buf[640];
        D3DMATRIX m;
        memcpy(&m, g_vsShadow + cand[i].base * 4, sizeof(m));
        Mat::Format(m, buf, sizeof(buf));
        LOG_INFO("  c%-3d %-5s%s :%s", cand[i].base, KindName(cand[i].kind),
                 cand[i].fresh ? " fresh" : "", buf);
    }
    if (n == 0)
        LOG_INFO("  (none)");
}
