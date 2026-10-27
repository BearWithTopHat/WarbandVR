// SPDX-License-Identifier: GPL-3.0-only
#include "DeviceHooks.h"
#include "StereoRenderer.h"
#include "Config.h"
#include "Logger.h"
#include "Capture.h"
#include "CameraScan.h"
#include "DupGate.h"

#include <vector>
#include <iterator>
#include <cstring>

// ---------------------------------------------------------------------------
// Shared detour machinery
// ---------------------------------------------------------------------------
namespace
{
    // 2D / fullscreen-quad path: draw into BOTH eye targets without any view
    // or projection shift. UI and the engine's composite quads stay flat and
    // identical in both eyes.
    template <typename F>
    HRESULT DrawMonoBothHalves(IDirect3DDevice9* dev, F&& draw)
    {
        IDirect3DSurface9* savedRT = nullptr;
        IDirect3DSurface9* savedDS = nullptr;
        dev->GetRenderTarget(0, &savedRT);
        dev->GetDepthStencilSurface(&savedDS);

        // D3D9 resets the viewport to the full new target on SetRenderTarget.
        // Dialog panels are drawn into sub-viewports (e.g. the encounter
        // dialog's option band), so the game's viewport must be re-applied
        // after each target swap or panel content smears across the screen.
        D3DVIEWPORT9 vp{};
        const bool hasVp = StereoRenderer::Get().HasGameViewport();
        if (hasVp) vp = StereoRenderer::Get().GameViewport();

        HRESULT hrL, hrR;
        {
            InternalScope g;
            dev->SetRenderTarget(0, StereoRenderer::Get().EyeSurface(0));
            if (hasVp) dev->SetViewport(&vp);
        }
        hrL = draw();
        {
            InternalScope g;
            dev->SetRenderTarget(0, StereoRenderer::Get().EyeSurface(1));
            if (hasVp) dev->SetViewport(&vp);
        }
        hrR = draw();

        {
            InternalScope g;
            dev->SetRenderTarget(0, savedRT);
            dev->SetDepthStencilSurface(savedDS);
            StereoRenderer::Get().RestoreGameState(dev);
        }
        if (savedRT) savedRT->Release();
        if (savedDS) savedDS->Release();
        return FAILED(hrL) ? hrL : hrR;
    }

