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

    // Heuristic classification of 16 floats as a transform matrix.
    enum MatrixKind { MK_None, MK_ViewLike, MK_ProjLike, MK_Other };

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
        return MK_Other;
    }

    struct FrameStats
    {
        int setTransformView = 0;
        int setTransformProj = 0;
        unsigned lastViewSum = 0;
        unsigned lastProjSum = 0;

        struct RegStat
        {
            int writes = 0;
            unsigned lastSum = 0;
            int viewLike = 0;
            int projLike = 0;
        };
        RegStat regs[256];
        int vsWritesTotal = 0;

        void Reset()
        {
            setTransformView = setTransformProj = 0;
            vsWritesTotal = 0;
            memset(regs, 0, sizeof(regs));
        }
    };

    FrameStats g_stats;
    unsigned long long g_frame = 0;
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
    if (Config::Get().captureMode != 2 || !data || startRegister > 255) return;

    g_stats.vsWritesTotal += count4;

    // A matrix write covers >= 4 consecutive float4 registers; credit each.
    for (UINT r = startRegister; r < startRegister + count4 && r < 256; ++r)
        ++g_stats.regs[r].writes;

    if (count4 >= 4)
    {
        MatrixKind kind = ClassifyMatrix(data);
        FrameStats::RegStat& s = g_stats.regs[startRegister];
        s.lastSum = Fnv1a(data, 16 * sizeof(float));
        if (kind == MK_ViewLike) ++s.viewLike;
        if (kind == MK_ProjLike) ++s.projLike;
    }
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

    // mode 2: dump the top candidate registers.
    int best[5] = { -1, -1, -1, -1, -1 };
    for (int r = 0; r < 256; ++r)
    {
        const FrameStats::RegStat& s = g_stats.regs[r];
        if (s.writes == 0 || (s.viewLike == 0 && s.projLike == 0)) continue;
        int score = s.viewLike * 2 + s.projLike + s.writes / 10;
        for (int i = 0; i < 5; ++i)
        {
            if (best[i] == -1) { best[i] = r; break; }
            const FrameStats::RegStat& b = g_stats.regs[best[i]];
            int bscore = b.viewLike * 2 + b.projLike + b.writes / 10;
            if (score > bscore) { for (int j = 4; j > i; --j) best[j] = best[j - 1]; best[i] = r; break; }
        }
    }

    LOG_INFO("[CAPTURE f=%llu] VS constant writes=%d; matrix-like candidates:", g_frame, g_stats.vsWritesTotal);
    for (int i = 0; i < 5 && best[i] != -1; ++i)
    {
        const FrameStats::RegStat& s = g_stats.regs[best[i]];
        LOG_INFO("  c%-3d writes=%-4d viewLike=%-3d projLike=%-3d sum=%08X",
                 best[i], s.writes, s.viewLike, s.projLike, s.lastSum);
    }
    if (best[0] == -1)
        LOG_INFO("  (none this frame)");
}
