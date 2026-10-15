// SPDX-License-Identifier: GPL-3.0-only
// Deterministic reset/robustness fuzzing over the real hooks and renderer.
// The fake device injects HRESULT failures; assertions cover lifecycle
// invariants rather than requiring any particular call sequence.

#include "windows.h"
#include "d3d9.h"
#include "fake_device.h"
#include "Config.h"
#include "StereoRenderer.h"
#include "DeviceHooks.h"

#include <cstdio>
#include <cstdint>

static int g_failures = 0, g_checks = 0;
#define CHECK(cond) do { ++g_checks; if (!(cond)) { \
    ++g_failures; printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); } } while (0)

namespace
{
    struct Rng
    {
        uint32_t s = 0x5EED1234u;
        uint32_t Next()
        {
            s ^= s << 13; s ^= s >> 17; s ^= s << 5;
            return s;
        }
        uint32_t Below(uint32_t n) { return n ? Next() % n : 0; }
    };

    D3DMATRIX Matrix(float x)
    {
        D3DMATRIX m{};
        m._11 = m._22 = m._33 = m._44 = 1.0f;
        m._41 = x;
        return m;
    }

    bool KnownRT(IDirect3DSurface9* rt, IDirect3DSurface9* a,
                 IDirect3DSurface9* b, IDirect3DSurface9* c)
    {
        return !rt || rt == a || rt == b || rt == c ||
               rt == StereoRenderer::Get().EyeSurface(0) ||
               rt == StereoRenderer::Get().EyeSurface(1);
    }

    void CheckInvariants(FakeDevice& dev, IDirect3DSurface9* bb,
                         IDirect3DSurface9* off, IDirect3DSurface9* main2)
    {
        StereoRenderer& sr = StereoRenderer::Get();
        if (sr.IsActive())
        {
            CHECK(sr.EyeSurface(0) != nullptr);
            CHECK(sr.EyeSurface(1) != nullptr);
            CHECK(sr.EyeWidth() != 0 && sr.EyeHeight() != 0);
        }
        CHECK(KnownRT(dev.rt[0], bb, off, main2));
        CHECK(dev.presentCount >= 0);
    }
}

int main()
{
    Config& cfg = Config::Get();
    cfg.enableStereo = true;
    cfg.outputMode = 0;
    cfg.dumpFrames = 0;
    cfg.logLevel = 0;
    cfg.sbsMonoUI = true;
    cfg.uiMaxTriangles = 64;
    cfg.ipd = 0.064f;
    cfg.worldScale = 1.0f;
    cfg.offscreenPasses = false;
    cfg.sharedSurface = false;
    cfg.viewSource = 0;
    cfg.headTracking = false;

    FakeDevice dev(320, 240);
    FakeSurface bb(320, 240), off(64, 64), main2(320, 240);
    StereoRenderer::Get().OnDeviceCreated(&dev, &dev.pp);
    DeviceHooks::Get().OnDeviceCreated(&dev, &dev.pp);
    hkSetRenderTarget(&dev, 0, &bb);
    hkBeginScene(&dev);

    Rng rng;
    for (int i = 0; i < 800; ++i)
    {
        const uint32_t op = rng.Below(14);

        // Sparse deterministic HRESULT failures.
        dev.drawResult = (rng.Below(41) == 0) ? E_FAIL : S_OK;
        dev.presentResult = (rng.Below(53) == 0) ? E_FAIL : S_OK;
        dev.beginSceneResult = (rng.Below(59) == 0) ? E_FAIL : S_OK;
        dev.endSceneResult = (rng.Below(61) == 0) ? E_FAIL : S_OK;
        dev.clearResult = (rng.Below(47) == 0) ? E_FAIL : S_OK;

        switch (op)
        {
        case 0:
            hkSetRenderTarget(&dev, 0, &bb);
            break;
        case 1:
            hkSetRenderTarget(&dev, 0, &off);
            break;
        case 2:
            hkSetRenderTarget(&dev, 0, &main2);
            break;
        case 3:
        {
            D3DMATRIX m = Matrix((float)(rng.Below(21) - 10) * 0.1f);
            hkSetTransform(&dev, D3DTS_VIEW, &m);
            break;
        }
        case 4:
        {
            D3DMATRIX m = Matrix(0.25f);
            Config::Get().viewSource = 1;
            hkSetVertexShaderConstantF(&dev, 0, reinterpret_cast<const float*>(&m), 4);
            Config::Get().viewSource = 0;
            break;
        }
        case 5:
        {
            D3DVIEWPORT9 vp{0, 0, 160 + rng.Below(161), 120 + rng.Below(121), 0.0f, 1.0f};
            hkSetViewport(&dev, &vp);
            break;
        }
        case 6:
        {
            RECT r{0, 0, (LONG)(1 + rng.Below(320)), (LONG)(1 + rng.Below(240))};
            hkSetRenderState(&dev, D3DRS_SCISSORTESTENABLE, rng.Below(2));
            hkSetScissorRect(&dev, &r);
            break;
        }
        case 7:
            hkDrawPrimitive(&dev, D3DPT_TRIANGLELIST, 0, rng.Below(201));
            break;
        case 8:
            hkDrawIndexedPrimitive(&dev, D3DPT_TRIANGLELIST, 0, 0, 4, 0, rng.Below(201));
            break;
        case 9:
            hkDrawPrimitiveUP(&dev, D3DPT_TRIANGLELIST, rng.Below(201), nullptr, 0);
            break;
        case 10:
            hkClear(&dev, 0, nullptr, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, 0, 1.0f, 0);
            break;
        case 11:
            hkPresent(&dev, nullptr, nullptr, nullptr, nullptr);
            break;
        case 12:
            hkEndScene(&dev);
            hkBeginScene(&dev);
            break;
        case 13:
        {
            D3DPRESENT_PARAMETERS pp = dev.pp;
            static const UINT sizes[4][2] = {{320,240},{640,480},{800,600},{1600,900}};
            const uint32_t pick = rng.Below(4);
            pp.BackBufferWidth = sizes[pick][0];
            pp.BackBufferHeight = sizes[pick][1];
            dev.resetResult = (rng.Below(5) == 0) ? E_FAIL : S_OK;
            if (dev.resetResult == S_OK && rng.Below(3) == 0)
                dev.failCreateRenderTargetCalls = 1;
            if (dev.resetResult == S_OK && rng.Below(3) == 0)
                dev.failCreateDepthStencilCalls = 1;
            hkReset(&dev, &pp);
            dev.resetResult = S_OK;
            dev.drawResult = S_OK;
            hkSetRenderTarget(&dev, 0, &bb); // fake bb object remains valid across Reset
            hkBeginScene(&dev);
            break;
        }
        }

        Config::Get().offscreenPasses = (rng.Below(5) == 0);
        CheckInvariants(dev, &bb, &off, &main2);
    }

    // Final cleanup must be safe after any mixed success/failure stream.
    hkPresent(&dev, nullptr, nullptr, nullptr, nullptr);
    StereoRenderer::Get().Shutdown();
    DeviceHooks::Get().Shutdown();
    CHECK(StereoRenderer::Get().EyeSurface(0) == nullptr);
    CHECK(!StereoRenderer::Get().IsActive());

    printf("fuzz: %d checks, %d failures (800 iterations)\n", g_checks, g_failures);
    return g_failures ? 1 : 0;
}