    // Execute one game draw call for both eyes, each into its own game-sized
    // target. `draw` invokes the ORIGINAL. primCount drives the UI heuristic:
    // tiny batches (HUD text, icons, the engine's fullscreen composite quad)
    // are drawn per eye WITHOUT the eye view shift when SBSMonoUI is on, so
    // UI stays flat and identical in both eyes instead of getting stereo
    // separation.
    template <typename F>
    HRESULT DrawBothEyes(IDirect3DDevice9* dev, int primCount, F&& draw)
    {
        const Config& cfg = Config::Get();

        // Gate policy is pure (DupGate.h, unit-tested). Cases:
        //  Skip:            the draw samples a diverted surface (e.g. the
        //                   world texture feeding the final composite blit).
        //                   Its content is already in the eye targets;
        //                   drawing it would paint an empty surface.
        //  MonoPassThrough: the game's own offscreen business (shadow map
        //                   etc.) - stock behavior, textures keep content.
        //  FlatBothHalves:  tiny UI batch - both eyes, no eye shift.
        //  StereoBothEyes:  world geometry - full per-eye duplication.
        const DupDecision decision = DecideDuplication(
            StereoRenderer::Get().DivertedTextureBound(),
            StereoRenderer::Get().CurrentRTIsMain(),
            StereoRenderer::Get().ShadowActive(),
            cfg.sbsMonoUI, primCount, cfg.uiMaxTriangles);

        if (decision == DupDecision::Skip)
        {
            if (Capture::TraceActive())
                Capture::Trace("SKIP draw (diverted texture) primCount=%d", primCount);
            if (StereoRenderer::Get().RbLogActive())
                LOG_INFO("RB SKIP diverted pc=%d", primCount);
            return S_OK;
        }
        if (decision == DupDecision::MonoPassThrough)
        {
            if (Capture::TraceActive())
                Capture::Trace("MONO draw (offscreen RT) primCount=%d", primCount);
            if (StereoRenderer::Get().RbLogActive())
                LOG_INFO("RB MONO offscreen pc=%d", primCount);
            return draw();
        }
        if (decision == DupDecision::FlatBothHalves)
        {
            if (StereoRenderer::Get().RbLogActive())
                LOG_INFO("RB MONOUI pc=%d", primCount);
            return DrawMonoBothHalves(dev, draw);
        }

        StereoRenderer::Get().SetLogPrimCount(primCount);

        LARGE_INTEGER t0, t1;
        QueryPerformanceCounter(&t0);

        IDirect3DSurface9* savedRT = nullptr;
        IDirect3DSurface9* savedDS = nullptr;
        dev->GetRenderTarget(0, &savedRT);
        dev->GetDepthStencilSurface(&savedDS);

        HRESULT hrL, hrR;
        { InternalScope g; StereoRenderer::Get().ApplyEye(dev, 0); }
        hrL = draw();
        { InternalScope g; StereoRenderer::Get().ApplyEye(dev, 1); }
        hrR = draw();

        {
            InternalScope g;
            dev->SetRenderTarget(0, savedRT);
            dev->SetDepthStencilSurface(savedDS);
            StereoRenderer::Get().RestoreGameState(dev);
        }
        if (savedRT) savedRT->Release();
        if (savedDS) savedDS->Release();

        // Report the first failure; previously a failing left-eye draw was
        // silently masked by the right-eye result.
        HRESULT hr = FAILED(hrL) ? hrL : hrR;
        if (FAILED(hr))
        {
            static int loggedFails = 0;
            if (loggedFails < 10)
            {
                ++loggedFails;
                LOG_WARN("DrawBothEyes: eye draw failed hrL=0x%08X hrR=0x%08X", hrL, hrR);
            }
        }

        QueryPerformanceCounter(&t1);
        LARGE_INTEGER freq;
        QueryPerformanceFrequency(&freq);
        double us = static_cast<double>(t1.QuadPart - t0.QuadPart) * 1e6 / freq.QuadPart;
        StereoRenderer::Get().AddDuplicationTime(us, 2);
        return hr;
    }

    inline bool StereoOn()
    {
        return Config::Get().enableStereo && StereoRenderer::Get().IsActive();
    }
}

void ForceImmediatePresentForXR(D3DPRESENT_PARAMETERS* pp, const char* what)
{
    if (pp && Config::Get().outputMode == 1 && Config::Get().forceNoVsync &&
        pp->PresentationInterval != D3DPRESENT_INTERVAL_IMMEDIATE)
    {
        LOG_INFO("%s: forcing D3DPRESENT_INTERVAL_IMMEDIATE for XR pacing", what);
        pp->PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
    }
}

// ---------------------------------------------------------------------------
// Detours (invoked from DeviceProxy overrides; `dev` is the REAL device)
// ---------------------------------------------------------------------------
HRESULT __stdcall hkReset(IDirect3DDevice9* dev, D3DPRESENT_PARAMETERS* pp)
{
    if (DeviceHooks::Get().m_internal) return dev->Reset(pp);
    LOG_INFO("Reset(%ux%u)", pp ? pp->BackBufferWidth : 0, pp ? pp->BackBufferHeight : 0);
    ForceImmediatePresentForXR(pp, "Reset");
    StereoRenderer::Get().OnReset(dev, pp);       // release DEFAULT-pool stuff FIRST
    HRESULT hr = dev->Reset(pp);
    if (SUCCEEDED(hr)) LOG_INFO("Reset succeeded"); else LOG_ERROR("Reset failed hr=0x%08X", hr);
    return hr;
}

HRESULT __stdcall hkPresent(IDirect3DDevice9* dev, const RECT* s, const RECT* d, HWND w, const RGNDATA* r)
{
    if (DeviceHooks::Get().m_internal) return dev->Present(s, d, w, r);
    if (StereoOn())
    {
        InternalScope g;
        StereoRenderer::Get().OnPresent(dev);
    }
    return dev->Present(s, d, w, r);
}

HRESULT __stdcall hkSetRenderTarget(IDirect3DDevice9* dev, DWORD idx, IDirect3DSurface9* surf)
{
    if (DeviceHooks::Get().m_internal)
        return dev->SetRenderTarget(idx, surf);

    // Track-only: we need the current target's size to decide whether this
    // scene is main-frame (duplicate per eye) or the game's own offscreen
    // business (pass through untouched).
    if (idx == 0)
    {
        // M2 opt-in: the game's small offscreen RT passes (shadow maps and
        // similar) get per-eye target textures instead of staying mono. The
        // default remains OFF because pass semantics are engine-specific.
        if (StereoOn() && Config::Get().offscreenPasses)
        {
            if (StereoRenderer::Get().ActivateShadowPass(dev, surf))
                Capture::Trace("ShadowPass begin rt=%p", (void*)surf);
            else
                StereoRenderer::Get().EndShadowPass();
        }
        StereoRenderer::Get().SetCurrentRT(surf);
        Capture::Trace("SetRenderTarget(%u, %p)", idx, surf);
    }
    return dev->SetRenderTarget(idx, surf);
}

HRESULT __stdcall hkSetDepthStencilSurface(IDirect3DDevice9* dev, IDirect3DSurface9* ds)
{
    // Pure pass-through: eye draws bind their own per-eye depth; the game's
    // own scenes keep their own depth buffer.
    return dev->SetDepthStencilSurface(ds);
}

HRESULT __stdcall hkBeginScene(IDirect3DDevice9* dev)
{
    if (DeviceHooks::Get().m_internal) return dev->BeginScene();
    Capture::Trace("BeginScene");
    Capture::BeginFrame();
    HRESULT hr = dev->BeginScene();
    if (SUCCEEDED(hr))
    {
        InternalScope g;
        StereoRenderer::Get().OnBeginScene(dev);  // prepare per-eye targets
    }
    return hr;
}

HRESULT __stdcall hkEndScene(IDirect3DDevice9* dev)
{
    if (DeviceHooks::Get().m_internal) return dev->EndScene();
    Capture::Trace("EndScene");
    Capture::EndFrame();
    CameraScan::OnFrame();
    {
        InternalScope g;
        StereoRenderer::Get().OnEndScene(dev); // may draw the comfort overlay per eye
    }
    return dev->EndScene();
}

HRESULT __stdcall hkClear(IDirect3DDevice9* dev, DWORD count, const D3DRECT* rects,
                          DWORD flags, D3DCOLOR color, float z, DWORD stencil)
{
    if (DeviceHooks::Get().m_internal || !StereoOn())
        return dev->Clear(count, rects, flags, color, z, stencil);
    Capture::Trace("Clear(count=%u rects=%p flags=0x%X z=%.2f)", count, rects, flags, z);

    // The game clears its own target - pass through as-is. The eye targets
    // are cleared by StereoRenderer once per frame. (M2 shadow passes are
    // the one exception: their per-eye targets need per-eye clears.)
    if (StereoRenderer::Get().ShadowActive())
    {
        InternalScope g;
        return StereoRenderer::Get().ClearBothEyes(dev, count, rects, flags, color, z, stencil);
    }
    return dev->Clear(count, rects, flags, color, z, stencil);
}

HRESULT __stdcall hkSetTransform(IDirect3DDevice9* dev, D3DTRANSFORMSTATETYPE state, const D3DMATRIX* m)
{
    if (!DeviceHooks::Get().m_internal && m)
    {
        Capture::OnSetTransform(state, m); // M0 tooling (no-op when off)

        if (Config::Get().enableStereo && Config::Get().viewSource == 0)
        {
            if (state == D3DTS_VIEW)
                StereoRenderer::Get().CaptureViewMatrix(*m);
            else if (state == D3DTS_PROJECTION)
                StereoRenderer::Get().CaptureProjectionMatrix(*m);
        }
    }
    return dev->SetTransform(state, m);
}

HRESULT __stdcall hkSetViewport(IDirect3DDevice9* dev, const D3DVIEWPORT9* vp)
{
    // Track the game's intended viewport so ApplyEye derives correct per-eye
    // viewports even when the game changes it mid-frame.
    if (!DeviceHooks::Get().m_internal && vp)
    {
        StereoRenderer::Get().SetGameViewport(*vp);
        if (Capture::TraceActive())
            Capture::Trace("SetViewport(%u,%u %ux%u)", vp->X, vp->Y, vp->Width, vp->Height);
    }
    return dev->SetViewport(vp);
}

HRESULT __stdcall hkSetRenderState(IDirect3DDevice9* dev, D3DRENDERSTATETYPE state, DWORD value)
{
    if (!DeviceHooks::Get().m_internal && state == D3DRS_SCISSORTESTENABLE)
    {
        StereoRenderer::Get().SetScissorTestEnabled(value != 0);
        if (Capture::TraceActive())
            Capture::Trace("ScissorTestEnable=%u", value);
    }
    return dev->SetRenderState(state, value);
}

HRESULT __stdcall hkSetTexture(IDirect3DDevice9* dev, DWORD stage, IDirect3DBaseTexture9* tex)
{
    // Learns which sampler stages bind a shadowed surface so ApplyEye can
    // swap in the per-eye copy (cheap no-op when no shadows exist).
    if (!DeviceHooks::Get().m_internal && StereoOn())
        StereoRenderer::Get().OnGameSetTexture(stage, tex);
    return dev->SetTexture(stage, tex);
}

HRESULT __stdcall hkSetScissorRect(IDirect3DDevice9* dev, const RECT* rect)
{
    // Track the game's intended rect; ApplyEye re-applies it as-is on the
    // game-sized eye targets (no offset needed there).
    if (!DeviceHooks::Get().m_internal && rect)
    {
        StereoRenderer::Get().SetGameScissorRect(*rect);
        if (Capture::TraceActive())
            Capture::Trace("SetScissorRect(%ld,%ld,%ld,%ld)", rect->left, rect->top, rect->right, rect->bottom);
    }
    return dev->SetScissorRect(rect);
}

HRESULT __stdcall hkDrawPrimitive(IDirect3DDevice9* dev, D3DPRIMITIVETYPE type, UINT start, UINT count)
{
    if (DeviceHooks::Get().m_internal || !StereoOn())
        return dev->DrawPrimitive(type, start, count);
    if (Capture::TraceActive())
        Capture::Trace("DrawPrimitive(type=%d start=%u count=%u)", type, start, count);
    return DrawBothEyes(dev, static_cast<int>(count),
                        [&] { return dev->DrawPrimitive(type, start, count); });
}

HRESULT __stdcall hkDrawIndexedPrimitive(IDirect3DDevice9* dev, D3DPRIMITIVETYPE type, INT baseVertex,
                                         UINT minIndex, UINT numVertices, UINT startIndex, UINT count)
{
    if (DeviceHooks::Get().m_internal || !StereoOn())
        return dev->DrawIndexedPrimitive(type, baseVertex, minIndex, numVertices, startIndex, count);
    if (Capture::TraceActive())
        Capture::Trace("DrawIndexedPrimitive(type=%d numVerts=%u count=%u) rhw=%d", type, numVertices, count,
                       DeviceHooks::Get().CurrentDrawIsRHW() ? 1 : 0);
    return DrawBothEyes(dev, static_cast<int>(count), [&] {
        return dev->DrawIndexedPrimitive(type, baseVertex, minIndex, numVertices, startIndex, count); });
}

HRESULT __stdcall hkDrawPrimitiveUP(IDirect3DDevice9* dev, D3DPRIMITIVETYPE type, UINT count,
                                    const void* data, UINT stride)
{
    if (DeviceHooks::Get().m_internal || !StereoOn())
        return dev->DrawPrimitiveUP(type, count, data, stride);
    return DrawBothEyes(dev, static_cast<int>(count),
                        [&] { return dev->DrawPrimitiveUP(type, count, data, stride); });
}

HRESULT __stdcall hkDrawIndexedPrimitiveUP(IDirect3DDevice9* dev, D3DPRIMITIVETYPE type,
                                           UINT minVertex, UINT numVertices, UINT count,
                                           const void* indexData, D3DFORMAT indexFormat,
                                           const void* vertexData, UINT stride)
{
    if (DeviceHooks::Get().m_internal || !StereoOn())
        return dev->DrawIndexedPrimitiveUP(type, minVertex, numVertices, count, indexData, indexFormat, vertexData, stride);
    return DrawBothEyes(dev, static_cast<int>(count), [&] {
        return dev->DrawIndexedPrimitiveUP(type, minVertex, numVertices, count, indexData, indexFormat, vertexData, stride); });
}

HRESULT __stdcall hkSetVertexShaderConstantF(IDirect3DDevice9* dev, UINT reg, const float* data, UINT count)
{
    // M0/M2: when ViewSource=1, the captured view(/proj) matrix lives in a VS
    // register. Capture here like hkSetTransform does for fixed-function.
    if (!DeviceHooks::Get().m_internal)
    {
        Capture::OnSetVSConstantF(reg, data, count); // M0 tooling (no-op when off)

        if (Capture::TraceActive() && count >= 4 && data)
        {
            const float* f = data;
            Capture::Trace("VSCF c%u count=%u | %.3f %.3f %.3f %.3f", reg, count,
                           f[0], f[1], f[2], f[3]);
        }
        if (Config::Get().enableStereo && Config::Get().viewSource == 1 &&
            reg == static_cast<UINT>(Config::Get().vsViewRegister) && count >= 3 && data)
        {
            // UI passes often upload per-element matrices as 3x4 (count=3);
            // capture those too, with the affine fourth row implicit.
            D3DMATRIX m;
            if (count >= 4)
            {
                memcpy(&m, data, sizeof(D3DMATRIX));
            }
            else
            {
                memcpy(&m, data, 3 * 4 * sizeof(float));
                m._41 = 0.0f; m._42 = 0.0f; m._43 = 0.0f; m._44 = 1.0f;
            }
            StereoRenderer::Get().CaptureViewMatrix(m);
        }
    }
    return dev->SetVertexShaderConstantF(reg, data, count);
}

HRESULT __stdcall hkDrawRectPatch(IDirect3DDevice9* dev, UINT handle, const float* segs, const D3DRECTPATCH_INFO* info)
{
    // Patches are essentially unused by Warband; duplicate anyway for safety.
    // (No primitive count available - pass -1 so the UI heuristic never fires.)
    if (DeviceHooks::Get().m_internal || !StereoOn())
        return dev->DrawRectPatch(handle, segs, info);
    return DrawBothEyes(dev, -1, [&] { return dev->DrawRectPatch(handle, segs, info); });
}

// ---------------------------------------------------------------------------
// Device lifecycle
// ---------------------------------------------------------------------------
DeviceHooks& DeviceHooks::Get()
{
    static DeviceHooks s;
    return s;
}

void DeviceHooks::OnDeviceCreated(IDirect3DDevice9* device, const D3DPRESENT_PARAMETERS* pp)
{
    StereoRenderer::Get().OnDeviceCreated(device, pp);
}

void DeviceHooks::Shutdown()
{
    // Stateless by design; exists for symmetric teardown on DLL detach.
}

// ---------------------------------------------------------------------------
// Vertex-format tracking (diagnostics)
// ---------------------------------------------------------------------------

void DeviceHooks::TrackVertexDeclaration(IDirect3DVertexDeclaration9* decl)
{
    m_curIsRHW = false;
    if (!decl) return;
    D3DVERTEXELEMENT9 elems[32];
    UINT num = 0;
    if (FAILED(decl->GetDeclaration(elems, &num))) return;
    for (UINT i = 0; i < num && i < 32; ++i)
        if (elems[i].Usage == D3DDECLUSAGE_POSITIONT)
        {
            m_curIsRHW = true;
            static bool logged = false;
            if (!logged) { logged = true; LOG_INFO("DeviceHooks: RHW (pre-transformed) vertex declaration detected"); }
            break;
        }
}

void DeviceHooks::TrackFVF(DWORD fvf)
{
    m_curIsRHW = (fvf & D3DFVF_XYZRHW) == D3DFVF_XYZRHW;
    if (m_curIsRHW)
    {
        static bool logged = false;
        if (!logged) { logged = true; LOG_INFO("DeviceHooks: D3DFVF_XYZRHW (pre-transformed) FVF detected"); }
    }
}
